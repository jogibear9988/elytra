#include "platform/filesystem/Directory.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "platform/UV.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Mite {
namespace {
    struct Pool {
        GC_HOLDS_NO_CELL_EDGES;
        std::atomic<size_t> handles {};
        bool stopped {};
    };

    struct Entry {
        std::string name;
        int type {};
    };

    struct Directory {
        GC_HOLDS_NO_CELL_EDGES;

        Directory(std::shared_ptr<Pool> pool, std::string path, size_t batch)
            : pool(std::move(pool))
            , path(std::move(path))
            , batch(batch)
        {
            ++this->pool->handles;
        }

        ~Directory()
        {
            close();
            if (reserved)
                --pool->handles;
        }

        int close()
        {
            if (!native)
                return 0;
            uv_fs_t request {};
            auto status = uv_fs_closedir(nullptr, &request, native, nullptr);
            uv_fs_req_cleanup(&request);
            // closedir consumes the uv_dir_t allocation. Never retry through
            // an already freed native pointer, including native close errors.
            native = nullptr;
            entries.clear();
            offset = 0;
            if (reserved) {
                --pool->handles;
                reserved = false;
            }

            return status;
        }

        std::shared_ptr<Pool> pool;
        std::string path;
        size_t batch;
        uv_dir_t* native {};
        std::vector<Entry> entries;
        size_t offset {};
        bool busy {};
        bool reserved { true };
    };

    enum class Operation { Open, Read, Close };

    struct Request {
        GC_HOLDS_NO_CELL_EDGES;
        std::shared_ptr<Directory> directory;
        Operation op;
        int status {};
        std::optional<Entry> entry;

        void execute()
        {
            auto& dir = *directory;
            if (op == Operation::Close) {
                status = dir.close();
                return;
            }

            uv_fs_t request {};
            if (op == Operation::Open) {
                status = uv_fs_opendir(nullptr, &request, dir.path.c_str(), nullptr);
                if (!status)
                    dir.native = static_cast<uv_dir_t*>(request.ptr);
                uv_fs_req_cleanup(&request);
                return;
            }

            if (dir.offset == dir.entries.size()) {
                dir.entries.clear();
                dir.offset = 0;
                std::vector<uv_dirent_t> entries(dir.batch);
                dir.native->dirents = entries.data();
                dir.native->nentries = entries.size();
                status = uv_fs_readdir(nullptr, &request, dir.native, nullptr);
                if (status >= 0) {
                    for (int i = 0; i < status; ++i)
                        dir.entries.push_back({ entries[static_cast<size_t>(i)].name,
                            static_cast<int>(entries[static_cast<size_t>(i)].type) });
                    status = 0;
                }
                // libuv owns each name only until request cleanup.
                uv_fs_req_cleanup(&request);
                dir.native->dirents = nullptr;
                dir.native->nentries = 0;
            }

            if (!status && dir.offset < dir.entries.size())
                entry = dir.entries[dir.offset++];
        }
    };

    static JS::Value text(JS::VM& vm, std::string const& value)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(value.data(), value.size())));
    }

    static JS::Completion failure(JS::VM& vm, StringView code, Utf16View message)
    {
        auto error = JS::Error::create(vm.realm(), message);
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }

    static JS::Value native_error(JS::VM& vm, Request const& request)
    {
        auto error = JS::Error::create(vm.realm(),
            Utf16String::from_utf8(
                StringView(uv_strerror(request.status), strlen(uv_strerror(request.status)))));
        error->define_direct_property(
            "code"_utf16, text(vm, uv_err_name(request.status)), JS::default_attributes);
        error->define_direct_property(
            "errno"_utf16, JS::Value(request.status), JS::default_attributes);
        error->define_direct_property(
            "path"_utf16, text(vm, request.directory->path), JS::default_attributes);
        error->define_direct_property("syscall"_utf16,
            text(vm,
                request.op == Operation::Open       ? "opendir"
                    : request.op == Operation::Read ? "readdir"
                                                    : "closedir"),
            JS::default_attributes);
        return error;
    }

    static GC::Ref<JS::Object> make_handle(JS::Realm&, EventLoop&, std::shared_ptr<Directory>);

    static JS::ThrowCompletionOr<JS::Value> result(
        JS::Realm& realm, EventLoop& loop, Request const& request)
    {
        if (request.status < 0)
            return JS::throw_completion(native_error(realm.vm(), request));
        if (request.op == Operation::Open)
            return make_handle(realm, loop, request.directory);
        if (request.op == Operation::Close)
            return JS::js_undefined();
        if (!request.entry)
            return JS::js_null();
        GC::Root entry { JS::Object::create(realm, nullptr) };
        GC::Root bytes { TRY(JS::Uint8Array::create(realm, request.entry->name.size())) };
        bytes->viewed_array_buffer()->overwrite(
            0, request.entry->name.data(), request.entry->name.size());
        entry->define_direct_property("name"_utf16, bytes.ptr(), JS::default_attributes);
        entry->define_direct_property(
            "type"_utf16, JS::Value(request.entry->type), JS::default_attributes);
        return JS::Value(entry.ptr());
    }

    static JS::ThrowCompletionOr<JS::Value> run(JS::Realm& realm, EventLoop& loop,
        std::shared_ptr<Directory> directory, Operation op, JS::Value callback)
    {
        auto& vm = realm.vm();
        if (directory->pool->stopped)
            return failure(vm, "ERR_MITE_FS_CLOSED"sv, "Directory host has stopped"_utf16);
        if (directory->busy)
            return failure(
                vm, "ERR_DIR_CONCURRENT_OPERATION"sv, "Directory operation is pending"_utf16);
        if (op != Operation::Open && !directory->native)
            return failure(vm, "ERR_DIR_CLOSED"sv, "Directory handle was closed"_utf16);
        if (!callback.is_undefined() && !callback.is_function())
            return vm.throw_completion<JS::TypeError>("Invalid directory callback"_utf16);
        auto request = std::make_shared<Request>(Request { directory, op, 0, {} });
        if (callback.is_undefined()) {
            request->execute();
            return result(realm, loop, *request);
        }

        GC::Root complete { GC::create_function(vm.heap(),
            GC::captures(realm, native_reference(loop), shared_capture(request), callback),
            [](auto& realm, auto& owner, auto& request,
                auto& callback) -> JS::ThrowCompletionOr<JS::Value> {
                request->directory->busy = false;
                auto value = result(realm, owner.get(), *request);
                JS::Value args[] { value.is_error() ? value.error_value() : JS::js_null(),
                    value.is_error() ? JS::js_undefined() : value.value() };
                return JS::call(
                    realm.vm(), callback, JS::js_undefined(), ReadonlySpan<JS::Value> { args, 2 });
            }) };
        directory->busy = true;
        auto queued = loop.queue_work([request] { request->execute(); }, *complete);
        if (queued.is_error()) {
            directory->busy = false;
            return queued.release_error();
        }

        return JS::js_undefined();
    }

    static GC::Ref<JS::Object> make_handle(
        JS::Realm& realm, EventLoop& loop, std::shared_ptr<Directory> directory)
    {
        GC::Root object { JS::Object::create(realm, nullptr) };
        object->define_native_function(realm, "read"_utf16,
            GC::create_function(realm.vm().heap(),
                GC::captures(realm, native_reference(loop), shared_capture(directory)),
                [](auto& realm, auto& owner, auto& directory, JS::VM& vm) {
                    return run(
                        realm, owner.get(), directory.value, Operation::Read, vm.argument(0));
                }),
            1, JS::default_attributes);
        object->define_native_function(realm, "close"_utf16,
            GC::create_function(realm.vm().heap(),
                GC::captures(realm, native_reference(loop), shared_capture(directory)),
                [](auto& realm, auto& owner, auto& directory, JS::VM& vm) {
                    return run(
                        realm, owner.get(), directory.value, Operation::Close, vm.argument(0));
                }),
            1, JS::default_attributes);
        return *object;
    }
}

void install_directory_host(JS::Realm& realm, JS::Object& host, EventLoop& loop)
{
    auto pool = std::make_shared<Pool>();
    loop.on_shutdown([pool] { pool->stopped = true; });
    host.define_native_function(realm, "openDirectory"_utf16,
        GC::create_function(realm.vm().heap(),
            GC::captures(realm, native_reference(loop), shared_capture(pool)),
            [](auto& realm, auto& owner, auto& pool,
                JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto value = vm.argument(0);
                std::string path;
                if (value.is_string()) {
                    auto encoded
                        = value.as_string().utf16_string_view().to_utf8(AllowLonelySurrogates::No);
                    if (encoded.is_error())
                        return vm.throw_completion<JS::TypeError>(
                            "Invalid directory path Unicode"_utf16);
                    auto bytes = encoded.release_value();
                    path.assign(bytes.bytes_as_string_view().characters_without_null_termination(),
                        bytes.byte_count());
                } else if (value.is_object()
                    && JS::Uint8Array::is_engine_class_of(value.as_object())) {
                    auto& array = static_cast<JS::Uint8Array&>(value.as_object());
                    auto witness = JS::make_typed_array_with_buffer_witness_record(
                        array, JS::ArrayBuffer::Order::SeqCst);
                    if (JS::is_typed_array_out_of_bounds(witness))
                        return vm.throw_completion<JS::TypeError>("Detached directory path"_utf16);
                    auto size = JS::typed_array_byte_length(witness);
                    if (size)
                        path.assign(reinterpret_cast<char const*>(
                                        array.viewed_array_buffer()->data_at(array.byte_offset())),
                            size);
                } else
                    return vm.throw_completion<JS::TypeError>("Invalid directory path"_utf16);
                if (path.find('\0') != std::string::npos)
                    return vm.throw_completion<JS::TypeError>("Invalid directory path NUL"_utf16);
                if (!vm.argument(1).is_int32() || vm.argument(1).as_i32() < 1
                    || vm.argument(1).as_i32() > 4096)
                    return vm.throw_completion<JS::RangeError>(
                        "Directory batch must be 1..4096"_utf16);
                if (pool->handles >= 256)
                    return failure(vm, "ENOSPC"sv, "Directory handle budget exhausted"_utf16);
                auto directory = std::make_shared<Directory>(
                    pool.value, std::move(path), static_cast<size_t>(vm.argument(1).as_i32()));
                return run(
                    realm, owner.get(), std::move(directory), Operation::Open, vm.argument(2));
            }),
        3, JS::default_attributes);
}
}
