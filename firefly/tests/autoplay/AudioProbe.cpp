/** Real Rust WebAudio graph/lifecycle with SDK null outputs; no OS audio or GUI. */
#include "autoplay/Policy.h"
#include <AK/Queue.h>
#include <AK/ScopeGuard.h>
#include <LibCore/AnonymousBuffer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Timer.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibGC/Testing.h>
#include <LibGC/Weak.h>
#include <LibGfx/Palette.h>
#include <LibGfx/SystemTheme.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibMain/Main.h>
#include <LibMedia/Sinks/AudioOutputFactory.h>
#include <LibURL/Parser.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/BrowsingContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/RemoteNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/WindowProxy.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWebCommon/HTML/SessionHistoryEntryDescriptor.h>

namespace A = Firefly::Autoplay;
namespace H = Web::HTML;

namespace {
// Implement the real PageClient contract, following the SDK's headless client.
// Its only output override returns the genuine wall-clock-paced null stream.
class NullAudioClient final : public Web::PageClient {
    GC_CELL(NullAudioClient, Web::PageClient);
    GC_DECLARE_ALLOCATOR(NullAudioClient);

public:
    static GC::Ref<NullAudioClient> create(u64 namespace_id)
    {
        return GC::Heap::the().allocate<NullAudioClient>(namespace_id);
    }

    virtual Web::PageId id() const override
    {
        return Web::PageId { m_ids.namespace_id };
    }

    virtual H::CrossProcessId allocate_cross_process_id() override
    {
        return m_ids.allocate();
    }

    virtual Web::Page& page() override
    {
        return *m_page;
    }

    virtual Web::Page const& page() const override
    {
        return *m_page;
    }

    virtual bool is_connection_open() const override
    {
        return false;
    }

    virtual Gfx::Palette palette() const override
    {
        return Gfx::Palette(*m_palette);
    }

    virtual Web::DevicePixelRect screen_rect() const override
    {
        return {};
    }

    virtual double zoom_level() const override
    {
        return 1.0;
    }

    virtual double device_pixel_ratio() const override
    {
        return 1.0;
    }

    virtual double device_pixels_per_css_pixel() const override
    {
        return 1.0;
    }

    virtual Web::CSS::PreferredColorScheme preferred_color_scheme() const override
    {
        return Web::CSS::PreferredColorScheme::Auto;
    }

    virtual Web::CSS::PreferredContrast preferred_contrast() const override
    {
        return Web::CSS::PreferredContrast::NoPreference;
    }

    virtual Web::CSS::PreferredMotion preferred_motion() const override
    {
        return Web::CSS::PreferredMotion::NoPreference;
    }

    virtual size_t screen_count() const override
    {
        return 1;
    }

    virtual Queue<Web::QueuedInputEvent>& input_event_queue() override
    {
        return m_input;
    }

    virtual void report_finished_handling_input_event(Web::PageId, u64, Web::EventResult) override
    {
    }

    virtual void request_frame() override
    {
    }

    virtual void request_file(Web::FileRequest) override
    {
        VERIFY_NOT_REACHED();
    }

    virtual bool is_headless() const override
    {
        return true;
    }

    virtual NonnullRefPtr<Media::AudioOutputFactory> create_audio_output_factory() override
    {
        ++factories;
        return Media::AudioOutputFactory::create_null();
    }

    GC::Ptr<Web::Page> m_page;
    size_t factories { 0 };

private:
    explicit NullAudioClient(u64 namespace_id)
        : m_ids { .namespace_id = namespace_id }
    {
        auto buffer = MUST(Core::AnonymousBuffer::create_with_size(sizeof(Gfx::SystemTheme)));
        m_palette = Gfx::PaletteImpl::create_with_anonymous_buffer(buffer);
    }

    virtual void visit_edges(Cell::Visitor& visitor) override
    {
        Base::visit_edges(visitor);
        visitor.visit(m_page);
    }

    RefPtr<Gfx::PaletteImpl> m_palette;
    Queue<Web::QueuedInputEvent> m_input;
    H::CrossProcessIdAllocator m_ids;
};

GC_DEFINE_ALLOCATOR(NullAudioClient);

enum class Placement { Top, SameOriginChild, CrossOriginChild };

struct Context {
    GC::Root<NullAudioClient> client;
    GC::Root<Web::DOM::Document> document;
    String key;

    JS::Realm& realm() const
    {
        return document->window()->principal_realm();
    }
};

Context create_context(Placement placement)
{
    static u64 next_namespace = 800;
    auto namespace_id = ++next_namespace;
    GC::Root client { NullAudioClient::create(namespace_id) };
    GC::Root page { Web::Page::create(*client) };
    client->m_page = page;
    auto top_url = URL::Parser::basic_parse("https://audio-parent.example/"sv).release_value();
    auto child_url = placement == Placement::CrossOriginChild
        ? URL::Parser::basic_parse("https://audio-child.example/frame"sv).release_value()
        : URL::Parser::basic_parse("https://audio-parent.example/frame"sv).release_value();
    GC::Root<Web::DOM::Document> document;
    H::CrossProcessId root_id;
    if (placement == Placement::Top) {
        auto root
            = H::LocalTraversableNavigable::create_a_new_top_level_traversable(*page, nullptr, {});
        page->set_top_level_traversable(root);
        root_id = root->id();
        document = root->active_document();
        child_url = top_url;
    } else {
        root_id = client->allocate_cross_process_id();
        auto child_id = client->allocate_cross_process_id();
        H::ReplicatedNavigableState state {
            .target_name = {},
            .active_document_url = top_url,
            .active_document_origin = top_url.origin(),
            .active_document_is_fully_active = true,
            .top_level_creation_url = top_url,
            .top_level_origin = top_url.origin(),
            .browsing_context_group_id = {},
            .opener_policy = {},
            .container = {},
            .has_session_history_entry_and_ready_for_navigation = true,
            .compositor_context_id = {},
        };
        Vector<H::RemoteNavigableDescriptor> graph;
        graph.append({ root_id, {}, state });
        state.active_document_url = child_url;
        state.active_document_origin = child_url.origin();
        graph.append({ child_id, root_id, state });
        page->create_remote_navigable_graph(move(graph));
        auto history = H::create_initial_session_history_entry_descriptor(
            client->allocate_cross_process_id(), child_url, {});
        auto child = page->begin_hosting(child_id, history);
        page->adopt_hosted(*child);
        document = child->active_document();
        document->browsing_context()->window_proxy()->set_window(*document->window());
    }
    document->set_url(child_url);
    document->set_origin(child_url.origin());
    auto& settings = document->relevant_settings_object();
    settings.creation_url = child_url;
    settings.top_level_creation_url = top_url;
    settings.top_level_origin = top_url.origin();
    return { move(client), move(document),
        MUST(String::formatted("{}-{}", root_id.namespace_id, root_id.local_id)) };
}

ErrorOr<JS::Value> run(JS::Realm& realm, Utf16View source)
{
    H::TemporaryExecutionContext execution { realm,
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error())
        return Error::from_string_literal("Invalid native WebAudio fixture script");
    GC::Root script { parsed.release_value() };
    auto result = realm.vm().run(*script);
    if (result.is_error()) {
        warnln("WebAudio script exception: {}",
            result.error_value().to_utf16_string_without_side_effects());
        return Error::from_string_literal("Native WebAudio script threw");
    }
    return result.release_value();
}

ErrorOr<void> require(bool value, StringView label)
{
    if (!value) {
        warnln("FAIL native WebAudio: {}", label);
        return Error::from_string_literal("Native WebAudio assertion failed");
    }
    return {};
}

ErrorOr<void> check(Context const& context, Utf16View source, StringView label)
{
    auto result = TRY(run(context.realm(), source));
    return require(result.is_boolean() && result.as_bool(), label);
}

ErrorOr<void> wait(Context const& context, Utf16View condition, StringView label)
{
    auto deadline = Core::Timer::create_single_shot(AK::Duration::from_seconds(8), [] { });
    deadline->start();
    auto text = Utf16String::from_utf16(condition);
    H::main_thread_event_loop().spin_until(GC::create_function(context.realm().heap(),
        GC::captures(context.realm(), text, deadline->make_weak_ptr<Core::Timer>()),
        [](auto& realm, auto& text, auto& deadline) {
            auto result = run(realm, text);
            return result.is_error() || (result.value().is_boolean() && result.value().as_bool())
                || !deadline || !deadline->is_active();
        }));
    return check(context, condition, label);
}

ErrorOr<void> turn(Context const& context)
{
    TRY(run(
        context.realm(), u"globalThis.timerDone=false;setTimeout(()=>{timerDone=true},20);true"sv));
    return wait(context, u"timerDone"sv, "real timer/media task turn completes"sv);
}

void activate(Context const& context)
{
    H::TemporaryExecutionContext execution { context.realm(),
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    context.document->window()->notify_about_user_activation();
}

void consume(Context const& context)
{
    H::TemporaryExecutionContext execution { context.realm(),
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    context.document->window()->consume_user_activation();
}

ErrorOr<void> policy(Context const& context, A::Policy value, u32 generation = 1)
{
    Vector<A::Configuration> snapshot;
    snapshot.append({ context.key, generation, value });
    TRY(require(A::context_for(*context.document) == context.key, "actual native context key"sv));
    return A::apply(move(snapshot));
}

ErrorOr<void> close(Context const& context)
{
    TRY(run(context.realm(),
        u"globalThis.closeDone=false;ctx.close().then(()=>{closeDone=true});true"sv));
    return wait(context, u"closeDone && ctx.state==='closed'"sv, "real null graph closes"sv);
}

ErrorOr<void> invalid_state_promise(Context const& context, Utf16View expression)
{
    auto value = TRY(run(context.realm(), expression));
    if (!value.is_object() || !is<JS::Promise>(value.as_object()))
        return Error::from_string_literal("WebAudio method did not return a real Promise");
    auto& promise = static_cast<JS::Promise&>(value.as_object());
    if (promise.state() != JS::Promise::State::Rejected || !promise.result().is_object())
        return Error::from_string_literal("Inactive WebAudio Promise did not reject");
    auto* exception
        = Web::Bindings::impl_from<Web::WebIDL::DOMException>(&promise.result().as_object());
    return require(exception && exception->name() == "InvalidStateError"_utf16,
        "native inactive-owner Promise rejects InvalidStateError"sv);
}

struct WeakPending {
    GC::Weak<JS::Object> context;
    GC::Weak<JS::Object> promise;
};

[[gnu::noinline]] ErrorOr<WeakPending> unreachable_blocked(Context const& context)
{
    auto value = TRY(run(context.realm(),
        u"(()=>{const c=new AudioContext();const p=c.resume();p.catch(()=>{});return [c,p]})()"sv));
    GC::Root array { value.as_object() };
    auto object = array->get("0"_utf16);
    auto promise = array->get("1"_utf16);
    if (object.is_error() || promise.is_error())
        return Error::from_string_literal("Cannot inspect real blocked context references");
    return WeakPending { object.value().as_object(), promise.value().as_object() };
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    (void)Core::EventLoop::initialize_for_current_thread();
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    // Install the SDK's real agent/event-loop infrastructure, not a mocked VM.
    GC::Root bootstrap { Web::Bindings::create_a_simple_javascript_realm() };
    ScopeGuard cleanup([] { A::shutdown(); });
    A::initialize_owned();
    constexpr auto strict = A::Policy::UserGestureRequired;

    auto blocked = create_context(Placement::CrossOriginChild);
    TRY(policy(blocked, strict));
    TRY(check(blocked,
        u"globalThis.ctx=new AudioContext();globalThis.resumes=0;globalThis.resumeError='';ctx.resume().then(()=>resumes++,e=>resumeError=e.name);ctx.state==='suspended'"sv,
        "cross-origin constructor and resume stay suspended"sv));
    TRY(require(
        blocked.client->factories == 0, "blocked constructor/resume acquire no output factory"sv));
    activate(blocked);
    TRY(turn(blocked));
    TRY(check(blocked, u"ctx.state==='suspended' && resumes===0 && resumeError===''"sv,
        "bare native activation does not resume context or settle promise"sv));
    TRY(require(blocked.client->factories == 0, "activation alone does not acquire output"sv));
    consume(blocked);
    TRY(run(blocked.realm(), u"ctx.resume().then(()=>resumes++,e=>resumeError=e.name);true"sv));
    TRY(turn(blocked));
    TRY(require(blocked.client->factories == 0, "sticky-only resume remains blocked"sv));
    activate(blocked);
    TRY(run(blocked.realm(), u"ctx.resume().then(()=>resumes++,e=>resumeError=e.name);true"sv));
    TRY(wait(blocked, u"ctx.state==='running' && resumes===3 && ctx.currentTime>0"sv,
        "transient resume starts genuine null graph and settles all queued promises"sv));
    TRY(require(
        blocked.client->factories == 1, "first authorized resume acquires one output factory"sv));
    consume(blocked);
    outln("PASS WebAudio blocked acquisition, bare activation, transient resume and real "
          "null-graph clock");

    TRY(run(blocked.realm(),
        u"globalThis.suspended=false;ctx.suspend().then(()=>suspended=true);true"sv));
    TRY(wait(
        blocked, u"suspended && ctx.state==='suspended'"sv, "explicit user suspension settles"sv));
    TRY(run(blocked.realm(),
        u"globalThis.source=ctx.createOscillator();source.connect(ctx.destination);source.start();true"sv));
    TRY(turn(blocked));
    TRY(check(blocked, u"ctx.state==='suspended'"sv,
        "source start does not override explicit user suspension"sv));
    TRY(require(
        blocked.client->factories == 1, "suspended source start creates no extra renderer"sv));
    TRY(run(blocked.realm(), u"ctx.resume().then(()=>resumes++);true"sv));
    TRY(wait(blocked, u"ctx.state==='running' && resumes===4"sv,
        "authorized context resumes after gesture has been consumed"sv));
    TRY(close(blocked));
    TRY(run(blocked.realm(),
        u"globalThis.closedResume='';ctx.resume().catch(e=>closedResume=e.name);true"sv));
    TRY(wait(blocked, u"closedResume==='InvalidStateError'"sv, "closed context rejects resume"sv));
    TRY(require(blocked.client->factories == 1, "closed context does not reacquire output"sv));
    outln("PASS WebAudio user suspension, persistent authorization and closed-context lifecycle");

    auto suspended_blocked = create_context(Placement::CrossOriginChild);
    TRY(policy(suspended_blocked, strict));
    TRY(run(suspended_blocked.realm(),
        u"globalThis.ctx=new AudioContext();globalThis.didSuspend=false;ctx.suspend().then(()=>didSuspend=true);true"sv));
    TRY(wait(suspended_blocked, u"didSuspend && ctx.state==='suspended'"sv,
        "explicit suspension of still-blocked context settles"sv));
    TRY(require(suspended_blocked.client->factories == 0,
        "explicit suspension of blocked context acquires no output"sv));
    activate(suspended_blocked);
    TRY(run(suspended_blocked.realm(),
        u"globalThis.source=ctx.createOscillator();source.connect(ctx.destination);source.start();true"sv));
    TRY(turn(suspended_blocked));
    TRY(check(suspended_blocked, u"ctx.state==='suspended'"sv,
        "transient source start respects explicit user suspension"sv));
    TRY(require(suspended_blocked.client->factories == 0,
        "gesture source start does not acquire a user-suspended graph"sv));
    consume(suspended_blocked);
    TRY(run(suspended_blocked.realm(),
        u"globalThis.didResume=false;ctx.resume().then(()=>didResume=true);true"sv));
    TRY(wait(suspended_blocked, u"didResume && ctx.state==='running' && ctx.currentTime>0"sv,
        "source-start gesture authorization persists for later explicit resume"sv));
    TRY(require(suspended_blocked.client->factories == 1,
        "later explicit resume acquires exactly one real null output"sv));
    TRY(close(suspended_blocked));
    outln(
        "PASS blocked user suspension preserves source-start authorization without auto-resuming");

    auto constructor = create_context(Placement::CrossOriginChild);
    TRY(policy(constructor, strict));
    activate(constructor);
    TRY(run(constructor.realm(), u"globalThis.ctx=new AudioContext();true"sv));
    TRY(wait(constructor, u"ctx.state==='running' && ctx.currentTime>0"sv,
        "transient constructor starts real graph"sv));
    TRY(require(constructor.client->factories == 1, "authorized constructor acquires once"sv));
    consume(constructor);
    TRY(close(constructor));
    outln("PASS WebAudio constructor evaluates actual current transient activation");

    auto scheduled = create_context(Placement::CrossOriginChild);
    TRY(policy(scheduled, strict));
    TRY(run(scheduled.realm(),
        u"globalThis.ctx=new AudioContext();globalThis.first=ctx.createOscillator();first.connect(ctx.destination);first.start();true"sv));
    TRY(turn(scheduled));
    TRY(require(
        scheduled.client->factories == 0, "blocked scheduled source does not acquire output"sv));
    activate(scheduled);
    TRY(turn(scheduled));
    TRY(require(
        scheduled.client->factories == 0, "activation alone does not start existing source"sv));
    TRY(run(scheduled.realm(),
        u"globalThis.second=ctx.createOscillator();second.connect(ctx.destination);second.start();true"sv));
    TRY(wait(scheduled, u"ctx.state==='running' && ctx.currentTime>0"sv,
        "scheduled source start under transient activation starts actual graph"sv));
    TRY(require(scheduled.client->factories == 1, "scheduled source starts one null renderer"sv));
    TRY(close(scheduled));
    outln("PASS genuine AudioScheduledSourceNode start admission");

    auto buffer = create_context(Placement::CrossOriginChild);
    TRY(policy(buffer, strict));
    TRY(run(buffer.realm(),
        u"globalThis.ctx=new AudioContext();globalThis.samples=ctx.createBuffer(1,32,ctx.sampleRate);globalThis.first=ctx.createBufferSource();first.buffer=samples;first.connect(ctx.destination);first.start();true"sv));
    TRY(turn(buffer));
    TRY(require(
        buffer.client->factories == 0, "blocked independent buffer start acquires no output"sv));
    activate(buffer);
    TRY(run(buffer.realm(),
        u"globalThis.second=ctx.createBufferSource();second.buffer=samples;second.connect(ctx.destination);second.start();true"sv));
    TRY(wait(buffer, u"ctx.state==='running' && ctx.currentTime>0"sv,
        "independent buffer start under activation starts real graph"sv));
    TRY(require(buffer.client->factories == 1, "buffer source starts one null renderer"sv));
    TRY(close(buffer));
    outln("PASS genuine independent AudioBufferSourceNode start admission");

    for (auto placement :
        { Placement::Top, Placement::SameOriginChild, Placement::CrossOriginChild }) {
        auto allowed = create_context(placement);
        TRY(policy(allowed,
            placement == Placement::CrossOriginChild ? A::Policy::NoUserGestureRequired : strict));
        TRY(run(allowed.realm(), u"globalThis.ctx=new AudioContext();true"sv));
        TRY(wait(allowed, u"ctx.state==='running' && ctx.currentTime>0"sv,
            "top/same-origin/no-gesture policy admits actual graph"sv));
        TRY(require(
            allowed.client->factories == 1, "unrestricted context acquires only its own output"sv));
        TRY(close(allowed));
    }
    outln("PASS WebAudio top-level, same-origin child and no-gesture policy paths");

    auto transitioning = create_context(Placement::CrossOriginChild);
    TRY(policy(transitioning, A::Policy::NoUserGestureRequired));
    TRY(run(transitioning.realm(), u"globalThis.ctx=new AudioContext();true"sv));
    TRY(wait(transitioning, u"ctx.state==='running'"sv, "unrestricted graph initially runs"sv));
    TRY(run(transitioning.realm(),
        u"globalThis.didSuspend=false;ctx.suspend().then(()=>didSuspend=true);true"sv));
    TRY(wait(transitioning, u"didSuspend && ctx.state==='suspended'"sv,
        "suspend before stricter policy"sv));
    TRY(policy(transitioning, strict, 2));
    TRY(run(transitioning.realm(),
        u"globalThis.didResume=false;ctx.resume().then(()=>didResume=true);true"sv));
    TRY(turn(transitioning));
    TRY(check(transitioning, u"ctx.state==='suspended' && !didResume"sv,
        "no-gesture admission is not a reusable strict-policy unlock"sv));
    activate(transitioning);
    TRY(run(transitioning.realm(), u"ctx.resume();true"sv));
    TRY(wait(transitioning, u"ctx.state==='running' && didResume"sv,
        "fresh activation admits stricter policy resume"sv));
    TRY(require(transitioning.client->factories == 1,
        "stricter-policy resume reuses real suspended renderer"sv));
    TRY(close(transitioning));
    outln("PASS real WebAudio no-gesture to strict policy authorization revocation");

    for (bool at_constructor : { true, false }) {
        auto revoked = create_context(Placement::CrossOriginChild);
        TRY(policy(revoked, strict));
        if (at_constructor)
            activate(revoked);
        TRY(run(revoked.realm(),
            u"globalThis.ctx=new AudioContext();globalThis.states=[];ctx.onstatechange=()=>states.push(ctx.state);globalThis.earlierPending=ctx.resume();earlierPending.catch(()=>{});true"sv));
        if (!at_constructor)
            activate(revoked);
        TRY(run(revoked.realm(),
            u"globalThis.didResume=false;globalThis.resumeError='';globalThis.pendingToReject=ctx.resume();pendingToReject.then(()=>didResume=true,e=>resumeError=e.name);true"sv));
        auto pending_value = TRY(run(revoked.realm(), u"pendingToReject"sv));
        TRY(require(pending_value.is_object() && is<JS::Promise>(pending_value.as_object())
                && static_cast<JS::Promise&>(pending_value.as_object()).state()
                    == JS::Promise::State::Pending,
            "admitted start has a genuine still-pending publication Promise"sv));
        TRY(require(revoked.client->factories == 1,
            "admitted graph acquired before publication revocation"sv));
        // No event-loop turn occurs between admission and removing the trusted
        // snapshot. The existing graph was real, but may not publish Running.
        TRY(A::apply({}));
        TRY(wait(revoked, u"resumeError==='InvalidStateError' && ctx.state==='suspended'"sv,
            "queued start fails closed after native policy removal"sv));
        TRY(check(revoked, u"!didResume && !states.includes('running')"sv,
            "revoked queued publication neither resolves nor emits Running"sv));
        TRY(invalid_state_promise(revoked, u"earlierPending"sv));
        TRY(invalid_state_promise(revoked, u"pendingToReject"sv));
        TRY(run(revoked.realm(), u"globalThis.future=ctx.resume();future.catch(()=>{});true"sv));
        TRY(invalid_state_promise(revoked, u"future"sv));
        TRY(require(revoked.client->factories == 1,
            "permanently retired context does not reacquire output"sv));
        TRY(close(revoked));
    }
    outln("PASS actual constructor/resume publication revocation rejects pending starts before "
          "Running");

    auto closing = create_context(Placement::CrossOriginChild);
    TRY(policy(closing, strict));
    TRY(run(closing.realm(),
        u"globalThis.ctx=new AudioContext();globalThis.reason='';ctx.resume().catch(e=>reason=e.name);true"sv));
    TRY(close(closing));
    TRY(wait(closing, u"reason==='InvalidStateError'"sv, "close settles blocked pending resume"sv));
    TRY(require(closing.client->factories == 0, "closing blocked context never acquires output"sv));
    outln("PASS blocked WebAudio close rejects queued resume without acquiring output");

    auto inactive = create_context(Placement::CrossOriginChild);
    TRY(policy(inactive, strict));
    TRY(run(inactive.realm(),
        u"globalThis.ctx=new AudioContext();globalThis.oldPending=ctx.resume();oldPending.catch(()=>{});true"sv));
    inactive.document->navigable()->top_level_traversable()->set_has_been_destroyed();
    TRY(run(inactive.realm(),
        u"globalThis.currentPending=ctx.resume();currentPending.catch(()=>{});true"sv));
    // An inactive document need not execute its author callbacks. Inspect the
    // actual settled Promises instead of requiring callbacks in a retired realm.
    TRY(invalid_state_promise(inactive, u"oldPending"sv));
    TRY(invalid_state_promise(inactive, u"currentPending"sv));
    TRY(require(inactive.client->factories == 0, "inactive document never acquires output"sv));
    outln("PASS actual native document inactivation rejects blocked pending resumes");

    auto collecting = create_context(Placement::CrossOriginChild);
    TRY(policy(collecting, strict));
    auto weak = TRY(unreachable_blocked(collecting));
    for (size_t attempt = 0; attempt < 4; ++attempt) {
        GC::scrub_stack_for_testing();
        collecting.realm().vm().heap().collect_garbage();
    }
    TRY(require(!weak.context && !weak.promise,
        "actual blocked Rust context and queued promise are reclaimable"sv));
    TRY(require(
        collecting.client->factories == 0, "unreachable blocked context never acquires output"sv));
    outln("PASS actual Rust WebAudio blocked-context and pending-promise GC");
    return 0;
}
