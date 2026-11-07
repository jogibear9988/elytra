/** Default-session protocol hooks belong to the main bridge's window/VM lifetime. */
#include "Impl.h"
#include "TransportHooks.h"
#include "core/EventLoop.h"
#include <LibWebView/CanonicalTraversable.h>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
void Bridge::Impl::initialize_protocols()
{
    protocols = adopt_ref(*new Protocols::FileRoutes(
        [this](auto root) {
            if (stopping)
                return false;
            for (auto const& [id, state] : windows)
                if (!state.closing && state.view->has_traversable()
                    && state.view->traversable().id() == root)
                    return true;
            return false;
        },
        [this](auto token, auto type, auto data) {
            QTimer::singleShot(
                0, context.get(), [this, token, type = move(type), data = move(data)] {
                    deliver_protocol(token, type, data);
                });
        },
        [this](auto root, auto const& navigation, auto const& url) {
            for (auto& [id, state] : windows) {
                if (state.closing || !state.pending || !state.view->has_traversable()
                    || state.view->traversable().id() != root || !state.requested_url.has_value()
                    || !state.requested_url->equals(url, URL::ExcludeFragment::Yes))
                    continue;
                auto current = state.view->traversable().ongoing_navigation_id();
                if (current.has_value() && *current != navigation)
                    continue;
                state.pending = false;
                state.failed = true;
                enqueue(id, "failed", "Custom protocol file request failed");
            }
        }));
    auto weak_protocols = protocols->make_weak_ptr();
    set_native_navigation_handler([weak_protocols](auto& page, auto const& request) {
        if (auto routes = weak_protocols.strong_ref())
            if (auto result = routes->navigation(page, request); result.is_error())
                warnln("Native file navigation grant: {}", result.error());
    });
    set_native_document_handler(
        [weak_protocols](auto& page, auto const& request, auto const& result) {
            if (auto routes = weak_protocols.strong_ref())
                if (auto granted = routes->document(page, request, result); granted.is_error())
                    warnln("Native file document grant: {}", granted.error());
        });
    protocol_timer
        = Core::Timer::create_repeating(AK::Duration::from_milliseconds(250), [weak_protocols] {
              if (auto routes = weak_protocols.strong_ref())
                  routes->sweep();
          });
    protocol_timer->start();
}

void Bridge::Impl::deliver_protocol(u64 token, ByteString type, ByteString data)
{
    if (stopping)
        return;
    bool request = type == "protocol-request"sv;
    if (request && !protocols->is_pending(token))
        return; // Cancellation must retire queued work even while another VM frame is busy.
    if (jobs.is_busy()) {
        QTimer::singleShot(1, context.get(), [this, token, type = move(type), data = move(data)] {
            deliver_protocol(token, type, data);
        });
        return;
    }

    if (request && !protocols->mark_delivered(token))
        return;
    deliver(0, type, data);
}

void Bridge::Impl::stop_protocols()
{
    set_native_navigation_handler({});
    set_native_document_handler({});
    if (protocol_timer)
        protocol_timer->stop();
    if (protocols)
        protocols->stop();
}
}
