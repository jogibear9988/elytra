/** Native selected-file provenance. No public JavaScript API, filename inference or file grants. */
#pragma once
#include <AK/Error.h>
#include <AK/JsonValue.h>
#include <AK/Noncopyable.h>
#include <AK/Optional.h>
#include <AK/OwnPtr.h>
#include <AK/Span.h>
#include <AK/String.h>
#include <LibWeb/Export.h>
#include <LibWebCommon/HTML/SelectedFile.h>

namespace Web::DOM {
class Document;
}

namespace Web::FileAPI {
class File;
}

namespace Firefly::FileOrigins {
constexpr size_t max_entries = 1024;
constexpr size_t max_path_bytes = 1024 * 1024;

// The controller supplies an exact native selection span and validated path metadata.
// A synchronous scope cannot be nested or retained by script. File creation still
// works without one, but such selected files have explicitly unavailable provenance.
class WEB_API Selection final {
    AK_MAKE_NONCOPYABLE(Selection);

    AK_MAKE_NONMOVABLE(Selection);

public:
    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<NonnullOwnPtr<Selection>> begin(JsonValue const&, Span<Web::HTML::SelectedFile>);

    ~Selection();

private:
    friend void did_select(
        Web::DOM::Document&, Web::HTML::SelectedFile const&, Web::FileAPI::File&);

    Selection(Vector<String>, Vector<String>, Span<Web::HTML::SelectedFile>);

    Vector<String> m_documents;
    Vector<String> m_paths;
    Span<Web::HTML::SelectedFile> m_files;
};

WEB_API void did_select(Web::DOM::Document&, Web::HTML::SelectedFile const&, Web::FileAPI::File&);
// None means a File never observed at the native selection boundary (e.g. new File).
// An observed selection without valid metadata or the right document fails. After
// quota refusal, unrecorded identities also fail rather than being misclassified.
WEB_API ErrorOr<Optional<String>> lookup(Web::DOM::Document&, Web::FileAPI::File&);

WEB_API size_t live_entries();
// Native lifecycle/test cleanup only. No active Selection may be cleared.
WEB_API void clear();
}
