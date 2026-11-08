/** Weak exact File/document identities prevent reuse, filename spoofing and retained DOM roots. */
#include "Origins.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/LexicalPath.h>
#include <AK/NeverDestroyed.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/FileAPI/File.h>

namespace Firefly::FileOrigins {
namespace {
    struct Entry {
        GC::Weak<Web::FileAPI::File> file;
        GC::Weak<Web::DOM::Document> document;
        Optional<String> path;
    };

    struct State {
        Selection* selection { nullptr };
        Vector<Entry> entries;
        Vector<GC::Weak<Web::DOM::Document>> refused;
        size_t bytes { 0 };
        bool refused_all { false };
        bool missing_refusal { false };
    };

    NeverDestroyed<State> state;

    void sweep()
    {
        auto& data = state.get();
        data.entries.remove_all_matching([&](auto& entry) {
            if (!entry.file) {
                if (entry.path.has_value())
                    data.bytes -= entry.path->bytes().size();
                return true;
            }
            // Keep a weak negative tombstone for a surviving native File. Dropping
            // its inactive document must never make it look like a synthetic File.
            if ((!entry.document || !entry.document->is_fully_active()) && entry.path.has_value()) {
                data.bytes -= entry.path->bytes().size();
                entry.path.clear();
            }

            return false;
        });
        data.refused.remove_all_matching([](auto const& document) { return !document; });
    }

    void refuse(Web::DOM::Document& document)
    {
        auto& data = state.get();
        // A refused File could outlive its document without a table slot. Missing
        // identities must remain ambiguous even after that weak document is gone.
        data.missing_refusal = true;
        if (data.refused.contains([&](auto const& item) { return item.ptr().ptr() == &document; }))
            return;
        // A bounded failure ledger prevents missing native records from looking
        // like synthetic Files after quota exhaustion. Overflow fails globally.
        if (data.refused.size() >= 64) {
            data.refused_all = true;
            return;
        }

        data.refused.append(document);
    }

    bool valid_document_id(StringView value)
    {
        auto parts = value.split_view(':');
        if (parts.size() != 2)
            return false;
        for (auto part : parts) {
            if (part.is_empty() || part[0] == '0')
                return false;
            for (auto ch : part)
                if (ch < '0' || ch > '9')
                    return false;
            auto number = part.to_number<u64>();
            if (!number.has_value() || !*number)
                return false;
        }

        return true;
    }
}

Selection::Selection(
    Vector<String> documents, Vector<String> paths, Span<Web::HTML::SelectedFile> files)
    : m_documents(move(documents))
    , m_paths(move(paths))
    , m_files(files)
{
    state.get().selection = this;
}

Selection::~Selection()
{
    VERIFY(state.get().selection == this);
    state.get().selection = nullptr;
}

ErrorOr<NonnullOwnPtr<Selection>> Selection::begin(
    JsonValue const& metadata, Span<Web::HTML::SelectedFile> files)
{
    if (state.get().selection || !metadata.is_object() || metadata.as_object().size() != 3
        || metadata.as_object().get_u32("version"sv) != 1u)
        return Error::from_string_literal("Invalid/nested native file selection metadata");
    auto docs = metadata.as_object().get_array("documents"sv);
    auto paths = metadata.as_object().get_array("paths"sv);
    if (!docs.has_value() || docs->is_empty() || docs->size() > 64 || !paths.has_value()
        || paths->size() != files.size() || files.size() > 16)
        return Error::from_string_literal("Invalid native file selection collections");
    Vector<String> documents, native_paths;
    size_t bytes = 0;
    for (auto const& item : docs->values()) {
        if (!item.is_string() || item.as_string().bytes().size() > 64
            || !valid_document_id(item.as_string()) || documents.contains_slow(item.as_string()))
            return Error::from_string_literal("Invalid native selection document identity");
        documents.append(item.as_string());
    }

    for (size_t i = 0; i < files.size(); ++i) {
        auto const& item = (*paths)[i];
        if (!item.is_string() || item.as_string().contains('\0')
            || item.as_string().bytes().size() > 4096
            || !LexicalPath::is_absolute_path(item.as_string())
            || item.as_string().bytes().size() > 65536 - bytes)
            return Error::from_string_literal("Invalid native selected path/budget");
        auto name = files[i].name().utf16_view().to_utf8(AllowLonelySurrogates::No);
        if (name.is_error()
            || LexicalPath::basename(item.as_string().to_byte_string())
                != name.value().bytes_as_string_view())
            return Error::from_string_literal("Selected path/name mismatch");
        bytes += item.as_string().bytes().size();
        native_paths.append(item.as_string());
    }

    return adopt_own(*new Selection(move(documents), move(native_paths), files));
}

void did_select(
    Web::DOM::Document& document, Web::HTML::SelectedFile const& selected, Web::FileAPI::File& file)
{
    GC::Root retained_file { file };
    GC::Root retained_document { document };
    sweep();
    if (!document.is_fully_active())
        return;
    auto& data = state.get();
    Optional<String> path;
    if (auto* selection = data.selection) {
        auto identity
            = ByteString::formatted("{}:{}", document.id().namespace_id, document.id().local_id);
        bool admitted = selection->m_documents.contains_slow(identity.view());
        if (admitted)
            for (size_t i = 0; i < selection->m_files.size(); ++i)
                if (&selection->m_files[i] == &selected && file.name() == selected.name()) {
                    path = selection->m_paths[i];
                    break;
                }
    }
    // A cell can be noted only once; a later control path cannot upgrade or replace it.
    for (auto const& entry : data.entries)
        if (entry.file.ptr().ptr() == &file)
            return;
    auto charge = path.has_value() ? path->bytes().size() : 0;
    if (data.entries.size() >= max_entries || charge > max_path_bytes - data.bytes) {
        refuse(document);
        return;
    }

    data.bytes += charge;
    data.entries.append({ file, document, move(path) });
}

ErrorOr<Optional<String>> lookup(Web::DOM::Document& document, Web::FileAPI::File& file)
{
    sweep();
    auto& data = state.get();
    if (!document.is_fully_active() || data.refused_all
        || data.refused.contains([&](auto const& item) { return item.ptr().ptr() == &document; }))
        return Error::from_string_literal(
            "Native file provenance document unavailable/quota exceeded");
    for (auto const& entry : data.entries) {
        if (entry.file.ptr().ptr() != &file)
            continue;
        if (entry.document.ptr().ptr() != &document || !entry.path.has_value())
            return Error::from_string_literal(
                "Native selected file provenance unavailable for this document");
        return entry.path;
    }

    if (data.missing_refusal)
        return Error::from_string_literal(
            "Native file provenance identity is ambiguous after quota refusal");
    return Optional<String> {};
}

size_t live_entries()
{
    sweep();
    return state.get().entries.size();
}

void clear()
{
    VERIFY(!state.get().selection);
    state.get().entries.clear();
    state.get().refused.clear();
    state.get().bytes = 0;
    state.get().refused_all = false;
    state.get().missing_refusal = false;
}
}
