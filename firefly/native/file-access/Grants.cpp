#include "Grants.h"

#include <AK/ByteString.h>
#include <AK/LexicalPath.h>
#include <AK/NumericLimits.h>
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <AK/Utf8View.h>
#include <LibCore/System.h>
#include <errno.h>

#ifndef AK_OS_WINDOWS
#include <dirent.h>
#include <fcntl.h>
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

struct FileAccessGrants::Writer {
    AK_ALLOC_WITH_KMALLOC;

public:
    Document owner;
    String entry_token;
    NonnullOwnPtr<Core::File> parent;
    NonnullOwnPtr<Core::File> file;
    ByteString temporary_name;

    ~Writer()
    {
#ifndef AK_OS_WINDOWS
        if (!temporary_name.is_empty())
            (void)unlinkat(parent->fd(), temporary_name.characters(), 0);
#endif
    }
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

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::remember(
    Document owner, StringView name, Kind kind, NonnullOwnPtr<Core::File> file)
{
    auto display_name = TRY(String::from_utf8(name));
    auto token = new_token();
    TRY(m_entries.try_set(token, Entry { owner, kind, move(file) }));
    return Handle { move(token), move(display_name), kind };
}

ErrorOr<FileAccessGrants::Entry const*> FileAccessGrants::lookup(
    Document owner, String const& token, Kind kind) const
{
    auto it = m_entries.find(token);
    if (it == m_entries.end() || it->value.owner != owner)
        return Error::from_errno(EACCES);
    if (it->value.kind != kind)
        return Error::from_errno(kind == Kind::Directory ? ENOTDIR : EISDIR);
    return &it->value;
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
    Document owner, StringView path, Kind kind)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)path;
    (void)kind;
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
    return remember(owner, LexicalPath::basename(path), kind, move(file));
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
    auto target = TRY(adopt_nonnull_own_or_enomem(
        new (nothrow) SaveTarget { move(parent), ByteString { name } }));

    auto file = open_entry(target->parent->fd(), name, Kind::File);
    if (file.is_error() && file.error().is_errno() && file.error().code() == ENOENT) {
        auto fd = openat(target->parent->fd(), target->name.characters(),
            O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0)
            return Error::from_errno(errno);
        ArmedScopeGuard close_on_error = [&] { (void)Core::System::close(fd); };
        file = Core::File::adopt_fd(fd, Core::File::OpenMode::ReadWrite);
        if (!file.is_error())
            close_on_error.disarm();
    }

    if (file.is_error())
        return file.release_error();

    auto handle = TRY(remember(owner, name, Kind::File, file.release_value()));
    auto stored = m_save_targets.try_set(handle.token, move(target));
    if (stored.is_error()) {
        release(owner, handle.token);
        return stored.release_error();
    }

    return handle;
#endif
}

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
        for (u64 offset = 0; offset < size;) {
            auto bytes = TRY(read(owner, token, offset, min<u64>(max_read_size, size - offset)));
            if (bytes.is_empty())
                break;
            TRY(file->write_until_depleted(bytes.bytes()));
            offset += bytes.size();
        }
    }

    auto* writer = new (nothrow) Writer { owner, token, move(parent), move(file), move(name) };
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

ErrorOr<FileAccessGrants::Handle> FileAccessGrants::child(
    Document owner, String const& token, StringView name, Kind kind)
{
#ifdef AK_OS_WINDOWS
    (void)owner;
    (void)token;
    (void)name;
    (void)kind;
    return Error::from_errno(ENOTSUP);
#else
    auto parent = TRY(lookup(owner, token, Kind::Directory));
    if (name.is_empty() || name == "."sv || name == ".."sv || name.length() > 255
        || name.contains('/') || name.contains('\\') || name.contains('\0'))
        return Error::from_errno(EINVAL);
    if (m_entries.size() >= max_grants)
        return Error::from_errno(ENOSPC);

    auto file = TRY(open_entry(parent->descriptor->fd(), name, kind));
    return remember(owner, name, kind, move(file));
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
    while (m_entries.contains(token) || m_cursors.contains(token) || m_writers.contains(token))
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
        if (name == "."sv || name == ".."sv || name.contains('\\'))
            continue;
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
    m_save_targets.remove(token);
    m_entries.remove(it);
}

void FileAccessGrants::revoke_all()
{
    m_cursors.clear();
    m_writers.clear();
    m_save_targets.clear();
    m_entries.clear();
}

}
