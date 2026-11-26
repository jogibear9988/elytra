/** Own native file capabilities for one browser view. */
#pragma once

#include <AK/ByteBuffer.h>
#include <AK/HashMap.h>
#include <AK/RefPtr.h>
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
        Optional<Vector<String>> private_path;
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
    ErrorOr<Handle> admit(Document, StringView absolute_path, Kind, bool writable = false);

    ErrorOr<Handle> admit_save(Document, StringView absolute_path);

    ErrorOr<Handle> admit_private(Document, StringView profile_data, StringView bucket_key);

    ErrorOr<Handle> reopen_private(Document, StringView profile_data, StringView bucket_key,
        Vector<String> const& components, Kind);

    void set_private_area(String directory)
    {
        m_private_area = move(directory);
    }

    ErrorOr<String> begin_write(Document, String const& token, bool keep_existing_data);

    ErrorOr<void> write(Document, String const& writer, u64 offset, ReadonlyBytes);

    ErrorOr<void> truncate(Document, String const& writer, u64 size);

    ErrorOr<void> finish_write(Document, String const& writer, bool commit);

    ErrorOr<String> begin_sync(Document, String const& file, bool writable, bool exclusive);

    ErrorOr<ByteBuffer> sync_read(Document, String const&, u64 offset, size_t count);

    ErrorOr<void> sync_write(Document, String const&, u64 offset, ReadonlyBytes);

    ErrorOr<void> sync_truncate(Document, String const&, u64 size);

    ErrorOr<u64> sync_size(Document, String const&);

    ErrorOr<void> sync_flush(Document, String const&);

    void close_sync(Document, String const&);

    ErrorOr<Handle> child(
        Document, String const& token, StringView name, Kind, bool create = false);

    ErrorOr<void> remove(Document, String const& directory, StringView name, bool recursive);

    ErrorOr<bool> is_same_entry(Document, String const&, String const&) const;

    ErrorOr<Optional<Vector<String>>> resolve(
        Document, String const& directory, String const& descendant) const;

    ErrorOr<StringView> permission(Document, String const& token, bool writable) const;

    ErrorOr<String> name(Document, String const& token) const;

    ErrorOr<String> directory_hint(Document, String const& token) const;

    ErrorOr<void> set_write_permission(Document, String const& token, bool granted);

    struct FileVersion {
        String fingerprint;
        i64 last_modified;
    };

    ErrorOr<FileVersion> file_version(Document, String const& token) const;

    ErrorOr<ByteBuffer> read(Document, String const& token, u64 offset, size_t count) const;

    ErrorOr<String> begin_iteration(Document, String const& directory_token);

    ErrorOr<DirectoryPage> next_entries(Document, String const& cursor_token, size_t count);

    void close_iteration(Document, String const& cursor_token);

    void release(Document, String const& token);

    void revoke_all();

private:
    struct PrivateBucket;

    struct Entry {
        Document owner;
        Kind kind;
        NonnullOwnPtr<Core::File> descriptor;
        bool writable;
        bool write_denied { false };
        String name;
        RefPtr<PrivateBucket> private_bucket;
        Optional<Vector<String>> private_path;
    };

    struct Cursor;
    struct SaveTarget;
    struct Writer;
    struct SyncAccess;
    static constexpr size_t max_cursors = 32;
    static constexpr size_t max_scans_per_page = 256;

    ErrorOr<Handle> remember(Document, StringView name, Kind, NonnullOwnPtr<Core::File>,
        bool writable = false, RefPtr<PrivateBucket> = {}, Optional<Vector<String>> = {});

    ErrorOr<Handle> admit_saved_file(Document, StringView name, NonnullOwnPtr<Core::File> parent,
        bool create, RefPtr<PrivateBucket> = {}, Optional<Vector<String>> = {});

    ErrorOr<Entry const*> lookup(Document, String const& token) const;

    ErrorOr<Writer*> lookup_writer(Document, String const& token);

    ErrorOr<SyncAccess*> lookup_sync(Document, String const& token, bool writable = false);

    ErrorOr<Entry const*> lookup(Document, String const& token, Kind) const;

    String new_token() const;

    ErrorOr<void> check_public_directory(int fd) const;

    bool in_private_area(StringView path) const;

    String m_private_area;

    HashMap<String, Entry> m_entries;
    HashMap<String, NonnullOwnPtr<SaveTarget>> m_save_targets;
    HashMap<String, NonnullOwnPtr<Writer>> m_writers;
    HashMap<String, NonnullOwnPtr<SyncAccess>> m_sync_access;
    HashMap<String, NonnullOwnPtr<Cursor>> m_cursors;
};

}
