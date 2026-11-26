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
    while (m_entries.contains(token) || m_cursors.contains(token))
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
    m_entries.remove(it);
}

void FileAccessGrants::revoke_all()
{
    m_cursors.clear();
    m_entries.clear();
}

}
