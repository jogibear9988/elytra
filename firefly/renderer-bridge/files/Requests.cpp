/** Bind native results to the original live input and consume before invoking engine callbacks. */
#include "Requests.h"
#include "Origins.h"
#include "native/file-picker/Limits.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/NeverDestroyed.h>
#include <AK/NumericLimits.h>
#include <AK/RefPtr.h>
#include <AK/ScopeGuard.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/Page/Page.h>

namespace Firefly::FileRequests {
namespace {
    struct Callback : RefCounted<Callback> {
        explicit Callback(Sender value)
            : send(move(value))
        {
        }

        Sender send;
    };

    struct Request {
        GC::Weak<Web::Page> page;
        GC::Weak<Web::HTML::HTMLInputElement> input;
        GC::Weak<Web::DOM::Document> document;
        Ticket ticket;
        bool multiple { false };
    };

    struct Delivery {
        Web::HTML::HTMLInputElement& input;
        Span<Web::HTML::SelectedFile> files;
    };

    struct State {
        RefPtr<Callback> sender;
        Vector<Request> pending;
        u64 next { 0 };
        Delivery* delivery { nullptr };
    };

    NeverDestroyed<State> state;

    void sweep()
    {
        state.get().pending.remove_all_matching([](auto const& value) {
            return !value.page || !value.input || !value.document
                || !value.document->is_fully_active()
                || &value.input->document() != value.document.ptr().ptr()
                || &value.document->page() != value.page.ptr().ptr();
        });
    }

    bool valid_payload(JsonValue const& metadata, Ticket ticket,
        Vector<Web::HTML::SelectedFile> const& files, bool multiple)
    {
        if (!metadata.is_object())
            return false;
        auto documents = metadata.as_object().get_array("documents"sv);
        if (!documents.has_value() || documents->size() != 1 || !(*documents)[0].is_string())
            return false;
        auto identity = ByteString::formatted(
            "{}:{}", ticket.document.namespace_id, ticket.document.local_id);
        if ((*documents)[0].as_string() != identity.view()
            || files.size() > FileSelectionPolicy::max_files || (!multiple && files.size() > 1))
            return false;
        size_t total = 0;
        for (auto const& file : files) {
            if (!file.file_or_contents().has<ByteBuffer>())
                return false;
            auto bytes = file.file_or_contents().get<ByteBuffer>().size();
            if (bytes > FileSelectionPolicy::max_file_bytes
                || bytes > FileSelectionPolicy::max_total_bytes - total)
                return false;
            total += bytes;
        }

        return true;
    }
}

void set_sender(Sender sender)
{
    // Monotonic serials survive service replacement, so old replies cannot match
    // a newly configured sender even in the same helper process.
    state.get().sender = sender ? adopt_ref(*new Callback(move(sender))) : RefPtr<Callback> {};
    state.get().pending.clear();
}

bool requested(
    Web::Page& page, Web::HTML::HTMLInputElement& input, Web::HTML::AllowMultipleFiles mode)
{
    sweep();
    auto callback = state.get().sender;
    if (!callback)
        return true;
    GC::Root retained_page { page };
    GC::Root retained_input { input };
    GC::Root document { input.document() };
    if (!document->is_fully_active() || &document->page() != &page)
        return false;
    auto& data = state.get();
    closed(page);
    if (data.pending.size() >= 64 || data.next == NumericLimits<u64>::max())
        return false;
    Ticket ticket { ++data.next, document->id() };
    data.pending.append(
        { page, input, *document, ticket, mode == Web::HTML::AllowMultipleFiles::Yes });
    callback->send(page, ticket);
    return true;
}

void closed(Web::Page& page)
{
    state.get().pending.remove_all_matching(
        [&](auto const& request) { return request.page.ptr().ptr() == &page; });
}

bool allows(Web::HTML::HTMLInputElement& input, Span<Web::HTML::SelectedFile> files)
{
    auto* delivery = state.get().delivery;
    return !delivery
        || (&delivery->input == &input && input.document().is_fully_active()
            && delivery->files.data() == files.data() && delivery->files.size() == files.size());
}

bool deliver(Web::Page& page, Ticket ticket, JsonValue const& metadata,
    Vector<Web::HTML::SelectedFile>& files)
{
    sweep();
    auto& data = state.get();
    if (data.delivery)
        return false;
    Optional<Request> request;
    for (size_t i = 0; i < data.pending.size(); ++i) {
        auto const& candidate = data.pending[i];
        if (candidate.page.ptr().ptr() != &page || candidate.ticket.serial != ticket.serial
            || candidate.ticket.document != ticket.document)
            continue;
        request = data.pending.take(i);
        break;
    }

    if (!request.has_value())
        return false;
    GC::Root retained_page { page };
    GC::Root input { request->input.ptr() };
    GC::Root document { request->document.ptr() };
    if (!input || !document || !document->is_fully_active() || &input->document() != document.ptr())
        return false;
    auto apply = [&](Span<Web::HTML::SelectedFile> selected) {
        Delivery delivery { *input, selected };
        data.delivery = &delivery;
        ScopeGuard retire([&] { data.delivery = nullptr; });
        page.file_picker_closed(selected);
    };

    if (!valid_payload(metadata, ticket, files, request->multiple)) {
        apply({});
        return true;
    }

    auto scope = FileOrigins::Selection::begin(metadata, files);
    if (scope.is_error()) {
        apply({});
        return true;
    }

    apply(files);
    return true;
}

size_t pending_count()
{
    sweep();
    return state.get().pending.size();
}
}
