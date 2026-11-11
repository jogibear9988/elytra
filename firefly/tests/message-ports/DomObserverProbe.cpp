/** Real SDK port queues/transfer/GC observations; no GUI, JS listeners, or synthetic dequeue. */
#include "ports/Observer.h"
#include "worlds/IsolatedWorld.h"
#include <AK/ScopeGuard.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Heap.h>
#include <LibGC/Testing.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/VM.h>
#include <LibMain/Main.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/MessageEvent.h>
#include <LibWeb/HTML/MessagePort.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <chrono>

namespace H = Web::HTML;
namespace O = Firefly::PortObservation;

namespace {
ErrorOr<void> require(bool value, StringView label)
{
    if (!value) {
        warnln("FAIL native DOM port observer: {}", label);
        return Error::from_string_literal("Native DOM port observer failed");
    }

    return {};
}

GC::Root<Web::DOM::Document> document()
{
    GC::Root realm { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*realm);
    auto root = H::LocalTraversableNavigable::create_a_new_top_level_traversable(page, nullptr, {});
    page.set_top_level_traversable(root);
    return GC::Root { *root->active_document() };
}

struct Authority {
    Web::DOM::Document* document;
    JS::Realm* source;
    JS::Realm* principal;
};

bool validate(void* context, H::MessagePort&, JS::Realm& realm)
{
    auto& authority = *static_cast<Authority*>(context);
    return authority.document->is_fully_active()
        && (&realm == authority.source || &realm == authority.principal)
        && Web::Bindings::principal_host_defined_environment_settings_object(realm)
               .responsible_document()
               .ptr()
        == authority.document;
}

ErrorOr<void> post(H::MessagePort& port, JS::Realm& realm, int number)
{
    H::TemporaryExecutionContext execution { realm,
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    GC::RootVector<GC::Ref<JS::Object>> transfer;
    return require(!port.post_message(realm, JS::Value(number), transfer).is_exception(),
        "actual post_message admitted"sv);
}

struct Delivery {
    std::shared_ptr<O::State> closing;
    size_t count {};
    bool early_close {};
    bool wrong_data {};
};

class Target final : public Web::DOM::EventTarget {
    GC_CELL(Target, Web::DOM::EventTarget);
    GC_DECLARE_ALLOCATOR(Target);

public:
    explicit Target(Delivery& delivery)
        : m_delivery(delivery)
    {
    }

    virtual bool dispatch_event(Web::DOM::Event& event) override
    {
        m_delivery.early_close |= m_delivery.closing->closed.load();
        if (!is<H::MessageEvent>(event)) {
            m_delivery.wrong_data = true;
        } else {
            auto value = static_cast<H::MessageEvent&>(event).data();
            m_delivery.wrong_data |= !value.is_number() || value.as_double() != 4242;
        }

        ++m_delivery.count;
        return true;
    }

private:
    Delivery& m_delivery;
};

GC_DEFINE_ALLOCATOR(Target);

struct Shipped {
    H::MessagePort::TransferredToWorker transfer;
    GC::Weak<H::MessagePort> old;
};

[[gnu::noinline]] ErrorOr<Shipped> ship(
    JS::Realm& source, H::MessagePort& forwarder, std::shared_ptr<O::State> const& observation)
{
    H::TemporaryExecutionContext execution { source,
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    GC::Root receiver { H::MessagePort::create(source) };
    forwarder.entangle_with(*receiver);
    TRY(require(O::observe(*receiver, observation, true), "real source receiver admission"sv));
    for (int i = 0; i < 16; ++i)
        TRY(post(forwarder, source, i));
    TRY(require(observation->dequeued.load() == 0, "unstarted receiver retains all16 messages"sv));
    auto transfer
        = receiver->transfer_to_worker(source, H::MessagePort::WorkerMayBeInOtherProcess::No);
    TRY(require(!transfer.is_exception(), "actual SDK transfer steps"sv));
    return Shipped { transfer.release_value(), *receiver };
}

template <typename Predicate> bool pump(Core::EventLoop& loop, Predicate predicate)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline)
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
    return predicate();
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    auto doc = document();
    auto foreign = document();
    auto source = TRY(Firefly::IsolatedWorld::create(*doc->window()));
    auto& realm = doc->window()->principal_realm();
    auto& vm = realm.vm();
    Authority authority { doc.ptr(), source->realm(), &realm };
    auto observed = O::create(vm, &authority, validate);
    auto closing = O::create(vm, &authority, validate);
    TRY(require(observed && closing, "native observer records"sv));
    ScopeGuard revoke([&] {
        O::revoke(observed);
        O::revoke(closing);
        source->detach();
    });
    Delivery delivery { closing };
    GC::Root forwarder { H::MessagePort::create(*source->realm()) };
    GC::Root target { vm.heap().allocate<Target>(delivery) };
    forwarder->set_message_event_target(*target);
    TRY(require(O::observe(*forwarder, closing, false), "real retained forwarder admission"sv));
    forwarder->enable();
    auto shipped = TRY(ship(*source->realm(), *forwarder, observed));
    ScopeGuard parked(
        [&] { H::MessagePort::give_up_on_parked_end(shipped.transfer.parked_end_token); });
    for (size_t i = 0; i < 4; ++i) {
        GC::scrub_stack_for_testing();
        vm.heap().collect_garbage();
    }

    TRY(require(!shipped.old && !observed->closed.load() && !closing->closed.load(),
        "old port GC does not close moved observer"sv));
    GC::Root receiver { [&] {
        H::TemporaryExecutionContext execution { realm,
            H::TemporaryExecutionContext::CallbacksEnabled::Yes };
        return H::MessagePort::create_from_transfer(realm, move(shipped.transfer.data_holder));
    }() };
    TRY(require(observed->dequeued.load() == 0 && !observed->closed.load(),
        "real same-document source/principal transfer preserves pending credit"sv));
    receiver->enable(); // No JS/native listener: consumption belongs to the real queue task.
    TRY(require(pump(loop, [&] { return observed->dequeued.load() == 16; }),
        "16 real no-listener dequeues"sv));
    TRY(post(*receiver, realm, 4242));
    receiver->close();
    TRY(require(pump(loop, [&] { return closing->closed.load(); }),
        "actual peer close reaches retained forwarder"sv));
    TRY(require(delivery.count == 1 && !delivery.early_close && !delivery.wrong_data,
        "last actual message dispatch precedes forwarder closed"sv));
    // Also exercise the observed same-agent close path without a transfer/socket.
    auto direct_observed = O::create(vm, &authority, validate);
    auto direct_closing = O::create(vm, &authority, validate);
    TRY(require(direct_observed && direct_closing, "direct native observations"sv));
    ScopeGuard direct_revoke([&] {
        O::revoke(direct_observed);
        O::revoke(direct_closing);
    });
    Delivery direct_delivery { direct_closing };
    GC::Root direct_forwarder { H::MessagePort::create(realm) };
    GC::Root direct_receiver { H::MessagePort::create(realm) };
    GC::Root direct_target { vm.heap().allocate<Target>(direct_delivery) };
    direct_forwarder->entangle_with(*direct_receiver);
    direct_forwarder->set_message_event_target(*direct_target);
    TRY(require(O::observe(*direct_receiver, direct_observed, true)
            && O::observe(*direct_forwarder, direct_closing, false),
        "direct native admission"sv));
    direct_forwarder->enable();
    TRY(post(*direct_receiver, realm, 4242));
    direct_receiver->close();
    TRY(require(!direct_closing->closed.load(), "same-agent close waits for the queued message"sv));
    TRY(require(pump(loop, [&] { return direct_closing->closed.load(); })
            && direct_delivery.count == 1 && !direct_delivery.early_close
            && !direct_delivery.wrong_data,
        "same-agent final dispatch precedes observer close"sv));
    direct_forwarder->close();
    auto refused = O::create(vm, &authority, validate);
    GC::Root other { H::MessagePort::create(foreign->window()->principal_realm()) };
    TRY(require(refused && !O::observe(*other, refused, true) && refused->closed.load(),
        "cross-document native binding refused"sv));
    O::revoke(refused);
    auto retired = O::create(vm, &authority, validate);
    O::revoke(retired);
    TRY(require(!O::observe(*forwarder, retired, false), "revoked observer cannot be rebound"sv));
    forwarder->close();
    outln("PASS real SDK port unstarted credit, transfer/old-GC, no-listener dequeue, ordered peer "
          "close and binding revocation");
    return 0;
}
