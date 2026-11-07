/** Authenticate canonical IPC senders and reserve/release native request budgets. */
#include "Impl.h"
#include "core/EventLoop.h"
#include "serialization/BridgeData.h"
#include <AK/JsonObject.h>
#include <LibCore/Promise.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentPage.h>
#include <QPointer>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
// Navigation/crash invalidates both queued replies and native budget slots.
void Bridge::Impl::reset_ipc(int id)
{
    auto found = windows.find(id);
    if (found == windows.end())
        return;
    auto& window = found->second;
    ipc_pending_count -= window.ipc_pending.size();
    window.ipc_pending.clear();
    window.ipc_document = {};
    window.ipc_realm = {};
    window.ipc_last = 0;
}

ByteString Bridge::Impl::active_ipc_document(State const& window)
{
    auto const& doc = window.view->page().traversable().active_document();
    return ByteString::formatted("{}:{}", doc.id().namespace_id, doc.id().local_id);
}

bool Bridge::Impl::ipc_current(int id, ByteString const& document, ByteString const& realm_id)
{
    auto found = windows.find(id);
    return found != windows.end() && !found->second.closing
        && found->second.view->page().routed_connection() && found->second.ipc_document == document
        && found->second.ipc_realm == realm_id && active_ipc_document(found->second) == document;
}

void Bridge::Impl::dispatch_ipc(int id, ByteString document, ByteString realm_id, ByteString data)
{
    if (stopping || !ipc_current(id, document, realm_id))
        return;
    if (jobs.is_busy()) {
        QTimer::singleShot(1, context.get(),
            [this, id, document = move(document), realm_id = move(realm_id), data = move(data)] {
                dispatch_ipc(id, document, realm_id, data);
            });
        return;
    }

    deliver(id, "ipc-invoke", data);
}

// This sends native data only: it is also safe while the Mite VM is busy.
void Bridge::Impl::send_ipc_reply(State& window, ByteString const& document,
    ByteString const& realm_id, u32 request, bool success, ByteString const& wire)
{
    JsonObject data;
    data.set("document"sv, MUST(String::from_utf8(document.view())));
    data.set("realm"sv, MUST(String::from_utf8(realm_id.view())));
    data.set("request"sv, request);
    data.set("success"sv, success);
    data.set("wire"sv, MUST(String::from_utf8(wire.view())));
    auto& page = window.view->page();
    page.async_run_webdriver_bidi_command(
        0, page.traversable().id(), "firefly.ipc.reply.v1"sv, move(data));
}

void Bridge::Impl::receive_ipc(
    WebView::WebContentPage& page, Web::HTML::CrossProcessId navigable, JsonValue params)
{
    if (stopping || !params.is_object() || params.as_object().size() != 4)
        return;
    auto const& data = params.as_object();
    auto document = data.get_string("document"sv), realm_id = data.get_string("realm"sv),
         wire = data.get_string("wire"sv);
    auto request = data.get_u32("request"sv);
    if (!document.has_value() || document->bytes().size() > 64 || !realm_id.has_value()
        || realm_id->is_empty() || realm_id->bytes().size() > 1024 || !wire.has_value()
        || wire->bytes().size() > 128 * 1024 || !request.has_value() || *request == 0)
        return;
    auto canonical = page.hosted_navigable(navigable);
    if (!canonical.has_value() || !canonical->is_top_level_traversable()
        || !page.hosts_active_document_of(*canonical))
        return;
    auto const& actual = canonical->active_document();
    auto actual_key
        = ByteString::formatted("{}:{}", actual.id().namespace_id, actual.id().local_id);
    if (document->to_byte_string() != actual_key)
        return;
    for (auto& [id, window] : windows) {
        if (window.closing || !window.preload.has_value() || &page.view() != window.view)
            continue;
        if (active_ipc_document(window) != actual_key)
            return;
        auto caller = realm_id->to_byte_string();
        if (window.ipc_document != actual_key) {
            ipc_pending_count -= window.ipc_pending.size();
            window.ipc_pending.clear();
            window.ipc_document = actual_key;
            window.ipc_realm = caller;
            window.ipc_last = 0;
        }

        if (window.ipc_realm != caller || *request <= window.ipc_last)
            return;
        window.ipc_last = *request;
        if (window.ipc_pending.size() >= 16 || ipc_pending_count >= 64) {
            // Refuse an authenticated request promptly without reserving another
            // main-process slot. Never run an application handler over budget.
            auto wire = MUST(
                BridgeData::from_string(u"IPC pending request budget exceeded"sv).encode_wire({}));
            send_ipc_reply(window, actual_key, caller, *request, false, wire);
            return;
        }

        window.ipc_pending.insert(*request);
        ++ipc_pending_count;
        // History API updates are enqueued natively before the IPC envelope
        // arrives, but their canonical URL need not have been applied yet.
        // Sample metadata after those history steps, never from page data or
        // by guessing how many Qt event-loop turns will be sufficient.
        page.traversable().append_history_queue_steps(
            [this, lifetime = QPointer<QObject>(context.get()), id, document = actual_key,
                realm_id = caller,
                params = move(params)](NonnullRefPtr<Core::Promise<Empty>> completion) mutable {
                if (lifetime && !stopping && ipc_current(id, document, realm_id)) {
                    auto& window = windows.at(id);
                    params.as_object().set("url"sv,
                        window.view->page().traversable().active_document_url().serialize());
                    auto encoded = params.serialized().to_byte_string();
                    QTimer::singleShot(0, context.get(),
                        [this, id, document = move(document), realm_id = move(realm_id),
                            data = move(encoded)] { dispatch_ipc(id, document, realm_id, data); });
                }
                // Releasing the queue must not wait for application JS or a
                // handler reply, including after navigation/host destruction.
                completion->resolve({});
            });
        return;
    }
}
}
