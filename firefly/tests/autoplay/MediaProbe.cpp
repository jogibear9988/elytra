/** Real LibWeb media admission, without a GUI, a playable source, or an audio device. */
#include "autoplay/Policy.h"
#include <AK/ScopeGuard.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibGC/Testing.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/AutoplaySettings.h>
#include <LibWeb/HTML/BrowsingContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/HTMLMediaElement.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/RemoteNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/WindowProxy.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWebCommon/HTML/SessionHistoryEntryDescriptor.h>

namespace A = Firefly::Autoplay;
namespace H = Web::HTML;

namespace {
struct Context {
    GC::Root<Web::DOM::Document> document;
    String handle;

    JS::Realm& realm() const
    {
        return document->window()->principal_realm();
    }
};

Context create_context(StringView location, GC::Ptr<H::BrowsingContext> opener = nullptr)
{
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto traversable
        = H::LocalTraversableNavigable::create_a_new_top_level_traversable(page, opener, {});
    // The headless PageClient normally reuses its local allocator namespace.
    // Assign an actual SDK native ID as canonical UI history reconstruction does;
    // deliberately retain a different BiDi window handle to detect UUID lookups.
    static u64 next_context = 0;
    auto id = H::CrossProcessId { 710, ++next_context };
    traversable->set_id_for_session_history_reconstruction(id);
    traversable->set_window_handle(
        Utf16String::formatted("different-bidi-window-{}", next_context));
    page.set_top_level_traversable(traversable);
    GC::Root document { *traversable->active_document() };
    auto parsed_url = URL::Parser::basic_parse(location);
    VERIFY(parsed_url.has_value());
    auto url = parsed_url.release_value();
    document->set_url(url);
    document->set_origin(url.origin());
    auto& settings = document->relevant_settings_object();
    settings.creation_url = url;
    settings.top_level_creation_url = url;
    settings.top_level_origin = url.origin();
    return { move(document), MUST(String::formatted("{}-{}", id.namespace_id, id.local_id)) };
}

Context create_remote_child_context()
{
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto url = URL::Parser::basic_parse("https://remote-parent.example/child"sv).release_value();
    H::ReplicatedNavigableState state {
        .target_name = {},
        .active_document_url = url,
        .active_document_origin = url.origin(),
        .active_document_is_fully_active = true,
        .top_level_creation_url = url,
        .top_level_origin = url.origin(),
        .browsing_context_group_id = {},
        .opener_policy = {},
        .container = {},
        .has_session_history_entry_and_ready_for_navigation = true,
        .compositor_context_id = {},
    };

    H::CrossProcessId root_id { 711, 1 }, child_id { 711, 2 };
    Vector<H::RemoteNavigableDescriptor> descriptors;
    descriptors.append({ root_id, {}, state });
    descriptors.append({ child_id, root_id, state });
    page.create_remote_navigable_graph(move(descriptors));
    auto history = H::create_initial_session_history_entry_descriptor({ 711, 3 }, url, {});
    auto child = page.begin_hosting(child_id, history);
    page.adopt_hosted(*child);
    GC::Root document { *child->active_document() };
    // Complete the native WindowProxy handoff for this hosted document. No
    // renderer process or wire traffic is simulated by this component fixture.
    document->browsing_context()->window_proxy()->set_window(*document->window());
    document->set_url(url);
    document->set_origin(url.origin());
    auto& settings = document->relevant_settings_object();
    settings.creation_url = url;
    settings.top_level_creation_url = url;
    settings.top_level_origin = url.origin();
    return { move(document),
        MUST(String::formatted("{}-{}", root_id.namespace_id, root_id.local_id)) };
}

ErrorOr<JS::Value> run(Context const& context, Utf16View source)
{
    auto& realm = context.realm();
    H::TemporaryExecutionContext execution { realm,
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error())
        return Error::from_string_literal("Invalid native media fixture script");
    GC::Root script { parsed.release_value() };
    auto result = realm.vm().run(*script);
    if (result.is_error()) {
        warnln("Native media script exception: {}",
            result.error_value().to_utf16_string_without_side_effects());
        return Error::from_string_literal("Native media fixture script threw");
    }

    return result.release_value();
}

ErrorOr<void> require(bool condition, StringView label)
{
    if (!condition) {
        warnln("FAIL native autoplay: {}", label);
        return Error::from_string_literal("Native media admission assertion failed");
    }

    return {};
}

ErrorOr<void> check(Context const& context, Utf16View script, StringView label)
{
    auto result = TRY(run(context, script));
    return require(result.is_boolean() && result.as_bool(), label);
}

Web::HTML::HTMLMediaElement& media(Context const& context)
{
    auto value = context.realm().global_object().get_without_side_effects("media"_utf16);
    VERIFY(value.is_object());
    return Web::Bindings::impl_from<H::HTMLMediaElement>(value.as_object());
}

ErrorOr<void> play(Context const& context, bool admitted, StringView label)
{
    // Inspect the genuine binding's Promise immediately. With no source an
    // admitted play remains pending; it cannot demonstrate decoded playback.
    auto value = TRY(
        run(context, u"globalThis.playResult=media.play();playResult.catch(()=>{});playResult"sv));
    TRY(require(value.is_object() && is<JS::Promise>(value.as_object()),
        "play returns a genuine Promise"sv));
    GC::Root promise { static_cast<JS::Promise&>(value.as_object()) };
    bool denied = false;
    if (promise->state() == JS::Promise::State::Rejected && promise->result().is_object()) {
        auto* exception
            = Web::Bindings::impl_from<Web::WebIDL::DOMException>(&promise->result().as_object());
        denied = exception && exception->name() == "NotAllowedError"_utf16;
    }

    if (admitted)
        return require(
            !denied && promise->state() == JS::Promise::State::Pending && !media(context).paused(),
            label);
    return require(denied && media(context).paused(), label);
}

void activate(Context const& context)
{
    // Invoke the actual native activation algorithm. This is not evidence of
    // physical input delivery, and no synthetic DOM event can set this state.
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

void native_controls_play(H::HTMLMediaElement& element)
{
    // Dispatch the genuine public Page context-menu route, which calls the
    // SDK's hidden play_from_user_interaction internally. Use the element's
    // current owner after adoption, not the realm holding a JS reference.
    H::TemporaryExecutionContext execution { element.document().relevant_settings_object() };
    auto navigable = element.document().navigable();
    VERIFY(navigable);
    auto& page = navigable->page();
    page.did_request_media_context_menu(element.unique_id(), navigable->id(), {}, {}, 0, {}, {});
    page.toggle_media_play_state();
}

ErrorOr<void> snapshot(Context const& first, u32 first_generation, A::Policy first_policy,
    Context const& second, u32 second_generation, A::Policy second_policy, Context const& third,
    u32 third_generation, A::Policy third_policy)
{
    Vector<A::Configuration> values;
    values.append({ first.handle, first_generation, first_policy });
    values.append({ second.handle, second_generation, second_policy });
    values.append({ third.handle, third_generation, third_policy });
    return A::apply(move(values));
}

// Keep the only native strong pointer off the collecting caller's stack. The
// actual element constructor/destructor, not a test-created state record, owns
// registration in the production media adapter.
[[gnu::noinline]] GC::Weak<H::HTMLMediaElement> unreachable_element(Web::DOM::Document& document)
{
    auto element
        = MUST(document.create_element("audio"_utf16, Web::Bindings::ElementCreationOptions {}));
    return static_cast<H::HTMLMediaElement&>(*element);
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    (void)loop;
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    auto first = create_context("https://allowed.example/media"sv);
    auto second = create_context("file:///autoplay-probe.html"sv);
    auto third = create_context("https://separate.example/media"sv);
    auto& settings = H::AutoplaySettings::the();
    ScopeGuard cleanup([&] {
        A::shutdown();
        settings.set_policy(H::AutoplayPolicy::BlockAudio, {});
    });
    constexpr auto strict = A::Policy::UserGestureRequired;
    constexpr auto unrestricted = A::Policy::NoUserGestureRequired;
    auto first_root = first.document->navigable()->top_level_traversable();
    TRY(require(A::context_for(*first.document) == first.handle
            && first.handle != Web::WebDriver::BiDi::navigable_id(*first_root),
        "canonical native ID differs from the real BiDi window handle"sv));

    Vector<Utf16String> allowlist;
    allowlist.append("https://allowed.example"_utf16);
    settings.set_policy(H::AutoplayPolicy::BlockAudioAndVideo, allowlist);
    TRY(check(first, u"globalThis.media=document.createElement('audio');true"sv,
        "create allowlisted audio"sv));
    TRY(play(first, true, "unowned origin allowlist remains upstream behavior"sv));
    TRY(check(
        second, u"globalThis.media=document.createElement('audio');true"sv, "create file audio"sv));
    TRY(play(second, true, "unowned file exception remains upstream behavior"sv));
    TRY(check(third, u"globalThis.media=document.createElement('audio');true"sv,
        "create ordinary audio"sv));
    TRY(play(third, false, "unowned non-allowlisted origin remains blocked"sv));
    outln("PASS native media retains unmanaged upstream autoplay behavior");

    TRY(snapshot(first, 1, strict, second, 1, strict, third, 1, strict));
    for (auto* context : { &first, &second, &third }) {
        TRY(check(*context,
            u"media.pause();globalThis.media=document.createElement('audio');true"sv,
            "create strict audio"sv));
        TRY(play(*context, false, "explicit strict policy overrides origin/file exceptions"sv));
    }

    outln("PASS explicit native policy overrides file and HTTP origin bypasses");

    TRY(check(first,
        u"globalThis.firstAudio=media;globalThis.otherAudio=document.createElement('audio');true"sv,
        "create independent audio"sv));
    activate(first);
    TRY(require(first.document->window()->has_transient_activation(),
        "real native activation is transient"sv));
    TRY(play(first, true, "transient activation unlocks this element"sv));
    consume(first);
    TRY(require(first.document->window()->has_sticky_activation()
            && !first.document->window()->has_transient_activation(),
        "consumption leaves sticky activation only"sv));
    TRY(check(first, u"media.pause();media.load();true"sv, "pause and explicit reload"sv));
    TRY(play(first, true, "unlocked element remains unlocked after pause and load"sv));
    TRY(check(first, u"media.pause();globalThis.media=otherAudio;true"sv,
        "select independent locked audio"sv));
    TRY(play(first, false, "sticky document activation does not unlock another element"sv));
    TRY(check(first, u"media.load();true"sv, "sticky-only load"sv));
    TRY(play(first, false, "sticky-only explicit load does not unlock"sv));
    TRY(play(third, false, "another top-level context does not inherit activation"sv));
    outln("PASS actual native transient activation and per-element persistent locks");

    activate(first);
    TRY(check(first, u"media.load();true"sv, "transient explicit load"sv));
    consume(first);
    TRY(play(first, true, "explicit load during transient activation unlocks element"sv));
    TRY(check(first, u"media.pause();media.src='';media.removeAttribute('src');media.load();true"sv,
        "source attribute change canceled before selection"sv));
    TRY(play(first, true, "unlock survives source selection reload with no playable source"sv));
    outln("PASS explicit load unlock and pause/source reload persistence");

    TRY(check(first,
        u"media.pause();globalThis.media=document.createElement('video');media.muted=true;true"sv,
        "create muted video"sv));
    TRY(play(first, true, "muted video is the strict-policy inaudible exception"sv));
    TRY(check(first, u"media.muted=false;media.paused"sv,
        "unmuting without gesture reblocks and pauses"sv));
    TRY(play(first, false, "audible video remains locked without transient activation"sv));
    TRY(check(first, u"globalThis.media=document.createElement('audio');media.muted=true;true"sv,
        "create muted audio"sv));
    TRY(play(first, false, "muted audio does not use the muted-video exception"sv));
    TRY(check(first, u"media.volume=0;true"sv, "zero-volume audio"sv));
    TRY(play(first, false, "zero-volume audio remains locked"sv));
    outln("PASS real muted-video exception excludes audio and reblocks on unmute");

    TRY(check(first, u"globalThis.media=document.createElement('video');media.volume=0;true"sv,
        "create silent video"sv));
    TRY(play(first, true, "zero-volume video admitted"sv));
    TRY(check(first, u"media.volume=0.5;media.paused"sv,
        "increasing volume without activation pauses"sv));
    activate(first);
    TRY(check(first, u"media.volume=1;true"sv, "gesture-controlled audible transition"sv));
    consume(first);
    TRY(play(first, true, "audible transition unlock persists after transient consumption"sv));
    outln("PASS volume transitions use genuine native activation and real pause state");

    TRY(check(third, u"globalThis.media=document.createElement('audio');true"sv,
        "create native-controls audio"sv));
    native_controls_play(media(third));
    TRY(require(!media(third).paused() && !third.document->window()->has_sticky_activation(),
        "native controls unlock without invented document activation"sv));
    TRY(check(third, u"media.pause();true"sv, "pause native controls audio"sv));
    TRY(play(third, true, "native controls unlock persists for later JavaScript play"sv));
    outln("PASS genuine native media-controls play path unlocks its own element");

    {
        H::TemporaryExecutionContext execution { second.realm(),
            H::TemporaryExecutionContext::CallbacksEnabled::Yes };
        auto adopted = second.document->adopt_node(media(third));
        TRY(require(!adopted.is_exception(), "real adoption into another strict document"sv));
    }

    TRY(check(third, u"media.pause();true"sv, "pause adopted audio"sv));
    TRY(play(third, true, "strict-to-strict adoption retains an existing element unlock"sv));
    TRY(snapshot(first, 1, strict, second, 2, unrestricted, third, 1, strict));
    TRY(check(third, u"media.pause();true"sv, "pause before unrestricted policy"sv));
    TRY(play(third, true, "adopted element uses actual destination policy"sv));
    TRY(check(third, u"media.pause();true"sv, "pause before strict transition"sv));
    TRY(snapshot(first, 1, strict, second, 3, strict, third, 1, strict));
    TRY(play(third, false, "transition from unrestricted to strict relocks element"sv));
    TRY(snapshot(first, 1, strict, second, 4, unrestricted, third, 1, strict));
    native_controls_play(media(third));
    TRY(play(third, true, "native controls unlock actual adopted unrestricted element"sv));
    TRY(check(third, u"media.pause();true"sv, "pause before cross-policy adoption"sv));
    {
        H::TemporaryExecutionContext execution { first.realm(),
            H::TemporaryExecutionContext::CallbacksEnabled::Yes };
        auto adopted = first.document->adopt_node(media(third));
        TRY(require(!adopted.is_exception(), "real unrestricted-to-strict adoption"sv));
    }

    TRY(play(third, false, "adoption into strict policy revokes an unrestricted element unlock"sv));
    outln("PASS actual document adoption and policy transition ownership");

    TRY(snapshot(first, 1, strict, second, 5, strict, third, 2, unrestricted));
    TRY(check(third, u"globalThis.media=document.createElement('audio');true"sv,
        "create unrestricted context audio"sv));
    TRY(play(third, true, "unrestricted native context admits source-less play"sv));
    TRY(check(first, u"media.pause();globalThis.media=document.createElement('audio');true"sv,
        "create independent strict context audio"sv));
    TRY(play(first, false, "unrestricted context does not alter strict sibling"sv));
    Vector<A::Configuration> invalid;
    invalid.append({ first.handle, 1, unrestricted });
    TRY(require(
        A::apply(move(invalid)).is_error(), "same-generation conflicting snapshot rejected"sv));
    Vector<A::Configuration> duplicate;
    duplicate.append({ third.handle, 2, unrestricted });
    duplicate.append({ third.handle, 2, unrestricted });
    TRY(require(A::apply(move(duplicate)).is_error(), "duplicate context snapshot rejected"sv));
    Vector<A::Configuration> stale;
    stale.append({ third.handle, 1, strict });
    TRY(require(A::apply(move(stale)).is_error(), "stale snapshot rejected"sv));
    TRY(require(
        A::policy_for(*first.document) == strict && A::policy_for(*third.document) == unrestricted,
        "failed snapshots retain complete previous policy"sv));
    outln("PASS native context separation and atomic invalid-snapshot rejection");

    auto weak = unreachable_element(*first.document);
    for (size_t attempt = 0; attempt < 4; ++attempt) {
        GC::scrub_stack_for_testing();
        first.realm().vm().heap().collect_garbage();
    }

    TRY(require(!weak, "native media state does not root an unreachable real element"sv));
    TRY(check(first, u"globalThis.media=document.createElement('audio');true"sv,
        "create element after native destruction"sv));
    TRY(play(first, false, "fresh element uses a strict lock after collection"sv));
    outln("PASS actual media-element GC and fresh lock initialization");

    auto remote_child = create_remote_child_context();
    auto child_navigable = remote_child.document->navigable();
    auto remote_root = child_navigable->top_level_traversable();
    TRY(require(child_navigable->parent() && is<H::RemoteNavigable>(*remote_root)
            && !is<H::LocalTraversableNavigable>(*child_navigable)
            && A::context_for(*remote_child.document) == remote_child.handle,
        "actual hosted local child resolves its remote top-level native ID"sv));
    Vector<A::Configuration> remote_strict;
    remote_strict.append({ remote_child.handle, 1, strict });
    TRY(A::apply(move(remote_strict)));
    TRY(check(remote_child, u"globalThis.media=document.createElement('audio');true"sv,
        "create real hosted-child audio"sv));
    TRY(play(remote_child, false, "remote parent strict policy governs the actual local child"sv));
    Vector<A::Configuration> remote_unrestricted;
    remote_unrestricted.append({ remote_child.handle, 2, unrestricted });
    TRY(A::apply(move(remote_unrestricted)));
    TRY(play(
        remote_child, true, "remote parent unrestricted policy governs the actual local child"sv));
    outln("PASS actual remote-top/local-child native context policy inheritance (no IPC claim)");

    auto opener = create_context("https://opener.example/"sv);
    auto popup = create_context("https://popup.example/"sv, opener.document->browsing_context());
    TRY(require(popup.document->navigable()->active_browsing_context_opener_navigable().ptr()
            == opener.document->navigable().ptr(),
        "SDK auxiliary browsing context retains its genuine local opener"sv));
    Vector<A::Configuration> popup_policies;
    popup_policies.append({ opener.handle, 1, strict });
    popup_policies.append({ popup.handle, 1, strict });
    TRY(A::apply(move(popup_policies)));
    TRY(check(
        popup, u"globalThis.media=document.createElement('audio');true"sv, "create popup audio"sv));
    TRY(play(popup, false, "inactive opener does not authorize popup play"sv));
    activate(opener);
    TRY(require(!popup.document->window()->has_sticky_activation()
            && !popup.document->window()->has_transient_activation(),
        "opener activation does not invent popup activation"sv));
    TRY(play(popup, true, "genuine local opener transient activation allows this play call"sv));
    TRY(check(popup, u"media.pause();true"sv, "pause opener-authorized play"sv));
    consume(opener);
    TRY(play(popup, false, "opener allowance does not persistently unlock the popup element"sv));
    outln(
        "PASS actual local-opener transient one-play allowance without persistent element unlock");
    return 0;
}
