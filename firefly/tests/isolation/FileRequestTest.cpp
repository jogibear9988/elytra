/** Actual engine pending-dialog lifecycle, exact target ownership and one-shot native delivery. */
#include "FileRequestTest.h"
#include "files/Origins.h"
#include "files/Requests.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibWeb/Bindings/Document.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/FileAPI/FileList.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/Page/Page.h>

ErrorOr<void> check_file_requests(JS::VM& vm, Web::DOM::Document& document)
{
    namespace Requests = Firefly::FileRequests;
    namespace Origins = Firefly::FileOrigins;
    auto& page = document.page();
    Origins::clear();
    ScopeGuard cleanup([&] {
        Requests::set_sender({});
        page.file_picker_closed({});
        Origins::clear();
    });
    Web::HTML::TemporaryExecutionContext context(document.relevant_settings_object());
    size_t stage = 0;
    auto require = [&](bool condition) -> ErrorOr<void> {
        ++stage;
        if (!condition) {
            warnln("Native picker ticket assertion {}", stage);
            return Error::from_string_literal("Native picker ticket assertion failed");
        }

        return {};
    };

    auto input = [&] {
        auto element = MUST(
            document.create_element("input"_utf16, Web::Bindings::ElementCreationOptions {}));
        GC::Ref node { static_cast<Web::HTML::HTMLInputElement&>(*element) };
        node->set_type(u"file"sv);
        return node;
    };

    auto files = [] {
        Vector<Web::HTML::SelectedFile> result;
        result.append(
            { Utf16String::from_utf8("chosen.txt"sv), MUST(ByteBuffer::copy("abc"sv.bytes())) });
        return result;
    };

    auto metadata = [&] {
        JsonObject result;
        result.set("version"sv, 1);
        JsonArray docs, paths;
        docs.must_append(
            MUST(String::formatted("{}:{}", document.id().namespace_id, document.id().local_id)));
        paths.must_append("/selected/chosen.txt"sv);
        result.set("documents"sv, move(docs));
        result.set("paths"sv, move(paths));
        return result;
    };

    Vector<Requests::Ticket> tickets;
    auto install = [&] {
        Requests::set_sender([&](Web::Page& actual, Requests::Ticket ticket) {
            VERIFY(&actual == &page);
            tickets.append(ticket);
        });
    };

    install();
    page.did_request_file_picker({}, {}, Web::HTML::AllowMultipleFiles::No);
    TRY(require(tickets.is_empty() && Requests::pending_count() == 0));
    GC::Root first { input() };
    page.did_request_file_picker(GC::Weak { *first }, {}, Web::HTML::AllowMultipleFiles::No);
    TRY(require(tickets.size() == 1 && Requests::pending_count() == 1
        && tickets.last().document == document.id()));
    auto selected = files();
    TRY(require(Requests::deliver(page, tickets.last(), metadata(), selected)));
    TRY(require(first->files() && first->files()->length() == 1 && Requests::pending_count() == 0));
    auto path = TRY(Origins::lookup(document, *first->files()->item(0)));
    TRY(require(path.has_value() && *path == "/selected/chosen.txt"sv));
    auto old = tickets.last();
    auto replay = files();
    TRY(require(!Requests::deliver(page, old, metadata(), replay)));
    GC::Root second { input() };
    page.did_request_file_picker(GC::Weak { *second }, {}, Web::HTML::AllowMultipleFiles::No);
    auto current = tickets.last();
    TRY(require(current.serial > old.serial && Requests::pending_count() == 1));
    TRY(require(
        !Requests::deliver(page, old, metadata(), replay) && Requests::pending_count() == 1));
    auto wrong = current;
    ++wrong.document.local_id;
    TRY(require(
        !Requests::deliver(page, wrong, metadata(), replay) && Requests::pending_count() == 1));
    auto depth = vm.execution_context_stack().size();
    GC::Root foreign_realm { Web::Bindings::create_a_principal_javascript_realm() };
    // The upstream test realm factory leaves its execution context current.
    // Restore the original settings frame before native delivery/cleanup.
    while (vm.execution_context_stack().size() > depth)
        vm.pop_execution_context();
    auto& foreign_page = Web::Bindings::principal_host_defined_page(*foreign_realm);
    TRY(require(!Requests::deliver(foreign_page, current, metadata(), replay)
        && Requests::pending_count() == 1));
    TRY(require(Requests::deliver(page, current, metadata(), replay)));
    TRY(require(
        second->files()->length() == 1 && first->files()->item(0) != second->files()->item(0)));
    // Every matching malformed response cancels its own input without importing any data.
    for (int kind = 0; kind < 7; ++kind) {
        GC::Root target { input() };
        page.did_request_file_picker(GC::Weak { *target }, {},
            kind >= 5 ? Web::HTML::AllowMultipleFiles::Yes : Web::HTML::AllowMultipleFiles::No);
        auto data = files();
        auto config = metadata();
        if (kind == 0)
            config.set("version"sv, 2);
        if (kind == 1) {
            JsonArray docs;
            docs.must_append("1:999999"sv);
            config.set("documents"sv, move(docs));
        }

        if (kind == 2)
            data[0] = Web::HTML::SelectedFile { Utf16String::from_utf8("chosen.txt"sv),
                MUST(ByteBuffer::create_zeroed(4 * 1024 * 1024 + 1)) };
        if (kind == 3) {
            JsonArray paths;
            paths.must_append("/selected/wrong.txt"sv);
            config.set("paths"sv, move(paths));
        }

        if (kind >= 4) {
            data.clear();
            JsonArray paths;
            auto count = kind == 4 ? 2 : kind == 5 ? 17 : 5;
            for (int index = 0; index < count; ++index) {
                data.append({ Utf16String::from_utf8("chosen.txt"sv),
                    MUST(ByteBuffer::create_zeroed(kind == 6 ? 4 * 1024 * 1024 : 0)) });
                paths.must_append("/selected/chosen.txt"sv);
            }

            config.set("paths"sv, move(paths));
        }

        TRY(require(Requests::deliver(page, tickets.last(), config, data)
            && Requests::pending_count() == 0));
        TRY(require(!target->files() || target->files()->length() == 0));
    }
    // Native close retires a ticket before another input can take its place.
    GC::Root cancelled { input() };
    page.did_request_file_picker(GC::Weak { *cancelled }, {}, Web::HTML::AllowMultipleFiles::No);
    auto cancelled_ticket = tickets.last();
    page.file_picker_closed({});
    TRY(require(Requests::pending_count() == 0
        && !Requests::deliver(page, cancelled_ticket, metadata(), replay)));
    GC::Root stale { input() };
    page.did_request_file_picker(GC::Weak { *stale }, {}, Web::HTML::AllowMultipleFiles::No);
    auto stale_ticket = tickets.last();
    {
        GC::Root navigable { document.navigable() };
        ScopeGuard restore([&] { document.set_navigable(navigable.ptr()); });
        document.set_navigable(nullptr);
        TRY(require(Requests::pending_count() == 0));
    }

    TRY(require(!Requests::deliver(page, stale_ticket, metadata(), replay)));
    page.file_picker_closed({});
    // Replacing a native sender cannot reuse a serial, including after cancellation.
    GC::Root replaced { input() };
    page.did_request_file_picker(GC::Weak { *replaced }, {}, Web::HTML::AllowMultipleFiles::No);
    auto replaced_ticket = tickets.last();
    Requests::set_sender({});
    page.file_picker_closed({});
    install();
    GC::Root fresh { input() };
    page.did_request_file_picker(GC::Weak { *fresh }, {}, Web::HTML::AllowMultipleFiles::No);
    auto fresh_ticket = tickets.last();
    TRY(require(fresh_ticket.serial > replaced_ticket.serial
        && !Requests::deliver(page, replaced_ticket, metadata(), replay)));
    auto final_data = files();
    TRY(require(Requests::deliver(page, fresh_ticket, metadata(), final_data)));
    // Moving the same input cell to another document retires its original ticket.
    GC::Root moved { input() };
    page.did_request_file_picker(GC::Weak { *moved }, {}, Web::HTML::AllowMultipleFiles::No);
    auto moved_ticket = tickets.last();
    GC::Root other_document { Web::DOM::Document::create(
        page, document.relevant_global_event_target()) };
    MUST(other_document->adopt_node(GC::Ref<Web::DOM::Node> { *moved }));
    TRY(require(Requests::pending_count() == 0
        && !Requests::deliver(page, moved_ticket, metadata(), replay)));
    page.file_picker_closed({});
    MUST(document.adopt_node(GC::Ref<Web::DOM::Node> { *moved }));
    // Pending bookkeeping must not keep a disconnected input alive.
    Requests::Ticket collected {};
    {
        GC::Root transient { input() };
        page.did_request_file_picker(
            GC::Weak { *transient }, {}, Web::HTML::AllowMultipleFiles::No);
        collected = tickets.last();
    }

    vm.heap().collect_garbage();
    TRY(require(
        Requests::pending_count() == 0 && !Requests::deliver(page, collected, metadata(), replay)));
    page.file_picker_closed({});
    // No sender preserves the upstream path; native metadata is not fabricated.
    Requests::set_sender({});
    GC::Root legacy { input() };
    page.did_request_file_picker(GC::Weak { *legacy }, {}, Web::HTML::AllowMultipleFiles::No);
    auto plain = files();
    page.file_picker_closed(plain);
    TRY(require(Requests::pending_count() == 0 && legacy->files()->length() == 1
        && Origins::lookup(document, *legacy->files()->item(0)).is_error()));
    outln("PASS native picker tickets: real Page request/delivery/target/path/no replay/foreign "
          "page/doc/cancel/inactivity/replacement/invalid payload/legacy");
    return {};
}
