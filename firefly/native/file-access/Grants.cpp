#include "Grants.h"

#include <AK/ByteString.h>
#include <AK/CharacterTypes.h>
#include <AK/Function.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/LexicalPath.h>
#include <AK/NumericLimits.h>
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <AK/Time.h>
#include <AK/Utf8View.h>
#include <LibCore/System.h>
#include <errno.h>

#ifndef AK_OS_WINDOWS
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Firefly {

struct FileAccessGrants::Cursor {
    AK_ALLOC_WITH_KMALLOC;

public:
    Document owner;
    String directory_token;
#ifndef AK_OS_WINDOWS
    DIR* stream;

    ~Cursor()
    {
        closedir(stream);
    }
#endif
};

struct FileAccessGrants::SaveTarget {
    AK_ALLOC_WITH_KMALLOC;

public:
    NonnullOwnPtr<Core::File> parent;
    ByteString name;
};

struct FileAccessGrants::PrivateBucket : RefCounted<PrivateBucket> {
    explicit PrivateBucket(NonnullOwnPtr<Core::File> directory)
        : directory(move(directory))
    {
    }

    ErrorOr<void> check_growth(u64 bytes) const;

    NonnullOwnPtr<Core::File> directory;
};

struct FileAccessGrants::Writer {
    AK_ALLOC_WITH_KMALLOC;

public:
    Document owner;
    String entry_token;
    NonnullOwnPtr<Core::File> parent;
    NonnullOwnPtr<Core::File> file;
    ByteString temporary_name;
    RefPtr<PrivateBucket> private_bucket;
    OwnPtr<Core::File> locked_original;

    ~Writer()
    {
#ifndef AK_OS_WINDOWS
        if (!temporary_name.is_empty())
            (void)unlinkat(parent->fd(), temporary_name.characters(), 0);
#endif
    }
};

struct FileAccessGrants::SyncAccess {
    AK_ALLOC_WITH_KMALLOC;

public:
    Document owner;
    String file_token;
    NonnullOwnPtr<Core::File> file;
    NonnullRefPtr<PrivateBucket> bucket;
    bool writable;
};

FileAccessGrants::FileAccessGrants() = default;
FileAccessGrants::~FileAccessGrants() = default;

bool FileAccessGrants::supported()
{
#ifdef AK_OS_WINDOWS
    return false;
#else
    return true;
#endif
}

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::remember(Document owner, StringView name,
    Kind kind, NonnullOwnPtr<Core::File> file, bool writable, RefPtr<PrivateBucket> bucket,
    Optional<Vector<String>> private_path)
{
    auto display_name = TRY(String::from_utf8(name));
    auto token = new_token();
    TRY(m_entries.try_set(token,
        Entry { owner, kind, move(file), writable, false, display_name, move(bucket), private_path,
            {} }));
    return Handle { move(token), move(display_name), kind, move(private_path), {} };
}

ErrorOr<FileAccessGrants::Entry const*> FileAccessGrants::lookup(
    Document owner, String const& token, bool require_read) const
{
    auto it = m_entries.find(token);
    if (it == m_entries.end() || it->value.owner != owner || (require_read && !it->value.readable))
        return Error::from_errno(EACCES);
    return &it->value;
}

ErrorOr<FileAccessGrants::Entry const*> FileAccessGrants::lookup(
    Document owner, String const& token, Kind kind) const
{
    auto entry = TRY(lookup(owner, token));
    if (entry->kind != kind)
        return Error::from_errno(kind == Kind::Directory ? ENOTDIR : EISDIR);
    return entry;
}

#ifndef AK_OS_WINDOWS
static ErrorOr<NonnullOwnPtr<Core::File>> open_entry(
    int parent, StringView name, FileAccessGrants::Kind kind)
{
    // Refuse special files before opening and validate again on the actual
    // descriptor. O_NOFOLLOW prevents a final-component symlink substitution.
    auto path = ByteString { name };
    struct stat metadata {};
    if (fstatat(parent, path.characters(), &metadata, AT_SYMLINK_NOFOLLOW) < 0)
        return Error::from_errno(errno);
    auto matches_kind = [&](mode_t mode) {
        return kind == FileAccessGrants::Kind::Directory ? S_ISDIR(mode) : S_ISREG(mode);
    };

    if (!matches_kind(metadata.st_mode))
        return Error::from_errno(kind == FileAccessGrants::Kind::Directory ? ENOTDIR : EINVAL);

    auto directory_flag = kind == FileAccessGrants::Kind::Directory ? O_DIRECTORY : 0;
    auto fd = openat(
        parent, path.characters(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | directory_flag);
    if (fd < 0)
        return Error::from_errno(errno);
    ArmedScopeGuard close_on_error = [&] { (void)Core::System::close(fd); };
    auto opened = TRY(Core::System::fstat(fd));
    if (!matches_kind(opened.st_mode) || metadata.st_dev != opened.st_dev
        || metadata.st_ino != opened.st_ino)
        return Error::from_errno(EAGAIN);

    auto file = TRY(Core::File::adopt_fd(fd, Core::File::OpenMode::Read));
    close_on_error.disarm();
    return file;
}
#endif

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::admit(
    Document owner, StringView path, Kind kind, bool writable)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)path;
    (void)kind;
    (void)writable;
    return Error::from_errno(ENOTSUP);
#else
    if (!owner.namespace_id || !owner.local_id || !path.starts_with('/') || path.length() > 4096
        || path.contains('\0'))
        return Error::from_errno(EINVAL);
    if (m_entries.size() >= max_grants)
        return Error::from_errno(ENOSPC);

    while (path.length() > 1 && path.ends_with('/'))
        path = path.substring_view(0, path.length() - 1);
    auto file = TRY(open_entry(AT_FDCWD, path, kind));
    TRY(check_public_directory(file->fd()));
    return remember(
        owner, LexicalPath::basename(path), kind, move(file), writable && kind == Kind::Directory);
#endif
}

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::admit_save(Document owner, StringView path)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)path;
    return Error::from_errno(ENOTSUP);
#else
    if (!owner.namespace_id || !owner.local_id || !path.starts_with('/') || path.ends_with('/')
        || path.length() > 4096 || path.contains('\0'))
        return Error::from_errno(EINVAL);
    if (m_entries.size() >= max_grants)
        return Error::from_errno(ENOSPC);

    auto separator = path.find_last('/').value();
    auto name = path.substring_view(separator + 1);
    if (name == "."sv || name == ".."sv || name.length() > 255 || !Utf8View { name }.validate())
        return Error::from_errno(EINVAL);
    auto parent_path = path.substring_view(0, max(separator, size_t { 1 }));
    auto parent = TRY(open_entry(AT_FDCWD, parent_path, Kind::Directory));
    TRY(check_public_directory(parent->fd()));
    return admit_saved_file(owner, name, move(parent), true);
#endif
}

#ifndef AK_OS_WINDOWS
ErrorOr<FileAccessGrants::Handle> FileAccessGrants::admit_saved_file(Document owner,
    StringView name, NonnullOwnPtr<Core::File> parent, bool create, RefPtr<PrivateBucket> bucket,
    Optional<Vector<String>> private_path)
{
    auto target = TRY(adopt_nonnull_own_or_enomem(
        new (nothrow) SaveTarget { move(parent), ByteString { name } }));

    auto file = open_entry(target->parent->fd(), name, Kind::File);
    if (create && file.is_error() && file.error().is_errno() && file.error().code() == ENOENT) {
        if (bucket)
            TRY(bucket->check_growth(0));
        auto fd = openat(target->parent->fd(), target->name.characters(),
            O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST)
            return Error::from_errno(errno);
        if (fd < 0) {
            file = open_entry(target->parent->fd(), name, Kind::File);
        } else {
            ArmedScopeGuard close_on_error = [&] { (void)Core::System::close(fd); };
            file = Core::File::adopt_fd(fd, Core::File::OpenMode::ReadWrite);
            if (!file.is_error())
                close_on_error.disarm();
        }
    }

    if (file.is_error())
        return file.release_error();

    auto handle = TRY(remember(
        owner, name, Kind::File, file.release_value(), true, move(bucket), move(private_path)));
    auto stored = m_save_targets.try_set(handle.token, move(target));
    if (stored.is_error()) {
        release(owner, handle.token);
        return stored.release_error();
    }

    return handle;
}
#endif

ErrorOr<FileAccessGrants::Writer*> FileAccessGrants::lookup_writer(
    Document owner, String const& token)
{
    auto it = m_writers.find(token);
    if (it == m_writers.end() || it->value->owner != owner)
        return Error::from_errno(EACCES);
    return it->value.ptr();
}

ErrorOr<String> FileAccessGrants::begin_write(
    Document owner, String const& token, bool keep_existing_data)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)keep_existing_data;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token, Kind::File));
    auto target = m_save_targets.find(token);
    if (target == m_save_targets.end())
        return Error::from_errno(EACCES);
    if (m_writers.size() >= max_cursors)
        return Error::from_errno(ENOSPC);
    for (auto const& writer : m_writers) {
        if (writer.value->entry_token == token)
            return Error::from_errno(EBUSY);
    }

    auto parent = TRY(open_entry(target->value->parent->fd(), "."sv, Kind::Directory));
    if (faccessat(parent->fd(), target->value->name.characters(), W_OK, AT_EACCESS) < 0)
        return Error::from_errno(errno);

    OwnPtr<Core::File> locked_original;
    if (entry->private_bucket) {
        locked_original = TRY(open_entry(parent->fd(), target->value->name.view(), Kind::File));
        if (flock(locked_original->fd(), LOCK_SH | LOCK_NB) < 0)
            return Error::from_errno(errno == EWOULDBLOCK ? ENOLCK : errno);
    }

    auto writer_token = new_token();
    auto name = TRY(String::formatted(".elytra-file-{}", writer_token)).to_byte_string();
    auto fd = openat(
        parent->fd(), name.characters(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return Error::from_errno(errno);

    // Until the Writer is registered, the stack owns the descriptor and temporary name.
    ArmedScopeGuard unlink_on_error = [&] { (void)unlinkat(parent->fd(), name.characters(), 0); };
    ArmedScopeGuard close_on_error = [&] { (void)Core::System::close(fd); };
    auto file = TRY(Core::File::adopt_fd(fd, Core::File::OpenMode::ReadWrite));
    close_on_error.disarm();
    // Bound initial copying on the browser IPC thread.
    if (keep_existing_data) {
        constexpr u64 max_initial_copy = 16 * 1024 * 1024;
        auto original = TRY(Core::System::fstat(entry->descriptor->fd()));
        auto size = static_cast<u64>(original.st_size);
        if (size > max_initial_copy)
            return Error::from_errno(E2BIG);
        if (entry->private_bucket)
            TRY(entry->private_bucket->check_growth(size));
        for (u64 offset = 0; offset < size;) {
            auto bytes = TRY(read(owner, token, offset, min<u64>(max_read_size, size - offset)));
            if (bytes.is_empty())
                break;
            TRY(file->write_until_depleted(bytes.bytes()));
            offset += bytes.size();
        }
    }

    auto* writer = new (nothrow) Writer { owner, token, move(parent), move(file), move(name),
        entry->private_bucket, move(locked_original) };
    if (!writer)
        return Error::from_errno(ENOMEM);
    unlink_on_error.disarm();
    TRY(m_writers.try_set(writer_token, adopt_own(*writer)));
    return writer_token;
#endif
}

ErrorOr<void> FileAccessGrants::write(
    Document owner, String const& token, u64 offset, ReadonlyBytes bytes)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)offset;
    (void)bytes;
    return Error::from_errno(ENOTSUP);
#else
    auto writer = TRY(lookup_writer(owner, token));
    if (bytes.size() > max_read_size
        || offset > static_cast<u64>(NumericLimits<off_t>::max()) - bytes.size())
        return Error::from_errno(EINVAL);

    if (writer->private_bucket && !bytes.is_empty()) {
        auto size = static_cast<u64>(TRY(Core::System::fstat(writer->file->fd())).st_size);
        auto end = offset + bytes.size();
        if (end > size)
            TRY(writer->private_bucket->check_growth(end - size));
    }

    // Once a staged write fails, a later close must not publish a partial chunk.
    ArmedScopeGuard discard_on_error = [&] { m_writers.remove(token); };
    size_t written = 0;
    while (written < bytes.size()) {
        auto amount = pwrite(writer->file->fd(), bytes.data() + written, bytes.size() - written,
            static_cast<off_t>(offset + written));
        if (amount < 0 && errno == EINTR)
            continue;
        if (amount <= 0)
            return Error::from_errno(amount < 0 ? errno : EIO);
        written += static_cast<size_t>(amount);
    }

    discard_on_error.disarm();
    return {};
#endif
}

ErrorOr<void> FileAccessGrants::truncate(Document owner, String const& token, u64 size)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)size;
    return Error::from_errno(ENOTSUP);
#else
    auto writer = TRY(lookup_writer(owner, token));
    if (size > static_cast<u64>(NumericLimits<off_t>::max()))
        return Error::from_errno(EINVAL);
    if (writer->private_bucket) {
        auto current = static_cast<u64>(TRY(Core::System::fstat(writer->file->fd())).st_size);
        if (size > current)
            TRY(writer->private_bucket->check_growth(size - current));
    }

    if (ftruncate(writer->file->fd(), static_cast<off_t>(size)) < 0) {
        auto error = Error::from_errno(errno);
        m_writers.remove(token);
        return error;
    }

    return {};
#endif
}

ErrorOr<void> FileAccessGrants::finish_write(Document owner, String const& token, bool commit)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)commit;
    return Error::from_errno(ENOTSUP);
#else
    auto writer = TRY(lookup_writer(owner, token));
    ArmedScopeGuard retire = [&] { m_writers.remove(token); };
    if (!commit)
        return {};

    auto entry = m_entries.find(writer->entry_token);
    auto target = m_save_targets.find(writer->entry_token);
    VERIFY(entry != m_entries.end() && target != m_save_targets.end());
    auto parent = writer->parent->fd();
    auto* name = target->value->name.characters();
    auto original = TRY(Core::System::fstat(entry->value.descriptor->fd()));
    if (fchmod(writer->file->fd(), original.st_mode & 0777) < 0 || fsync(writer->file->fd()) < 0)
        return Error::from_errno(errno);

    auto same_entry = [&](int fd, char const* basename) -> ErrorOr<void> {
        auto opened = TRY(Core::System::fstat(fd));
        struct stat current {};
        if (fstatat(parent, basename, &current, AT_SYMLINK_NOFOLLOW) < 0)
            return Error::from_errno(errno);
        if (opened.st_dev != current.st_dev || opened.st_ino != current.st_ino)
            return Error::from_errno(EAGAIN);
        return {};
    };

    TRY(same_entry(entry->value.descriptor->fd(), name));
    TRY(same_entry(writer->file->fd(), writer->temporary_name.characters()));
    if (renameat(parent, writer->temporary_name.characters(), parent, name) < 0)
        return Error::from_errno(errno);

    writer->temporary_name = {};
    entry->value.descriptor = move(writer->file);
    return {};
#endif
}

#ifndef AK_OS_WINDOWS
static bool is_entry_name(StringView name)
{
    return !name.is_empty() && name != "."sv && name != ".."sv && name.length() <= 255
        && !name.contains('/') && !name.contains('\\') && !name.contains('\0')
        && !name.starts_with(".elytra-file-"sv);
}

static bool identical(struct stat const& left, struct stat const& right)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

static ErrorOr<String> descriptor_path(int fd)
{
    char path[PATH_MAX] {};
#if defined(AK_OS_MACOS)
    if (fcntl(fd, F_GETPATH, path) < 0)
        return Error::from_errno(errno);
    return String::from_utf8(StringView { path, strlen(path) });
#elif defined(AK_OS_LINUX)
    auto link = TRY(String::formatted("/proc/self/fd/{}", fd)).to_byte_string();
    auto length = readlink(link.characters(), path, sizeof(path));
    if (length < 0 || static_cast<size_t>(length) == sizeof(path))
        return Error::from_errno(length < 0 ? errno : ENAMETOOLONG);
    return String::from_utf8(StringView { path, static_cast<size_t>(length) });
#else
    return Error::from_errno(ENOTSUP);
#endif
}

static ErrorOr<void> remove_entry(int parent, char const* name, bool recursive, dev_t volume,
    size_t remaining_depth, size_t& remaining_entries,
    Function<ErrorOr<void>(int)> const& can_enter, bool honor_file_locks)
{
    if (!remaining_depth || !remaining_entries)
        return Error::from_errno(E2BIG);
    --remaining_entries;
    struct stat metadata {};
    if (fstatat(parent, name, &metadata, AT_SYMLINK_NOFOLLOW) < 0) {
        if (errno == ENOENT)
            return {};
        return Error::from_errno(errno);
    }

    if (metadata.st_dev != volume)
        return Error::from_errno(EXDEV);

    bool directory = S_ISDIR(metadata.st_mode);
    if (directory) {
        auto file = TRY(open_entry(
            parent, StringView { name, strlen(name) }, FileAccessGrants::Kind::Directory));
        if (!identical(metadata, TRY(Core::System::fstat(file->fd()))))
            return Error::from_errno(EAGAIN);
        TRY(can_enter(file->fd()));
        if (!recursive) {
            if (unlinkat(parent, name, AT_REMOVEDIR) < 0 && errno != ENOENT)
                return Error::from_errno(errno);
            return {};
        }

        auto* stream = fdopendir(file->fd());
        if (!stream)
            return Error::from_errno(errno);
        (void)file->leak_fd();
        ScopeGuard close = [&] { closedir(stream); };
        for (;;) {
            errno = 0;
            auto* entry = readdir(stream);
            if (!entry) {
                if (errno)
                    return Error::from_errno(errno);
                break;
            }

            StringView child(entry->d_name, strlen(entry->d_name));
            if (child.is_one_of("."sv, ".."sv))
                continue;
            if (child.starts_with(".elytra-file-"sv))
                return Error::from_errno(EBUSY);
            TRY(remove_entry(dirfd(stream), entry->d_name, true, volume, remaining_depth - 1,
                remaining_entries, can_enter, honor_file_locks));
        }

        struct stat current {};
        if (fstatat(parent, name, &current, AT_SYMLINK_NOFOLLOW) < 0)
            return Error::from_errno(errno);
        if (!identical(metadata, current))
            return Error::from_errno(EAGAIN);
    }

    OwnPtr<Core::File> removal_lock;
    if (honor_file_locks && S_ISREG(metadata.st_mode)) {
        removal_lock = TRY(
            open_entry(parent, StringView { name, strlen(name) }, FileAccessGrants::Kind::File));
        if (!identical(metadata, TRY(Core::System::fstat(removal_lock->fd()))))
            return Error::from_errno(EAGAIN);
        if (flock(removal_lock->fd(), LOCK_EX | LOCK_NB) < 0)
            return Error::from_errno(errno == EWOULDBLOCK ? ENOLCK : errno);
    }

    if (unlinkat(parent, name, directory ? AT_REMOVEDIR : 0) < 0 && errno != ENOENT)
        return Error::from_errno(errno);
    return {};
}
#endif

ErrorOr<FileAccessGrants::SyncAccess*> FileAccessGrants::lookup_sync(
    Document owner, String const& token, bool writable)
{
    auto writer = m_sync_access.find(token);
    if (writer == m_sync_access.end() || writer->value->owner != owner)
        return Error::from_errno(EBADF);
    if (writable && !writer->value->writable)
        return Error::from_errno(EROFS);
    return writer->value.ptr();
}

ErrorOr<String> FileAccessGrants::begin_sync(
    Document owner, String const& token, bool writable, bool exclusive)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)writable;
    (void)exclusive;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token, Kind::File));
    auto target = m_save_targets.find(token);
    if (!entry->private_bucket)
        return Error::from_errno(EBADF);
    if (target == m_save_targets.end())
        return Error::from_errno(EACCES);
    if (m_sync_access.size() >= max_cursors)
        return Error::from_errno(ENOSPC);

    auto fd = openat(target->value->parent->fd(), target->value->name.characters(),
        (writable ? O_RDWR : O_RDONLY) | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return Error::from_errno(errno);
    ArmedScopeGuard close = [&] { (void)Core::System::close(fd); };
    if (!identical(TRY(Core::System::fstat(fd)), TRY(Core::System::fstat(entry->descriptor->fd()))))
        return Error::from_errno(EAGAIN);
    if (flock(fd, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) < 0)
        return Error::from_errno(errno == EWOULDBLOCK ? ENOLCK : errno);

    auto file = TRY(Core::File::adopt_fd(
        fd, writable ? Core::File::OpenMode::ReadWrite : Core::File::OpenMode::Read));
    close.disarm();
    auto session = TRY(adopt_nonnull_own_or_enomem(new (nothrow) SyncAccess {
        owner,
        token,
        move(file),
        *entry->private_bucket,
        writable,
    }));
    auto result = new_token();
    TRY(m_sync_access.try_set(result, move(session)));
    return result;
#endif
}

ErrorOr<ByteBuffer> FileAccessGrants::sync_read(
    Document owner, String const& token, u64 offset, size_t count)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)offset;
    (void)count;
    return Error::from_errno(ENOTSUP);
#else
    auto handle = TRY(lookup_sync(owner, token));
    if (count > max_read_size || offset > static_cast<u64>(NumericLimits<off_t>::max()) - count)
        return Error::from_errno(EINVAL);
    auto bytes = TRY(ByteBuffer::create_uninitialized(count));
    ssize_t amount;
    do {
        amount = pread(handle->file->fd(), bytes.data(), count, static_cast<off_t>(offset));
    } while (amount < 0 && errno == EINTR);
    if (amount < 0)
        return Error::from_errno(errno);
    bytes.resize(static_cast<size_t>(amount));
    return bytes;
#endif
}

ErrorOr<void> FileAccessGrants::sync_write(
    Document owner, String const& token, u64 offset, ReadonlyBytes bytes)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)offset;
    (void)bytes;
    return Error::from_errno(ENOTSUP);
#else
    auto handle = TRY(lookup_sync(owner, token, true));
    if (bytes.size() > max_read_size
        || offset > static_cast<u64>(NumericLimits<off_t>::max()) - bytes.size())
        return Error::from_errno(EINVAL);
    auto length = static_cast<u64>(TRY(Core::System::fstat(handle->file->fd())).st_size);
    if (!bytes.is_empty() && offset + bytes.size() > length)
        TRY(handle->bucket->check_growth(offset + bytes.size() - length));
    for (size_t written = 0; written < bytes.size();) {
        auto count = pwrite(handle->file->fd(), bytes.data() + written, bytes.size() - written,
            static_cast<off_t>(offset + written));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return Error::from_errno(count < 0 ? errno : EIO);
        written += static_cast<size_t>(count);
    }

    return {};
#endif
}

ErrorOr<void> FileAccessGrants::sync_truncate(Document owner, String const& token, u64 size)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)size;
    return Error::from_errno(ENOTSUP);
#else
    auto handle = TRY(lookup_sync(owner, token, true));
    if (size > static_cast<u64>(NumericLimits<off_t>::max()))
        return Error::from_errno(EINVAL);
    auto length = static_cast<u64>(TRY(Core::System::fstat(handle->file->fd())).st_size);
    if (size > length)
        TRY(handle->bucket->check_growth(size - length));
    if (ftruncate(handle->file->fd(), static_cast<off_t>(size)) < 0)
        return Error::from_errno(errno);
    return {};
#endif
}

ErrorOr<u64> FileAccessGrants::sync_size(Document owner, String const& token)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    return Error::from_errno(ENOTSUP);
#else
    auto handle = TRY(lookup_sync(owner, token));
    return static_cast<u64>(TRY(Core::System::fstat(handle->file->fd())).st_size);
#endif
}

ErrorOr<void> FileAccessGrants::sync_flush(Document owner, String const& token)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    return Error::from_errno(ENOTSUP);
#else
    auto handle = TRY(lookup_sync(owner, token, true));
    if (fsync(handle->file->fd()) < 0)
        return Error::from_errno(errno);
    return {};
#endif
}

void FileAccessGrants::close_sync(Document owner, String const& token)
{
    auto handle = m_sync_access.find(token);
    if (handle != m_sync_access.end() && handle->value->owner == owner)
        m_sync_access.remove(handle);
}

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::child(
    Document owner, String const& token, StringView name, Kind kind, bool create)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)name;
    (void)kind;
    (void)create;
    return Error::from_errno(ENOTSUP);
#else
    auto parent = TRY(lookup(owner, token, Kind::Directory));
    if (!is_entry_name(name) || !Utf8View { name }.validate())
        return Error::from_errno(EINVAL);
    if (create && !parent->writable)
        return Error::from_errno(EACCES);
    if (m_entries.size() >= max_grants)
        return Error::from_errno(ENOSPC);

    auto fd = parent->descriptor->fd();
    auto bucket = parent->private_bucket;
    auto private_path = parent->private_path;
    if (private_path.has_value()) {
        if (private_path->size() >= 64)
            return Error::from_errno(E2BIG);
        TRY(private_path->try_append(TRY(String::from_utf8(name))));
    }

    auto native_reference = parent->native_reference;
    if (native_reference.has_value()) {
        if (native_reference->components.size() >= 64)
            return Error::from_errno(E2BIG);
        TRY(native_reference->components.try_append(TRY(String::from_utf8(name))));
    }

    auto attach_reference = [&](Handle handle) {
        m_entries.find(handle.token)->value.native_reference = native_reference;
        handle.native_reference = move(native_reference);
        return handle;
    };

    if (!bucket)
        TRY(check_public_directory(fd));
    auto writable = parent->writable;
    if (writable && kind == Kind::File)
        return attach_reference(
            TRY(admit_saved_file(owner, name, TRY(open_entry(fd, "."sv, Kind::Directory)), create,
                move(bucket), move(private_path))));

    if (create) {
        auto basename = ByteString { name };
        struct stat existing {};
        if (fstatat(fd, basename.characters(), &existing, AT_SYMLINK_NOFOLLOW) < 0) {
            if (errno != ENOENT)
                return Error::from_errno(errno);
            if (bucket)
                TRY(bucket->check_growth(0));
            if (mkdirat(fd, basename.characters(), 0700) < 0 && errno != EEXIST)
                return Error::from_errno(errno);
        }
    }

    auto file = TRY(open_entry(fd, name, kind));
    if (!bucket)
        TRY(check_public_directory(file->fd()));
    return attach_reference(
        TRY(remember(owner, name, kind, move(file), writable, move(bucket), move(private_path))));
#endif
}

ErrorOr<void> FileAccessGrants::remove(
    Document owner, String const& token, StringView name, bool recursive)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)name;
    (void)recursive;
    return Error::from_errno(ENOTSUP);
#else
    auto directory = TRY(lookup(owner, token, Kind::Directory));
    if (!is_entry_name(name) || !Utf8View { name }.validate())
        return Error::from_errno(EINVAL);
    if (!directory->writable)
        return Error::from_errno(EACCES);

    auto fd = directory->descriptor->fd();
    auto volume = TRY(Core::System::fstat(fd)).st_dev;
    auto basename = ByteString { name };
    size_t remaining_entries = 4096;
    Function<ErrorOr<void>(int)> can_enter = [&](int directory_fd) -> ErrorOr<void> {
        if (directory->private_bucket)
            return {};
        return check_public_directory(directory_fd);
    };

    TRY(can_enter(fd));
    return remove_entry(fd, basename.characters(), recursive, volume, 64, remaining_entries,
        can_enter, directory->private_bucket != nullptr);
#endif
}

ErrorOr<bool> FileAccessGrants::is_same_entry(
    Document owner, String const& left, String const& right) const
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)left;
    (void)right;
    return Error::from_errno(ENOTSUP);
#else
    auto a = TRY(lookup(owner, left, false));
    auto b = TRY(lookup(owner, right, false));
    if (static_cast<bool>(a->private_bucket) != static_cast<bool>(b->private_bucket))
        return false;
    return identical(TRY(Core::System::fstat(a->descriptor->fd())),
        TRY(Core::System::fstat(b->descriptor->fd())));
#endif
}

ErrorOr<Optional<Vector<String>>> FileAccessGrants::resolve(
    Document owner, String const& token, String const& descendant_token) const
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)descendant_token;
    return Error::from_errno(ENOTSUP);
#else
    auto directory = TRY(lookup(owner, token, Kind::Directory));
    auto descendant = TRY(lookup(owner, descendant_token));
    if (static_cast<bool>(directory->private_bucket)
        != static_cast<bool>(descendant->private_bucket))
        return Optional<Vector<String>> {};
    auto identity = TRY(Core::System::fstat(descendant->descriptor->fd()));
    if (identical(TRY(Core::System::fstat(directory->descriptor->fd())), identity))
        return Optional<Vector<String>> { Vector<String> {} };

    auto root = TRY(descriptor_path(directory->descriptor->fd()));
    auto path = TRY(descriptor_path(descendant->descriptor->fd()));
    auto prefix = TRY(String::formatted("{}{}", root, root.ends_with('/') ? ""sv : "/"sv));
    if (!path.starts_with_bytes(prefix.bytes_as_string_view()))
        return Optional<Vector<String>> {};

    // Treat the OS path as a hint. Only return components that can be walked from the authorized
    // directory descriptor and still identify the actual descendant descriptor.
    auto parts = path.bytes_as_string_view().substring_view(prefix.byte_count()).split_view('/');
    if (parts.is_empty() || parts.size() > 64)
        return Optional<Vector<String>> {};

    Vector<String> relative;
    auto current = TRY(open_entry(directory->descriptor->fd(), "."sv, Kind::Directory));
    for (size_t index = 0; index < parts.size(); ++index) {
        auto name = parts[index];
        if (!is_entry_name(name))
            return Optional<Vector<String>> {};
        auto kind = index + 1 == parts.size() ? descendant->kind : Kind::Directory;
        auto opened = open_entry(current->fd(), name, kind);
        if (opened.is_error())
            return Optional<Vector<String>> {};
        current = opened.release_value();
        TRY(relative.try_append(TRY(String::from_utf8(name))));
    }

    if (!identical(TRY(Core::System::fstat(current->fd())), identity))
        return Optional<Vector<String>> {};
    return Optional<Vector<String>> { move(relative) };
#endif
}

#ifndef AK_OS_WINDOWS
static ErrorOr<void> measure_bucket(
    int fd, dev_t volume, size_t depth, size_t& allowance, u64& bytes)
{
    if (!depth)
        return Error::from_errno(E2BIG);
    auto directory = TRY(open_entry(fd, "."sv, FileAccessGrants::Kind::Directory));
    auto* stream = fdopendir(directory->fd());
    if (!stream)
        return Error::from_errno(errno);
    (void)directory->leak_fd();
    ScopeGuard close = [&] { closedir(stream); };
    for (;;) {
        errno = 0;
        auto* entry = readdir(stream);
        if (!entry) {
            if (errno)
                return Error::from_errno(errno);
            return {};
        }

        StringView name(entry->d_name, strlen(entry->d_name));
        if (name.is_one_of("."sv, ".."sv))
            continue;
        if (!allowance)
            return Error::from_errno(E2BIG);
        --allowance;
        struct stat metadata {};
        if (fstatat(dirfd(stream), entry->d_name, &metadata, AT_SYMLINK_NOFOLLOW) < 0) {
            if (errno == ENOENT)
                continue;
            return Error::from_errno(errno);
        }

        if (metadata.st_dev != volume)
            return Error::from_errno(EXDEV);
        if (S_ISDIR(metadata.st_mode)) {
            auto child = open_entry(dirfd(stream), name, FileAccessGrants::Kind::Directory);
            if (child.is_error())
                return child.release_error();
            if (!identical(metadata, TRY(Core::System::fstat(child.value()->fd()))))
                return Error::from_errno(EAGAIN);
            TRY(measure_bucket(child.value()->fd(), volume, depth - 1, allowance, bytes));
        } else if (S_ISREG(metadata.st_mode)) {
            auto size = static_cast<u64>(metadata.st_size);
            if (size > bytes)
                return Error::from_errno(ENOSPC);
            bytes -= size;
        }
    }
}

ErrorOr<void> FileAccessGrants::PrivateBucket::check_growth(u64 growth) const
{
    constexpr u64 quota = 64 * 1024 * 1024;
    if (growth > quota)
        return Error::from_errno(ENOSPC);
    auto bytes = quota - growth;
    size_t entries = 4095;
    auto volume = TRY(Core::System::fstat(directory->fd())).st_dev;
    return measure_bucket(directory->fd(), volume, 64, entries, bytes);
}

static ErrorOr<NonnullOwnPtr<Core::File>> open_private_directory(int parent, StringView name)
{
    auto basename = ByteString { name };
    if (mkdirat(parent, basename.characters(), 0700) < 0 && errno != EEXIST)
        return Error::from_errno(errno);

    auto directory = TRY(open_entry(parent, name, FileAccessGrants::Kind::Directory));
    auto status = TRY(Core::System::fstat(directory->fd()));
    if (status.st_uid != geteuid() || (status.st_mode & 0077))
        return Error::from_errno(EACCES);
    return directory;
}
#endif

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::admit_private(
    Document owner, StringView profile_data, StringView bucket_key)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)profile_data;
    (void)bucket_key;
    return Error::from_errno(ENOTSUP);
#else
    if (!is_entry_name(bucket_key) || m_entries.size() >= max_grants)
        return Error::from_errno(ENOSPC);

    auto profile = TRY(open_entry(AT_FDCWD, profile_data, Kind::Directory));
    auto private_area = TRY(open_private_directory(profile->fd(), "ElytraOriginFiles"sv));
    auto bucket_directory = TRY(open_private_directory(private_area->fd(), bucket_key));
    auto duplicate = TRY(open_entry(bucket_directory->fd(), "."sv, Kind::Directory));
    auto bucket = TRY(try_make_ref_counted<PrivateBucket>(move(duplicate)));
    return remember(owner, ""sv, Kind::Directory, move(bucket_directory), true, move(bucket),
        Vector<String> {});
#endif
}

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::reopen_private(Document owner,
    StringView profile_data, StringView bucket_key, Vector<String> const& components, Kind kind)
{
    if (components.size() > 64 || (components.is_empty() && kind != Kind::Directory))
        return Error::from_errno(EINVAL);

    // A serialized handle can only be rebound inside the authenticated origin's
    // bucket. Each component uses the same checked descriptor-relative lookup.
    auto current = TRY(admit_private(owner, profile_data, bucket_key));
    ArmedScopeGuard release_current = [&] { release(owner, current.token); };
    for (size_t i = 0; i < components.size(); ++i) {
        auto next = TRY(child(owner, current.token, components[i].bytes_as_string_view(),
            i + 1 == components.size() ? kind : Kind::Directory));
        release(owner, current.token);
        current = move(next);
    }

    release_current.disarm();
    return current;
}

#ifndef AK_OS_WINDOWS
static ErrorOr<NonnullOwnPtr<Core::File>> native_reference_directory(
    StringView profile_data, StringView origin_key)
{
    if (!is_entry_name(origin_key))
        return Error::from_errno(EINVAL);
    auto profile = TRY(open_entry(AT_FDCWD, profile_data, FileAccessGrants::Kind::Directory));
    auto area = TRY(open_private_directory(profile->fd(), "ElytraOriginFiles"sv));
    auto references = TRY(open_private_directory(area->fd(), ".native-references"sv));
    return open_private_directory(references->fd(), origin_key);
}

static bool valid_reference_ticket(StringView value)
{
    if (value.length() != 36)
        return false;
    for (size_t i = 0; i < value.length(); ++i) {
        auto c = value[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return false;
        } else if (!is_ascii_hex_digit(c)) {
            return false;
        }
    }

    return true;
}

enum class RememberedAccess : u8 {
    None,
    Read,
    Write,
};

static ErrorOr<bool> permission_marker(int directory, StringView name)
{
    struct stat metadata {};
    auto basename = ByteString { name };
    if (fstatat(directory, basename.characters(), &metadata, AT_SYMLINK_NOFOLLOW) < 0) {
        if (errno == ENOENT)
            return false;
        return Error::from_errno(errno);
    }

    if (!S_ISREG(metadata.st_mode) || metadata.st_uid != geteuid() || (metadata.st_mode & 0077)
        || metadata.st_size != 0)
        return Error::from_errno(EACCES);
    return true;
}

static ErrorOr<RememberedAccess> remembered_access(
    int origin_directory, FileAccessGrants::NativeReference const& reference)
{
    using Kind = FileAccessGrants::Kind;
    auto permissions = open_entry(origin_directory, ".permissions"sv, Kind::Directory);
    if (permissions.is_error()) {
        if (permissions.error().is_errno() && permissions.error().code() == ENOENT)
            return RememberedAccess::None;
        return permissions.release_error();
    }

    auto current = open_entry(
        permissions.value()->fd(), reference.ticket.bytes_as_string_view(), Kind::Directory);
    auto access = RememberedAccess::None;
    for (size_t depth = 0;; ++depth) {
        if (current.is_error()) {
            if (current.error().is_errno() && current.error().code() == ENOENT)
                return access;
            return current.release_error();
        }

        auto fd = current.value()->fd();
        if (TRY(permission_marker(fd, ".elytra-file-write"sv)))
            return RememberedAccess::Write;
        if (TRY(permission_marker(fd, ".elytra-file-read"sv)))
            access = RememberedAccess::Read;
        if (depth == reference.components.size())
            return access;
        current
            = open_entry(fd, reference.components[depth].bytes_as_string_view(), Kind::Directory);
    }
}
#endif

ErrorOr<void> FileAccessGrants::issue_native_reference(
    Document owner, Handle& handle, StringView profile_data, StringView origin_key)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)handle;
    (void)profile_data;
    (void)origin_key;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, handle.token));
    if (entry->private_bucket || entry->native_reference.has_value())
        return Error::from_errno(EINVAL);
    auto path = TRY(descriptor_path(entry->descriptor->fd()));
    TRY(check_public_directory(entry->descriptor->fd()));
    auto anchor = entry->descriptor->fd();
    OwnPtr<Core::File> parent;
    if (entry->kind == Kind::File) {
        auto dirname = LexicalPath::dirname(path.to_byte_string());
        parent = TRY(open_entry(AT_FDCWD, dirname.view(), Kind::Directory));
        auto reopened
            = TRY(open_entry(parent->fd(), entry->name.bytes_as_string_view(), Kind::File));
        if (!identical(TRY(Core::System::fstat(reopened->fd())), TRY(Core::System::fstat(anchor))))
            return Error::from_errno(EAGAIN);
        anchor = parent->fd();
    }

    auto identity = TRY(Core::System::fstat(anchor));
    auto directory = TRY(native_reference_directory(profile_data, origin_key));
    auto ticket = AK::generate_random_uuid();
    auto filename = ticket.to_byte_string();
    auto fd = openat(directory->fd(), filename.characters(),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return Error::from_errno(errno);
    ArmedScopeGuard remove_on_failure
        = [&] { (void)unlinkat(directory->fd(), filename.characters(), 0); };
    ArmedScopeGuard close_on_failure = [&] { (void)Core::System::close(fd); };
    auto record = TRY(Core::File::adopt_fd(fd, Core::File::OpenMode::Write));
    close_on_failure.disarm();

    JsonObject metadata;
    metadata.set("version"sv, 1u);
    metadata.set("path"sv, path);
    metadata.set("kind"sv, entry->kind == Kind::File ? "file"sv : "directory"sv);
    metadata.set("device"sv, static_cast<u64>(identity.st_dev));
    metadata.set("inode"sv, static_cast<u64>(identity.st_ino));
    auto serialized = metadata.serialized();
    TRY(record->write_until_depleted(serialized.bytes()));
    if (fsync(record->fd()) < 0)
        return Error::from_errno(errno);

    NativeReference reference { move(ticket), {} };
    m_entries.find(handle.token)->value.native_reference = reference;
    handle.native_reference = move(reference);
    remove_on_failure.disarm();
    return {};
#endif
}

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::reopen_native(Document owner,
    StringView profile_data, StringView origin_key, NativeReference const& reference, Kind kind)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)profile_data;
    (void)origin_key;
    (void)reference;
    (void)kind;
    return Error::from_errno(ENOTSUP);
#else
    if (!valid_reference_ticket(reference.ticket.bytes_as_string_view())
        || reference.components.size() > 64)
        return Error::from_errno(EINVAL);

    auto directory = TRY(native_reference_directory(profile_data, origin_key));
    auto record
        = TRY(open_entry(directory->fd(), reference.ticket.bytes_as_string_view(), Kind::File));
    auto file_metadata = TRY(Core::System::fstat(record->fd()));
    constexpr size_t max_record_size = 64 * 1024;
    if (file_metadata.st_uid != geteuid() || (file_metadata.st_mode & 0077)
        || file_metadata.st_size <= 0 || static_cast<u64>(file_metadata.st_size) > max_record_size)
        return Error::from_errno(EACCES);

    auto bytes = TRY(ByteBuffer::create_uninitialized(static_cast<size_t>(file_metadata.st_size)));
    TRY(record->read_until_filled(bytes.bytes()));
    auto parsed = TRY(JsonValue::from_string(StringView(bytes.bytes())));
    if (!parsed.is_object())
        return Error::from_errno(EINVAL);
    auto const& metadata = parsed.as_object();
    auto path = metadata.get_string("path"sv);
    auto encoded_kind = metadata.get_string("kind"sv);
    auto device = metadata.get_u64("device"sv);
    auto inode = metadata.get_u64("inode"sv);
    if (metadata.get_u32("version"sv).value_or(0) != 1 || !path.has_value()
        || !encoded_kind.has_value()
        || (!encoded_kind->bytes_as_string_view().is_one_of("file"sv, "directory"sv))
        || !device.has_value() || !inode.has_value())
        return Error::from_errno(EINVAL);

    auto root_kind = *encoded_kind == "file"sv ? Kind::File : Kind::Directory;
    if ((reference.components.is_empty() && root_kind != kind)
        || (!reference.components.is_empty() && root_kind != Kind::Directory))
        return Error::from_errno(EINVAL);

    auto current = TRY(admit(owner, path->bytes_as_string_view(), root_kind));
    ArmedScopeGuard release_current = [&] { release(owner, current.token); };
    auto entry = TRY(lookup(owner, current.token));
    auto anchor = entry->descriptor->fd();
    OwnPtr<Core::File> parent;
    if (root_kind == Kind::File) {
        auto dirname = LexicalPath::dirname(path->to_byte_string());
        parent = TRY(open_entry(AT_FDCWD, dirname.view(), Kind::Directory));
        auto reopened
            = TRY(open_entry(parent->fd(), entry->name.bytes_as_string_view(), Kind::File));
        if (!identical(TRY(Core::System::fstat(reopened->fd())), TRY(Core::System::fstat(anchor))))
            return Error::from_errno(EAGAIN);
        anchor = parent->fd();
    }

    auto identity = TRY(Core::System::fstat(anchor));
    if (static_cast<u64>(identity.st_dev) != *device || static_cast<u64>(identity.st_ino) != *inode)
        return Error::from_errno(EACCES);

    for (size_t i = 0; i < reference.components.size(); ++i) {
        auto next = TRY(child(owner, current.token, reference.components[i].bytes_as_string_view(),
            i + 1 == reference.components.size() ? kind : Kind::Directory));
        release(owner, current.token);
        current = move(next);
    }

    // Only an explicit browser decision can survive navigation. The opaque
    // serialized reference itself carries no authority to read or change files.
    auto access = TRY(remembered_access(directory->fd(), reference));
    auto& restored = m_entries.find(current.token)->value;
    restored.readable = access != RememberedAccess::None;
    restored.native_reference = reference;
    current.native_reference = reference;
    if (access == RememberedAccess::Write)
        TRY(set_write_permission(owner, current.token, true));
    release_current.disarm();
    return current;
#endif
}

bool FileAccessGrants::in_private_area(StringView path) const
{
    auto area = m_private_area.bytes_as_string_view();
    return !area.is_empty()
        && (path == area
            || (path.starts_with(area) && path.length() > area.length()
                && path[area.length()] == '/'));
}

ErrorOr<void> FileAccessGrants::check_public_directory(int fd) const
{
#ifdef AK_OS_WINDOWS
    (void)fd;
#else
    if (!m_private_area.is_empty()) {
        auto path = TRY(descriptor_path(fd));
        if (in_private_area(path.bytes_as_string_view()))
            return Error::from_errno(EACCES);
    }
#endif
    return {};
}

ErrorOr<StringView> FileAccessGrants::permission(
    Document owner, String const& token, bool writable) const
{
    auto entry = TRY(lookup(owner, token, false));
    if (!entry->readable)
        return entry->read_denied || (writable && entry->write_denied) ? "denied"sv : "prompt"sv;
    if (!writable || entry->writable)
        return "granted"sv;
    return entry->write_denied ? "denied"sv : "prompt"sv;
}

ErrorOr<String> FileAccessGrants::name(Document owner, String const& token) const
{
    return TRY(lookup(owner, token, false))->name;
}

ErrorOr<String> FileAccessGrants::directory_hint(Document owner, String const& token) const
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token));
    if (entry->private_bucket)
        return Error::from_errno(EACCES);
    auto path = TRY(descriptor_path(entry->descriptor->fd()));
    if (entry->kind == Kind::Directory)
        return path;
    auto directory = LexicalPath::dirname(path.to_byte_string());
    return String::from_utf8(directory.view());
#endif
}

bool FileAccessGrants::has_native_reference(Document owner, String const& token) const
{
    auto entry = lookup(owner, token, false);
    return !entry.is_error() && entry.value()->native_reference.has_value();
}

ErrorOr<void> FileAccessGrants::remember_permission(Document owner, String const& token,
    StringView profile_data, StringView origin_key, bool writable)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)profile_data;
    (void)origin_key;
    (void)writable;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token));
    if (!entry->native_reference.has_value() || (writable && !entry->writable))
        return Error::from_errno(EACCES);

    auto const& reference = *entry->native_reference;
    auto origin = TRY(native_reference_directory(profile_data, origin_key));
    auto permissions = TRY(open_private_directory(origin->fd(), ".permissions"sv));
    auto current
        = TRY(open_private_directory(permissions->fd(), reference.ticket.bytes_as_string_view()));
    for (auto const& component : reference.components)
        current = TRY(open_private_directory(current->fd(), component.bytes_as_string_view()));

    auto mode = writable ? ".elytra-file-write"sv : ".elytra-file-read"sv;
    auto marker = ByteString { mode };
    auto fd = openat(current->fd(), marker.characters(),
        O_RDONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd >= 0)
        return Core::System::close(fd);
    if (errno == EEXIST && TRY(permission_marker(current->fd(), mode)))
        return {};
    return Error::from_errno(errno);
#endif
}

ErrorOr<void> FileAccessGrants::set_read_permission(
    Document owner, String const& token, bool granted)
{
    auto record = m_entries.find(token);
    if (record == m_entries.end() || record->value.owner != owner)
        return Error::from_errno(EACCES);

    if (!record->value.readable) {
        record->value.readable = granted;
        record->value.read_denied = !granted;
    }

    return {};
}

ErrorOr<void> FileAccessGrants::set_write_permission(
    Document owner, String const& token, bool granted)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)granted;
    return Error::from_errno(ENOTSUP);
#else
    auto record = m_entries.find(token);
    if (record == m_entries.end() || record->value.owner != owner)
        return Error::from_errno(EACCES);

    auto& entry = record->value;
    if (entry.writable)
        return {};
    if (!granted) {
        entry.write_denied = true;
        return {};
    }

    if (entry.kind == Kind::File) {
        auto path = TRY(descriptor_path(entry.descriptor->fd()));
        auto dirname = LexicalPath::dirname(path.to_byte_string());
        auto parent = TRY(open_entry(AT_FDCWD, dirname.view(), Kind::Directory));
        auto basename = LexicalPath::basename(path.to_byte_string());
        auto current = TRY(open_entry(parent->fd(), basename.view(), Kind::File));
        if (!identical(TRY(Core::System::fstat(current->fd())),
                TRY(Core::System::fstat(entry.descriptor->fd()))))
            return Error::from_errno(EAGAIN);
        auto target = TRY(adopt_nonnull_own_or_enomem(
            new (nothrow) SaveTarget { move(parent), ByteString { basename } }));
        TRY(m_save_targets.try_set(token, move(target)));
    }

    entry.readable = true;
    entry.read_denied = false;
    entry.writable = true;
    entry.write_denied = false;
    return {};
#endif
}

ErrorOr<FileAccessGrants::FileVersion> FileAccessGrants::file_version(
    Document owner, String const& token) const
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token, Kind::File));
    auto status = TRY(Core::System::fstat(entry->descriptor->fd()));
#ifdef AK_OS_MACOS
    auto modified = status.st_mtimespec;
    auto changed = status.st_ctimespec;
#else
    auto modified = status.st_mtim;
    auto changed = status.st_ctim;
#endif
    auto fingerprint = TRY(String::formatted("{}:{}:{}:{}:{}:{}:{}", status.st_dev, status.st_ino,
        status.st_size, modified.tv_sec, modified.tv_nsec, changed.tv_sec, changed.tv_nsec));
    return FileVersion { move(fingerprint),
        UnixDateTime::from_unix_timespec(modified).milliseconds_since_epoch() };
#endif
}

ErrorOr<ByteBuffer> FileAccessGrants::read(
    Document owner, String const& token, u64 offset, size_t count) const
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)offset;
    (void)count;
    return Error::from_errno(ENOTSUP);
#else
    auto entry = TRY(lookup(owner, token, Kind::File));
    if (count > max_read_size || offset > static_cast<u64>(NumericLimits<off_t>::max()) - count)
        return Error::from_errno(EINVAL);

    auto buffer = TRY(ByteBuffer::create_uninitialized(count));
    size_t used = 0;
    while (used < count) {
        auto amount = pread(entry->descriptor->fd(), buffer.data() + used, count - used,
            static_cast<off_t>(offset + used));
        if (amount < 0 && errno == EINTR)
            continue;
        if (amount < 0)
            return Error::from_errno(errno);
        if (amount == 0)
            break;
        used += static_cast<size_t>(amount);
    }

    buffer.resize(used);
    return buffer;
#endif
}

String FileAccessGrants::new_token() const
{
    auto token = AK::generate_random_uuid();
    while (m_entries.contains(token) || m_cursors.contains(token) || m_writers.contains(token)
        || m_sync_access.contains(token))
        token = AK::generate_random_uuid();
    return token;
}

ErrorOr<String> FileAccessGrants::begin_iteration(Document owner, String const& directory_token)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)directory_token;
    return Error::from_errno(ENOTSUP);
#else
    auto directory = TRY(lookup(owner, directory_token, Kind::Directory));
    if (m_cursors.size() >= max_cursors)
        return Error::from_errno(ENOSPC);

    // A fresh directory description gives every iterator its own position.
    auto file = TRY(open_entry(directory->descriptor->fd(), "."sv, Kind::Directory));
    auto* stream = fdopendir(file->fd());
    if (!stream)
        return Error::from_errno(errno);
    (void)file->leak_fd();

    auto cursor
        = adopt_nonnull_own_or_enomem(new (nothrow) Cursor { owner, directory_token, stream });
    if (cursor.is_error()) {
        closedir(stream);
        return cursor.release_error();
    }

    auto token = new_token();
    TRY(m_cursors.try_set(token, cursor.release_value()));
    return token;
#endif
}

ErrorOr<FileAccessGrants::DirectoryPage> FileAccessGrants::next_entries(
    Document owner, String const& token, size_t count)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)count;
    return Error::from_errno(ENOTSUP);
#else
    auto it = m_cursors.find(token);
    if (it == m_cursors.end() || it->value->owner != owner)
        return Error::from_errno(EACCES);
    if (count == 0 || count > max_entries_per_page)
        return Error::from_errno(EINVAL);

    DirectoryPage page;
    TRY(page.entries.try_ensure_capacity(count));
    auto* stream = it->value->stream;
    String public_directory;
    if (!m_private_area.is_empty()
        && !TRY(lookup(owner, it->value->directory_token))->private_bucket)
        public_directory = TRY(descriptor_path(dirfd(stream)));
    // On a hard error retire this cursor rather than retrying after consuming entries.
    ArmedScopeGuard retire = [&] { m_cursors.remove(it); };
    for (size_t scans = 0; scans < max_scans_per_page && page.entries.size() < count; ++scans) {
        errno = 0;
        auto* entry = readdir(stream);
        if (!entry) {
            if (errno)
                return Error::from_errno(errno);
            page.done = true;
            return page;
        }

        StringView name(entry->d_name, strlen(entry->d_name));
        if (!is_entry_name(name))
            continue;
        if (!public_directory.is_empty()) {
            auto path = TRY(String::formatted(
                "{}{}{}", public_directory, public_directory.ends_with('/') ? ""sv : "/"sv, name));
            if (in_private_area(path.bytes_as_string_view()))
                continue;
        }

        struct stat metadata {};
        if (fstatat(dirfd(stream), entry->d_name, &metadata, AT_SYMLINK_NOFOLLOW) < 0) {
            if (errno == ENOENT)
                continue;
            return Error::from_errno(errno);
        }

        if (!S_ISDIR(metadata.st_mode) && !S_ISREG(metadata.st_mode))
            continue;
        if (!Utf8View { name }.validate())
            continue;
        page.entries.append({ TRY(String::from_utf8(name)),
            S_ISDIR(metadata.st_mode) ? Kind::Directory : Kind::File });
    }

    retire.disarm();
    return page;
#endif
}

void FileAccessGrants::close_iteration(Document owner, String const& token)
{
    auto it = m_cursors.find(token);
    if (it != m_cursors.end() && it->value->owner == owner)
        m_cursors.remove(it);
}

void FileAccessGrants::release(Document owner, String const& token)
{
    auto it = m_entries.find(token);
    if (it == m_entries.end() || it->value.owner != owner)
        return;
    m_cursors.remove_all_matching(
        [&](auto const&, auto const& cursor) { return cursor->directory_token == token; });
    m_writers.remove_all_matching(
        [&](auto const&, auto const& writer) { return writer->entry_token == token; });
    m_sync_access.remove_all_matching(
        [&](auto const&, auto const& handle) { return handle->file_token == token; });
    m_save_targets.remove(token);
    m_entries.remove(it);
}

void FileAccessGrants::revoke_all()
{
    m_cursors.clear();
    m_writers.clear();
    m_sync_access.clear();
    m_save_targets.clear();
    m_entries.clear();
}

}
