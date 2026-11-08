/** Exercise actual native input File creation and weak exact provenance without desktop UI. */
#include "FileOriginTest.h"
#include "files/Origins.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/Bindings/Document.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/FileAPI/File.h>
#include <LibWeb/FileAPI/FileList.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>

ErrorOr<void> check_file_origins(JS::VM& vm, Web::DOM::Document& document)
{
    using namespace Firefly::FileOrigins;
    clear();
    ScopeGuard cleanup([] { clear(); });
    Web::HTML::TemporaryExecutionContext context(document.relevant_settings_object());
    auto require = [](bool value) -> ErrorOr<void> {
        if (!value)
            return Error::from_string_literal("Native File provenance assertion failed");
        return {};
    };

    auto metadata = [&](StringView path = "/chosen/ä.txt"sv) {
        JsonObject data;
        data.set("version"sv, 1);
        JsonArray docs, paths;
        docs.must_append(
            MUST(String::formatted("{}:{}", document.id().namespace_id, document.id().local_id)));
        paths.must_append(path);
        data.set("documents"sv, move(docs));
        data.set("paths"sv, move(paths));
        return data;
    };

    auto selection = [] {
        Vector<Web::HTML::SelectedFile> selected;
        selected.append(
            { Utf16String::from_utf8("ä.txt"sv), MUST(ByteBuffer::copy("bytes"sv.bytes())) });
        return selected;
    };

    auto files = selection();
    auto element
        = MUST(document.create_element("input"_utf16, Web::Bindings::ElementCreationOptions {}));
    GC::Root input { static_cast<Web::HTML::HTMLInputElement&>(*element) };
    input->set_type(u"file"sv);
    {
        auto scope = TRY(Selection::begin(metadata(), files));
        TRY(require(Selection::begin(metadata(), files).is_error()));
        input->did_select_files(files);
    }

    GC::Root list { input->files() };
    TRY(require(list && list->length() == 1));
    GC::Root selected { list->item(0) };
    auto path = TRY(lookup(document, *selected));
    TRY(require(path.has_value() && *path == "/chosen/ä.txt"sv));
    // Inactivation erases the path but keeps a weak negative tombstone. A later
    // reactivation must not reinterpret this surviving native File as synthetic.
    {
        GC::Root navigable { document.navigable() };
        ScopeGuard restore([&] { document.set_navigable(navigable.ptr()); });
        document.set_navigable(nullptr);
        TRY(require(lookup(document, *selected).is_error()));
    }

    TRY(require(document.is_fully_active() && lookup(document, *selected).is_error()));
    {
        auto scope = TRY(Selection::begin(metadata(), files));
        auto copied_selection = selection();
        GC::Root copied_file { Web::FileAPI::File::create(
            "bytes"sv.bytes(), Utf16String::from_utf8("ä.txt"sv), {}) };
        did_select(document, copied_selection[0], *copied_file);
        TRY(require(lookup(document, *copied_file).is_error()));
        GC::Root renamed { Web::FileAPI::File::create(
            "bytes"sv.bytes(), Utf16String::from_utf8("other"sv), {}) };
        did_select(document, files[0], *renamed);
        TRY(require(lookup(document, *renamed).is_error()));
    }
    // A file's name and bytes alone cannot inherit another File's native path.
    GC::Root synthetic { Web::FileAPI::File::create(
        "bytes"sv.bytes(), Utf16String::from_utf8("ä.txt"sv), {}) };
    TRY(require(!TRY(lookup(document, *synthetic)).has_value()));
    auto unavailable = selection();
    input->did_select_files(unavailable);
    GC::Root unknown { input->files()->item(0) };
    TRY(require(lookup(document, *unknown).is_error()));
    {
        auto scope = TRY(Selection::begin(metadata(), unavailable));
        did_select(document, unavailable[0], *unknown);
    }

    TRY(require(lookup(document, *unknown).is_error())); // No post-hoc upgrade.
    auto invalid = [&](JsonValue value) -> ErrorOr<void> {
        return require(Selection::begin(value, files).is_error());
    };

    TRY(invalid(JsonArray {}));
    auto wrong = metadata();
    wrong.set("version"sv, 1.5);
    TRY(invalid(wrong));
    wrong = metadata();
    wrong.set("extra"sv, true);
    TRY(invalid(wrong));
    wrong = metadata();
    wrong.set("paths"sv, JsonArray {});
    TRY(invalid(wrong));
    TRY(invalid(metadata("relative/ä.txt"sv)));
    TRY(invalid(metadata("/chosen/wrong.txt"sv)));
    TRY(invalid(metadata("/chosen/a\0ä.txt"sv)));
    for (auto id : { "0:1"sv, "01:1"sv, "1:0"sv, "1:2:3"sv, "18446744073709551616:1"sv }) {
        wrong = metadata();
        JsonArray docs;
        docs.must_append(id);
        wrong.set("documents"sv, move(docs));
        TRY(invalid(wrong));
    }

    wrong = metadata();
    JsonArray duplicate;
    duplicate.must_append("1:1"sv);
    duplicate.must_append("1:1"sv);
    wrong.set("documents"sv, move(duplicate));
    TRY(invalid(wrong));
    // A well-formed but different document ID never authorizes this input's File.
    wrong = metadata();
    JsonArray foreign;
    foreign.must_append(
        MUST(String::formatted("{}:{}", document.id().namespace_id, document.id().local_id + 1)));
    wrong.set("documents"sv, move(foreign));
    auto other = selection();
    {
        auto scope = TRY(Selection::begin(wrong, other));
        input->did_select_files(other);
    }

    TRY(require(lookup(document, *input->files()->item(0)).is_error()));
    clear();
    // Direct native boundary injection isolates GC/quota behavior from queued input tasks.
    {
        GC::Root transient { Web::FileAPI::File::create(
            "x"sv.bytes(), Utf16String::from_utf8("ä.txt"sv), {}) };
        auto scope = TRY(Selection::begin(metadata(), files));
        did_select(document, files[0], *transient);
        TRY(require(live_entries() == 1));
    }

    vm.heap().collect_garbage();
    TRY(require(live_entries() == 0));
    GC::RootVector<GC::Ref<Web::FileAPI::File>> retained;
    for (size_t i = 0; i < max_entries + 1; ++i) {
        auto file
            = Web::FileAPI::File::create("x"sv.bytes(), Utf16String::from_utf8("ä.txt"sv), {});
        retained.append(file);
        did_select(document, files[0], *file);
    }

    TRY(require(live_entries() == max_entries && lookup(document, *retained[0]).is_error()
        && lookup(document, *synthetic).is_error()));
    retained.clear();
    clear();
    // Long valid native paths hit the independent path-byte budget before entry count.
    auto prefix = MUST(String::repeated('x', 4088));
    auto long_path = MUST(String::formatted("/{}/ä.txt", prefix));
    auto grant = metadata(long_path);
    {
        auto scope = TRY(Selection::begin(grant, files));
        for (size_t i = 0; i < 260; ++i) {
            auto file
                = Web::FileAPI::File::create("x"sv.bytes(), Utf16String::from_utf8("ä.txt"sv), {});
            retained.append(file);
            did_select(document, files[0], *file);
            if (i == 255) {
                auto exact = TRY(lookup(document, *retained[0]));
                TRY(require(
                    live_entries() == 256 && exact.has_value() && exact->bytes().size() == 4096));
            }
        }
    }

    TRY(require(
        live_entries() == max_path_bytes / 4096 && lookup(document, *retained[0]).is_error()));
    outln("PASS native File provenance: actual input hook/schema/path identity/no spoof/no "
          "upgrade/GC/entry and byte quotas");
    return {};
}
