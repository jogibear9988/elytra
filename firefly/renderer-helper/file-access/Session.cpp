#include "Session.h"

#if __has_include(<LibWeb/FileSystemAccess/Provider.h>)

#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/NumericLimits.h>
#include <LibCore/Timer.h>
#include <LibGC/Weak.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/FileSystemAccess/Provider.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>

namespace Firefly::HelperFileAccess {
namespace {
    using Web::FileSystemAccess::Completion;

    struct Pending {
        Web::PageId page;
        Web::HTML::CrossProcessId document_id;
        GC::Weak<Web::DOM::Document> document;
        WeakPtr<WebContent::ConnectionFromClient> connection;
        Completion complete;
    };

    NeverDestroyed<HashMap<u64, Pending>> pending;
    RefPtr<Core::Timer> sweep_timer;
    u64 next_request = 0;

    JsonObject failure(StringView name)
    {
        JsonObject error;
        error.set("error"sv, name);
        return error;
    }

    bool active(Pending const& request)
    {
        auto document = request.document.ptr();
        auto connection = request.connection.strong_ref();
        return document && document->is_fully_active() && connection && connection->is_open();
    }

    void sweep()
    {
        Vector<u64> retired;
        for (auto const& item : *pending) {
            if (!active(item.value))
                retired.append(item.key);
        }

        for (auto id : retired) {
            if (auto item = pending->take(id); item.has_value())
                item->complete(failure("AbortError"sv), {});
        }
    }

    void send(
        Web::DOM::Document& document, JsonObject metadata, ReadonlyBytes bytes, Completion complete)
    {
        if (!document.is_fully_active() || !document.navigable()) {
            complete(failure("InvalidStateError"sv), {});
            return;
        }

        auto* page = as_if<WebContent::PageClient>(document.page().client());
        if (!page || !page->file_access_connection().is_open()) {
            complete(failure("NotSupportedError"sv), {});
            return;
        }

        sweep();
        if (pending->size() >= 128 || next_request == NumericLimits<u64>::max()) {
            complete(failure("QuotaExceededError"sv), {});
            return;
        }

        auto buffer = ByteBuffer::copy(bytes);
        if (buffer.is_error()) {
            complete(failure("QuotaExceededError"sv), {});
            return;
        }

        auto& connection = page->file_access_connection();
        auto id = document.id();
        auto request = ++next_request;
        pending->set(request,
            { page->id(), id, document,
                connection.make_weak_ptr<WebContent::ConnectionFromClient>(), move(complete) });

        auto posted = connection.post_message(
            Messages::WebContentClient::FireflyFileAccessRequest(page->id().value(),
                id.namespace_id, id.local_id, request, move(metadata), buffer.release_value()));
        if (posted.is_error()) {
            if (auto item = pending->take(request); item.has_value())
                item->complete(failure("AbortError"sv), {});
        }
    }

    void receive(IPC::Stub& stub, Bytes::FileAccessMessage message)
    {
        auto& connection = static_cast<WebContent::ConnectionFromClient&>(
            static_cast<WebContentServerStub&>(stub));
        auto found = pending->find(message.request);
        Web::HTML::CrossProcessId id { message.document_namespace, message.document_local };
        if (found == pending->end() || found->value.connection.ptr() != &connection
            || found->value.page.value() != message.page || found->value.document_id != id)
            return;

        auto entry = pending->take(message.request).release_value();
        if (!active(entry) || !message.metadata.is_object()) {
            entry.complete(failure("AbortError"sv), {});
            return;
        }

        entry.complete(move(message.metadata.as_object()), move(message.bytes));
    }
}
}
#endif

namespace Firefly::HelperFileAccess {

void initialize()
{
#if __has_include(<LibWeb/FileSystemAccess/Provider.h>)
    Bytes::set_file_access_reply_receiver(receive);
    Web::FileSystemAccess::set_provider(send);
    sweep_timer = Core::Timer::create_repeating(AK::Duration::from_seconds(1), sweep);
    sweep_timer->start();
#endif
}

void shutdown()
{
#if __has_include(<LibWeb/FileSystemAccess/Provider.h>)
    Web::FileSystemAccess::set_provider(nullptr);
    Bytes::set_file_access_reply_receiver({});
    sweep_timer->stop();
    sweep_timer = nullptr;
    pending->clear();
#endif
}

}
