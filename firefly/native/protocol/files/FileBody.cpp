/** Keep one checked descriptor until EOF/cancel/error; never reopen a pathname between chunks. */
#include "FileBody.h"
#include <AK/CharacterTypes.h>
#include <LibCore/System.h>
#ifdef AK_OS_WINDOWS
#include <AK/Windows.h>
#endif

namespace Firefly::Protocols {
ErrorOr<NonnullOwnPtr<FileBody>> FileBody::open(StringView path, bool head)
{
    bool absolute = path.starts_with('/');
#ifdef AK_OS_WINDOWS
    // A drive-relative path (C:foo) or rooted current-drive path is not a capability.
    absolute = (path.length() >= 3 && is_ascii_alpha(path[0]) && path[1] == ':'
                   && (path[2] == '/' || path[2] == '\\'))
        || path.starts_with("\\\\"sv);
#endif
    if (path.is_empty() || path.length() > 32768 || path.contains('\0') || !absolute)
        return Error::from_errno(EINVAL);
    auto mode = Core::File::OpenMode::Read;
#ifndef AK_OS_WINDOWS
    // Opening a FIFO must not block the main event loop before fstat refuses it.
    mode = mode | Core::File::OpenMode::Nonblocking;
#endif
    auto file = TRY(Core::File::open(path, mode));
#ifdef AK_OS_WINDOWS
    if (GetFileType(to_handle(file->fd())) != FILE_TYPE_DISK)
        return Error::from_errno(EACCES);
#endif
    auto info = TRY(file->stat());
    if (!S_ISREG(info.st_mode))
        return Error::from_errno(EACCES);
    if (info.st_size < 0 || static_cast<u64>(info.st_size) > file_size_limit)
        return Error::from_errno(EFBIG);
    auto length = static_cast<u64>(info.st_size);
    return adopt_nonnull_own_or_enomem(new (nothrow) FileBody(move(file), length, head));
}

FileBody::FileBody(NonnullOwnPtr<Core::File> file, u64 length, bool head)
    : m_file(move(file))
    , m_length(length)
{
    if (head || !length) {
        m_state = State::Complete;
        m_file->close();
    }
}

ErrorOr<ByteBuffer> FileBody::read(size_t maximum)
{
    if (!maximum || maximum > file_chunk_limit)
        return Error::from_errno(EINVAL);
    if (m_state == State::Complete)
        return ByteBuffer {};
    if (m_state != State::Open)
        return Error::from_errno(ECANCELED);
    auto bytes = TRY(ByteBuffer::create_uninitialized(
        static_cast<size_t>(min<u64>(maximum, m_length - m_transferred))));
    auto result = m_file->read_some(bytes);
    if (result.is_error() || result.value().is_empty()) {
        // A truncated/replaced file is not successful EOF at the advertised length.
        m_state = State::Failed;
        m_file->close();
        if (result.is_error())
            return result.release_error();
        return Error::from_errno(EIO);
    }

    bytes.trim(result.value().size(), false);
    m_transferred += bytes.size();
    if (m_transferred == m_length) {
        m_state = State::Complete;
        m_file->close();
    }

    return bytes;
}

void FileBody::cancel()
{
    if (m_state != State::Open)
        return;
    m_state = State::Cancelled;
    m_file->close();
}
}
