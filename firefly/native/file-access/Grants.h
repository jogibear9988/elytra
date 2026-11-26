/** Own native file capabilities for one browser view. */
#pragma once

#include <AK/ByteBuffer.h>
#include <AK/HashMap.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibCore/File.h>
#include <LibWebCommon/HTML/CrossProcessId.h>

namespace Firefly {

class FileAccessGrants {
public:
    enum class Kind : u8 { File, Directory };

    struct Handle {
        String token;
        String name;
        Kind kind;
    };

    struct DirectoryEntry {
        String name;
        Kind kind;
    };

    struct DirectoryPage {
        Vector<DirectoryEntry> entries;
        bool done { false };
    };

    using Document = Web::HTML::CrossProcessId;
    static constexpr size_t max_grants = 256;
    static constexpr size_t max_entries_per_page = 64;
    static constexpr size_t max_read_size = 1024 * 1024;

    FileAccessGrants();

    ~FileAccessGrants();

    static bool supported();

    // Only native picker completions supply paths. Subsequent requests operate
    // on opaque tokens and single child names relative to an opened directory.
    ErrorOr<Handle> admit(Document, StringView absolute_path, Kind);

    ErrorOr<Handle> child(Document, String const& token, StringView name, Kind);

    ErrorOr<ByteBuffer> read(Document, String const& token, u64 offset, size_t count) const;

    ErrorOr<String> begin_iteration(Document, String const& directory_token);

    ErrorOr<DirectoryPage> next_entries(Document, String const& cursor_token, size_t count);

    void close_iteration(Document, String const& cursor_token);

    void release(Document, String const& token);

    void revoke_all();

private:
    struct Entry {
        Document owner;
        Kind kind;
        NonnullOwnPtr<Core::File> descriptor;
    };

    struct Cursor;
    static constexpr size_t max_cursors = 32;
    static constexpr size_t max_scans_per_page = 256;

    ErrorOr<Handle> remember(Document, StringView name, Kind, NonnullOwnPtr<Core::File>);

    ErrorOr<Entry const*> lookup(Document, String const& token, Kind) const;

    String new_token() const;

    HashMap<String, Entry> m_entries;
    HashMap<String, NonnullOwnPtr<Cursor>> m_cursors;
};

}
