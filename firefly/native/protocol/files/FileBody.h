/** Bounded regular-file response body. Trusted main code authorizes the path before opening. */
#pragma once
#include "Limits.h"
#include <AK/ByteBuffer.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StringView.h>
#include <LibCore/File.h>

namespace Firefly::Protocols {
// Read into byte-channel sized chunks, never a whole-file JSON/string snapshot.
class FileBody final {
    AK_ALLOC_WITH_KMALLOC;
    AK_MAKE_NONCOPYABLE(FileBody);

    AK_MAKE_NONMOVABLE(FileBody);

public:
    enum class State { Open, Complete, Cancelled, Failed };

    // This is not URL-to-path resolution or a renderer filesystem capability.
    // The caller must already have authorized the application's returned path.
    static ErrorOr<NonnullOwnPtr<FileBody>> open(StringView authorized_path, bool head = false);

    ErrorOr<ByteBuffer> read(size_t maximum = file_chunk_limit);

    void cancel();

    u64 content_length() const
    {
        return m_length;
    }

    u64 transferred() const
    {
        return m_transferred;
    }

    State state() const
    {
        return m_state;
    }

private:
    FileBody(NonnullOwnPtr<Core::File>, u64 length, bool head);

    NonnullOwnPtr<Core::File> m_file;
    u64 m_length;
    u64 m_transferred { 0 };
    State m_state { State::Open };
};
}
