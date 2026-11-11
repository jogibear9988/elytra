/**
 * Implements filesystem hosts and descriptor operations through libuv. Asynchronous calls
 * snapshot native inputs and root only the owner-thread completion; descriptors close through
 * explicit lifecycle rules.
 */

#include "platform/filesystem/FileSystem.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "modules/ModulePaths.h"
#include "platform/UV.h"
#include "platform/filesystem/EventWatcher.h"
#include "platform/filesystem/StatSnapshot.h"
#include "platform/filesystem/StatWatcher.h"
#include <LibGC/CellAllocator.h>
#include <LibGC/Function.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace Mite {
// The private JS release function traces this owner. No worker ever accesses it;
// pending FileHandle operations retain their JS receiver until completion.
class FileOwner final : public GC::Cell {
    GC_CELL(FileOwner, GC::Cell);
    GC_DECLARE_ALLOCATOR(FileOwner);

public:
    explicit FileOwner(int fd)
        : m_fd(fd)
    {
    }

    ~FileOwner() override
    {
        close();
    }

    int release()
    {
        int fd = static_cast<int>(m_fd);
        m_fd = -1;
        return fd;
    }

    void finalize() override
    {
        close();
        Base::finalize();
    }

private:
    void close()
    {
        if (m_fd < 0)
            return;
        uv_fs_t request {};
        uv_fs_close(nullptr, &request, release(), nullptr);
        uv_fs_req_cleanup(&request);
    }

    int64_t m_fd; // Also satisfies the GC allocator minimum cell payload size.
};

GC_DEFINE_ALLOCATOR(FileOwner);

namespace {
    using namespace ModulePaths;

    struct NativeRequest {
        GC_HOLDS_NO_CELL_EDGES;

        std::string operation, path, dest;
        std::vector<unsigned char> data;
        int flags {}, mode {};
        int descriptor { -1 };
        int64_t position { -1 };
        size_t length {};
        int number {};
        bool recursive {}, with_file_types {}, force {};
        int error {};
        std::string syscall, error_path, created, error_code;
        uv_stat_t stat {};

        struct Entry {
            std::string name;
            uv_dirent_type_t type;
        };

        std::vector<Entry> entries;
    };

    struct FsRequest {
        uv_fs_t value {};

        ~FsRequest()
        {
            uv_fs_req_cleanup(&value);
        }
    };

    struct NativeFile {
        int descriptor;

        ~NativeFile()
        {
            if (descriptor >= 0) {
                FsRequest close;
                uv_fs_close(nullptr, &close.value, descriptor, nullptr);
            }
        }
    };

    static int record(
        NativeRequest& request, int result, char const* syscall, std::string const& path)
    {
        if (result < 0) {
            request.error = result;
            request.syscall = syscall;
            request.error_path = path;
        }

        return result;
    }

    static bool mkdir_recursive(NativeRequest& request, std::filesystem::path const& path)
    {
        auto name = path_string(path);
        FsRequest call;
        int result = uv_fs_mkdir(nullptr, &call.value, name.c_str(), request.mode, nullptr);
        if (!result) {
            if (request.created.empty())
                request.created = name;
            return true;
        }

        if (result == UV_ENOENT) {
            auto parent = path.parent_path();
            if (!parent.empty() && parent != path && mkdir_recursive(request, parent)) {
                FsRequest retry;
                result = uv_fs_mkdir(nullptr, &retry.value, name.c_str(), request.mode, nullptr);
                if (!result) {
                    if (request.created.empty())
                        request.created = name;
                    return true;
                }
            } else if (request.error)
                return false;
        }

        if (result == UV_EEXIST) {
            FsRequest stat;
            if (!uv_fs_stat(nullptr, &stat.value, name.c_str(), nullptr)
                && (stat.value.statbuf.st_mode & S_IFMT) == S_IFDIR)
                return true;
        }

        record(request, result, "mkdir", name);
        return false;
    }

    static void execute(NativeRequest& request)
    {
        const auto& op = request.operation;
        auto const* path = request.path.c_str();
        if (op == "open" || op == "close" || op == "read" || op == "write" || op == "fstat"
            || op == "ftruncate" || op == "fsync" || op == "fdatasync") {
            FsRequest call;
            int result = 0;
            if (op == "open")
                result
                    = uv_fs_open(nullptr, &call.value, path, request.flags, request.mode, nullptr);
            else if (op == "close")
                result = uv_fs_close(nullptr, &call.value, request.descriptor, nullptr);
            else if (op == "fstat") {
                result = uv_fs_fstat(nullptr, &call.value, request.descriptor, nullptr);
                if (!result)
                    request.stat = call.value.statbuf;
            } else if (op == "ftruncate")
                result = uv_fs_ftruncate(
                    nullptr, &call.value, request.descriptor, request.position, nullptr);
            else if (op == "fsync")
                result = uv_fs_fsync(nullptr, &call.value, request.descriptor, nullptr);
            else if (op == "fdatasync")
                result = uv_fs_fdatasync(nullptr, &call.value, request.descriptor, nullptr);
            else if (op == "read") {
                request.data.resize(request.length);
                if (request.length) {
                    auto buffer = uv_buf_init(reinterpret_cast<char*>(request.data.data()),
                        static_cast<unsigned int>(request.length));
                    result = uv_fs_read(nullptr, &call.value, request.descriptor, &buffer, 1,
                        request.position, nullptr);
                }

                request.data.resize(result > 0 ? static_cast<size_t>(result) : 0);
            } else {
                auto buffer = uv_buf_init(reinterpret_cast<char*>(request.data.data()),
                    static_cast<unsigned int>(request.data.size()));
                result = uv_fs_write(nullptr, &call.value, request.descriptor, &buffer, 1,
                    request.position, nullptr);
            }

            request.number = result;
            record(request, result, op.c_str(), request.path);
            return;
        }

        if (op == "readFile" || op == "writeFile") {
            FsRequest open;
            int descriptor = request.descriptor;
            if (descriptor < 0)
                descriptor
                    = uv_fs_open(nullptr, &open.value, path, request.flags, request.mode, nullptr);
            if (record(request, descriptor, "open", request.path) < 0)
                return;
            NativeFile file { request.descriptor < 0 ? descriptor : -1 };
            if (op == "readFile") {
                std::vector<unsigned char> chunk(65536);
                for (;;) {
                    FsRequest read;
                    auto buffer = uv_buf_init(reinterpret_cast<char*>(chunk.data()),
                        static_cast<unsigned int>(chunk.size()));
                    int count
                        = uv_fs_read(nullptr, &read.value, descriptor, &buffer, 1, -1, nullptr);
                    if (record(request, count, "read", "") <= 0)
                        break;
                    if (request.data.size() + static_cast<size_t>(count) > 0x7fffffff) {
                        record(request, UV_EFBIG, "read", "");
                        break;
                    }

                    request.data.insert(request.data.end(), chunk.begin(), chunk.begin() + count);
                }
            } else {
                size_t offset = 0;
                while (offset < request.data.size()) {
                    FsRequest write;
                    auto length = static_cast<unsigned int>(
                        std::min<size_t>(65536, request.data.size() - offset));
                    auto buffer = uv_buf_init(
                        reinterpret_cast<char*>(request.data.data() + offset), length);
                    int count
                        = uv_fs_write(nullptr, &write.value, descriptor, &buffer, 1, -1, nullptr);
                    if (record(request, count ? count : UV_EIO, "write", "") < 0)
                        break;
                    offset += static_cast<size_t>(count);
                }
            }

            if (request.descriptor >= 0)
                return;
            FsRequest close;
            int result = uv_fs_close(nullptr, &close.value, descriptor, nullptr);
            file.descriptor = -1;
            if (!request.error)
                record(request, result, "close", "");
            return;
        }

        if (op == "rm") {
            // lstat deliberately does not follow a final symlink. Recursive
            // deletion is not admitted by the public facade in this slice.
            FsRequest stat;
            auto status = uv_fs_lstat(nullptr, &stat.value, path, nullptr);
            if (status == UV_ENOENT && request.force)
                return;
            if (record(request, status, "lstat", request.path) < 0)
                return;
            if ((stat.value.statbuf.st_mode & S_IFMT) == S_IFDIR) {
                record(request, UV_EISDIR, "rm", request.path);
                request.error_code = "ERR_FS_EISDIR";
                return;
            }

            FsRequest unlink;
            status = uv_fs_unlink(nullptr, &unlink.value, path, nullptr);
            if (status != UV_ENOENT || !request.force)
                record(request, status, "unlink", request.path);
            return;
        }

        if (op == "mkdir" && request.recursive) {
            auto bytes = reinterpret_cast<char8_t const*>(request.path.data());
            mkdir_recursive(
                request, std::filesystem::path(std::u8string(bytes, request.path.size())));
            return;
        }

        FsRequest call;
        int result = UV_ENOSYS;
        if (op == "access")
            result = uv_fs_access(nullptr, &call.value, path, request.mode, nullptr);
        else if (op == "realpath") {
            result = uv_fs_realpath(nullptr, &call.value, path, nullptr);
            // Copy canonical path bytes before uv_fs_req_cleanup releases ptr.
            if (!result) {
                auto* resolved = static_cast<unsigned char const*>(call.value.ptr);
                request.data.assign(
                    resolved, resolved + strlen(reinterpret_cast<char const*>(resolved)));
            }
        } else if (op == "stat" || op == "lstat") {
            result = op == "stat" ? uv_fs_stat(nullptr, &call.value, path, nullptr)
                                  : uv_fs_lstat(nullptr, &call.value, path, nullptr);
            if (!result)
                request.stat = call.value.statbuf;
        } else if (op == "unlink")
            result = uv_fs_unlink(nullptr, &call.value, path, nullptr);
        else if (op == "rename")
            result = uv_fs_rename(nullptr, &call.value, path, request.dest.c_str(), nullptr);
        else if (op == "copyfile")
            result = uv_fs_copyfile(
                nullptr, &call.value, path, request.dest.c_str(), request.flags, nullptr);
        else if (op == "mkdir")
            result = uv_fs_mkdir(nullptr, &call.value, path, request.mode, nullptr);
        else if (op == "chmod")
            result = uv_fs_chmod(nullptr, &call.value, path, request.mode, nullptr);
        else if (op == "mkdtemp") {
            // libuv/OS atomically creates one fresh directory; no exists-then-create race.
            auto pattern = request.path + "XXXXXX";
            result = uv_fs_mkdtemp(nullptr, &call.value, pattern.c_str(), nullptr);
            if (!result)
                request.created = call.value.path;
        } else if (op == "rmdir")
            result = uv_fs_rmdir(nullptr, &call.value, path, nullptr);
        else if (op == "readdir") {
            result = uv_fs_scandir(nullptr, &call.value, path, 0, nullptr);
            if (result >= 0) {
                uv_dirent_t entry {};
                while (true) {
                    int next = uv_fs_scandir_next(&call.value, &entry);
                    if (next == UV_EOF)
                        break;
                    if (next < 0) {
                        result = next;
                        break;
                    }

                    auto type = entry.type;
                    if (type == UV_DIRENT_UNKNOWN && request.with_file_types) {
                        FsRequest stat;
                        auto child = request.path + "/" + entry.name;
                        if (record(request,
                                uv_fs_lstat(nullptr, &stat.value, child.c_str(), nullptr), "lstat",
                                child)
                            < 0)
                            return;
                        switch (stat.value.statbuf.st_mode & S_IFMT) {
                        case S_IFREG:
                            type = UV_DIRENT_FILE;
                            break;
                        case S_IFDIR:
                            type = UV_DIRENT_DIR;
                            break;
                        case S_IFLNK:
                            type = UV_DIRENT_LINK;
                            break;
                        case S_IFIFO:
                            type = UV_DIRENT_FIFO;
                            break;
#ifdef S_IFSOCK
                        case S_IFSOCK:
                            type = UV_DIRENT_SOCKET;
                            break;
#endif
                        case S_IFCHR:
                            type = UV_DIRENT_CHAR;
                            break;
#ifdef S_IFBLK
                        case S_IFBLK:
                            type = UV_DIRENT_BLOCK;
                            break;
#endif
                        }
                    }

                    request.entries.push_back({ entry.name, type });
                }
            }
        }

        record(request, result, op == "readdir" ? "scandir" : op.c_str(), request.path);
    }

    static GC::Ref<JS::PrimitiveString> text(JS::VM& vm, std::string const& value)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8_with_replacement_character(view(value)));
    }

    static JS::ThrowCompletionOr<JS::Value> byte_array(
        JS::Realm& realm, unsigned char const* data, size_t size)
    {
        auto array = TRY(JS::Uint8Array::create(realm, size));
        if (size)
            array->viewed_array_buffer()->overwrite(0, data, size);
        return JS::Value(array);
    }

    static GC::Ref<JS::Object> make_error(JS::VM& vm, NativeRequest const& request)
    {
        auto display_path = [](std::string const& path) {
#ifdef _WIN32
            if (path.starts_with("\\\\?\\UNC\\"))
                return std::string("\\\\") + path.substr(8);
            if (path.starts_with("\\\\?\\"))
                return path.substr(4);
#endif
            return path;
        };

        auto error = JS::Error::create(vm.realm(),
            Utf16String::from_utf8(
                StringView(uv_strerror(request.error), strlen(uv_strerror(request.error)))));
        error->define_direct_property(
            "errno"_utf16, JS::Value(request.error), JS::default_attributes);
        error->define_direct_property("code"_utf16,
            text(vm, request.error_code.empty() ? uv_err_name(request.error) : request.error_code),
            JS::default_attributes);
        error->define_direct_property(
            "syscall"_utf16, text(vm, request.syscall), JS::default_attributes);
        if (request.descriptor < 0 && request.syscall != "read" && request.syscall != "write"
            && request.syscall != "close")
            error->define_direct_property(
                "path"_utf16, text(vm, display_path(request.error_path)), JS::default_attributes);
        if (request.operation == "rename" || request.operation == "copyfile")
            error->define_direct_property(
                "dest"_utf16, text(vm, display_path(request.dest)), JS::default_attributes);
        return error;
    }

    static JS::ThrowCompletionOr<JS::Value> result(
        JS::VM& vm, JS::Realm& realm, NativeRequest const& request)
    {
        if (request.error)
            return JS::throw_completion(make_error(vm, request));
        if (request.operation == "open" || request.operation == "write")
            return JS::Value(request.number);
        if (request.operation == "read")
            return byte_array(realm, request.data.data(), request.data.size());
        if (request.operation == "readFile" || request.operation == "realpath")
            return byte_array(realm, request.data.data(), request.data.size());
        if (request.operation == "mkdtemp")
            return byte_array(realm, reinterpret_cast<unsigned char const*>(request.created.data()),
                request.created.size());
        if (request.operation == "mkdir" && !request.created.empty())
            return JS::Value(text(vm, request.created));
        if (request.operation == "stat" || request.operation == "lstat"
            || request.operation == "fstat") {
            return JS::Value(make_stat_snapshot(realm, request.stat));
        }

        if (request.operation == "readdir") {
            GC::RootVector<JS::Value> entries;
            for (auto const& entry : request.entries) {
                GC::Root object { JS::Object::create(realm, nullptr) };
                object->define_direct_property("name"_utf16,
                    TRY(byte_array(realm, reinterpret_cast<unsigned char const*>(entry.name.data()),
                        entry.name.size())),
                    JS::default_attributes);
                object->define_direct_property(
                    "type"_utf16, JS::Value(static_cast<int>(entry.type)), JS::default_attributes);
                entries.append(object);
            }

            return JS::Value(JS::Array::create_from(realm, entries));
        }

        return JS::js_undefined();
    }

    static JS::ThrowCompletionOr<std::string> path_argument(JS::VM& vm, JS::Value value)
    {
        if (value.is_string())
            return string(MUST(value.as_string().utf16_string_view().to_byte_string()));
        if (value.is_object() && JS::Uint8Array::is_engine_class_of(value.as_object())) {
            auto& array = static_cast<JS::Uint8Array&>(value.as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return vm.throw_completion<JS::TypeError>("Detached filesystem argument"_utf16);
            auto count = JS::typed_array_byte_length(witness);
            if (!count)
                return std::string {};
            auto data = array.viewed_array_buffer()->data_at(array.byte_offset());
            return std::string(reinterpret_cast<char const*>(data), count);
        }

        return vm.throw_completion<JS::TypeError>("Invalid private filesystem path"_utf16);
    }
}

GC::Ref<JS::Object> create_filesystem_host(JS::VM&, JS::Realm& realm, EventLoop& loop)
{
    auto host = JS::Object::create(realm, nullptr);
    auto constants = JS::Object::create(realm, nullptr);
#define CONSTANT(name)                                                                             \
    constants->define_direct_property(#name##_utf16, JS::Value(name), JS::Attribute::Enumerable)
    CONSTANT(O_RDONLY);
    CONSTANT(O_WRONLY);
    CONSTANT(O_RDWR);
    CONSTANT(O_CREAT);
    CONSTANT(O_EXCL);
    CONSTANT(O_TRUNC);
    CONSTANT(O_APPEND);
    constants->define_direct_property(
        "COPYFILE_EXCL"_utf16, JS::Value(UV_FS_COPYFILE_EXCL), JS::Attribute::Enumerable);
    constants->define_direct_property(
        "COPYFILE_FICLONE"_utf16, JS::Value(UV_FS_COPYFILE_FICLONE), JS::Attribute::Enumerable);
    constants->define_direct_property("COPYFILE_FICLONE_FORCE"_utf16,
        JS::Value(UV_FS_COPYFILE_FICLONE_FORCE), JS::Attribute::Enumerable);
    constants->define_direct_property(
        "O_SYNC"_utf16, JS::Value(UV_FS_O_SYNC), JS::Attribute::Enumerable);
    CONSTANT(F_OK);
    CONSTANT(R_OK);
    CONSTANT(W_OK);
    CONSTANT(X_OK);
    CONSTANT(S_IFMT);
    CONSTANT(S_IFREG);
    CONSTANT(S_IFDIR);
    CONSTANT(S_IFCHR);
    CONSTANT(S_IFIFO);
    CONSTANT(S_IFLNK);
#ifdef S_IFBLK
    CONSTANT(S_IFBLK);
#endif
#ifdef S_IFSOCK
    CONSTANT(S_IFSOCK);
#endif
#undef CONSTANT
    host->define_direct_property("constants"_utf16, constants, JS::default_attributes);
    host->define_native_function(realm, "own"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(realm),
            [](auto& realm, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_int32() || vm.argument(0).as_i32() < 0)
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid private filesystem descriptor"_utf16);
                auto owner = vm.heap().allocate<FileOwner>(vm.argument(0).as_i32());
                return JS::NativeFunction::create(realm,
                    GC::create_function(realm.vm().heap(), GC::captures(owner),
                        [](auto& owner, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                            return JS::Value(owner->release());
                        }),
                    0, "releaseFile"_utf16);
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "run"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(realm, native_reference(loop)),
            [](auto& realm, auto& captured_loop, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& loop = captured_loop.get();

                auto spec = TRY(vm.argument(0).to_object(vm));
                auto request = std::make_shared<NativeRequest>();
                auto operation = TRY(TRY(spec->get("op"_utf16)).to_utf16_string(vm));
                request->operation = string(MUST(operation.utf16_view().to_byte_string()));
                auto path = TRY(spec->get("path"_utf16));
                if (path.is_number())
                    request->descriptor = TRY(path.to_i32(vm));
                else
                    request->path = TRY(path_argument(vm, path));
                if (request->path.find('\0') != std::string::npos)
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid private filesystem path"_utf16);
                if (request->operation == "rename" || request->operation == "copyfile") {
                    request->dest = TRY(path_argument(vm, TRY(spec->get("dest"_utf16))));
                    if (request->dest.find('\0') != std::string::npos)
                        return vm.throw_completion<JS::TypeError>(
                            "Invalid private filesystem destination"_utf16);
                }

                auto position = TRY(spec->get("position"_utf16));
                if (position.is_string()) {
                    auto encoded = MUST(position.as_string().utf16_string_view().to_byte_string());
                    auto parsed = std::from_chars(encoded.characters(),
                        encoded.characters() + encoded.length(), request->position);
                    if (parsed.ec != std::errc {}
                        || parsed.ptr != encoded.characters() + encoded.length())
                        return vm.throw_completion<JS::RangeError>(
                            "Invalid private filesystem position"_utf16);
                }

                auto length = TRY(spec->get("length"_utf16));
                if (!length.is_undefined())
                    request->length = TRY(length.to_u32(vm));
                request->flags = TRY(TRY(spec->get("flags"_utf16)).to_i32(vm));
                request->mode = TRY(TRY(spec->get("mode"_utf16)).to_i32(vm));
                if (request->operation == "copyfile"
                    && (request->flags < 0
                        || (request->flags
                            & ~(UV_FS_COPYFILE_EXCL | UV_FS_COPYFILE_FICLONE
                                | UV_FS_COPYFILE_FICLONE_FORCE))))
                    return vm.throw_completion<JS::RangeError>(
                        "Invalid private copyfile flags"_utf16);
                request->recursive = TRY(spec->get("recursive"_utf16)).to_boolean();
                request->force = TRY(spec->get("force"_utf16)).to_boolean();
                request->with_file_types = TRY(spec->get("withFileTypes"_utf16)).to_boolean();
                if (request->operation == "writeFile" || request->operation == "write") {
                    auto data = TRY(path_argument(vm, TRY(spec->get("data"_utf16))));
                    request->data.assign(data.begin(), data.end());
                }

                auto callback = vm.argument(1);
                if (callback.is_undefined()) {
                    execute(*request);
                    return result(vm, realm, *request);
                }

                auto complete = GC::create_function(vm.heap(),
                    GC::captures(realm, shared_capture(request), callback),
                    [](auto& realm, auto& request,
                        auto& callback) -> JS::ThrowCompletionOr<JS::Value> {
                        auto& vm = realm.vm();

                        auto value = result(vm, realm, *request);
                        JS::Value args[] = { value.is_error() ? value.error_value() : JS::js_null(),
                            value.is_error() ? JS::js_undefined() : value.value() };
                        return JS::call(
                            vm, callback, JS::js_undefined(), ReadonlySpan<JS::Value> { args, 2 });
                    });
                TRY(loop.queue_work([request] { execute(*request); }, complete));
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    install_stat_watcher_host(realm, *host, loop);
    install_event_watcher_host(realm, *host, loop);
    return host;
}
}
