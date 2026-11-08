/** Non-GUI native provider fixture; it is never installed by product/diagnostic helpers. */
#include "FetchDispatchProbe.h"
#include "loading/protocol/Dispatch.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/FilePacket.h"
#include "native/protocol/files/FileRequests.h"
#include <AK/WeakPtr.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Timer.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibURL/Parser.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>

namespace PF = Firefly::ProtocolFetch;
namespace P = Firefly::Protocols;

namespace {
class Backend final : public RefCounted<Backend>, public Weakable<Backend> {
public:
    explicit Backend(StringView path)
        : m_path(path)
        , m_files(MUST(P::FileRequests::create(91)))
    {
    }

    ~Backend()
    {
        shutdown();
    }

    ErrorOr<NonnullRefPtr<PF::Response>> start(
        Web::Fetch::Infrastructure::FetchParams const& params)
    {
        ++calls;
        auto url = params.request()->current_url().serialize();
        bool slow = url == "app://bundle/slow"sv;
        bool cors = url.bytes_as_string_view().starts_with("cors://target/"sv);
        if (!active || (!slow && !cors && url != "app://bundle/body"sv))
            return Error::from_string_literal("No fixture route");
        bool refuses_cors = cors && !url.bytes_as_string_view().ends_with("/wild"sv)
            && !url.bytes_as_string_view().ends_with("/exact"sv);
        auto id = ++m_next;
        auto weak = make_weak_ptr();
        auto response = TRY(PF::Response::create(params, 91, id,
            {
                .credit =
                    [weak, id, refuses_cors](u64 count) {
                        if (auto self = weak.strong_ref()) {
                            if (refuses_cors)
                                self->unauthorized_credit += count;
                            auto job = self->m_jobs.find(id);
                            if (job == self->m_jobs.end())
                                return;
                            job->value.credit += count;
                            self->schedule(id);
                        }
                    },
                .cancel =
                    [weak, id] {
                        if (auto self = weak.strong_ref()) {
                            ++self->cancellations;
                            self->m_files->cancel(91, id);
                            self->m_jobs.remove(id);
                        }
                    },
            }));
        Vector<HTTP::Header> headers;
        TRY(headers.try_append({ "Cache-Control", "no-store" }));
        TRY(headers.try_append({ "Set-Cookie", "fixture=private" }));
        if (cors) {
            TRY(headers.try_append({ "X-Public", "visible" }));
            TRY(headers.try_append({ "X-Hidden", "private" }));
            TRY(headers.try_append({ "Access-Control-Expose-Headers", "X-Public" }));
            if (url.bytes_as_string_view().ends_with("/wild"sv))
                TRY(headers.try_append({ "Access-Control-Allow-Origin", "*" }));
            else if (url.bytes_as_string_view().ends_with("/exact"sv))
                TRY(headers.try_append({ "Access-Control-Allow-Origin", "app://bundle" }));
            else if (url.bytes_as_string_view().ends_with("/mismatch"sv))
                TRY(headers.try_append({ "Access-Control-Allow-Origin", "app://other" }));
            else if (url.bytes_as_string_view().ends_with("/duplicate"sv)) {
                TRY(headers.try_append({ "Access-Control-Allow-Origin", "app://bundle" }));
                TRY(headers.try_append({ "Access-Control-Allow-Origin", "*" }));
            }
        }

        auto head = TRY(m_files->begin(
            91, id, m_path, params.request()->method() == "HEAD"sv, headers, MonotonicTime::now()));
        TRY(m_jobs.try_set(id, Job { response, move(head), 0, false, false, slow }));
        schedule(id);
        return response;
    }

    void shutdown()
    {
        active = false;
        m_files->revoke();
        auto jobs = move(m_jobs);
        for (auto& entry : jobs)
            entry.value.response->stop();
    }

    size_t pending() const
    {
        return m_jobs.size();
    }

    size_t files() const
    {
        return m_files->pending();
    }

    size_t calls { 0 }, cancellations { 0 };
    u64 unauthorized_credit { 0 };
    bool active { true };

private:
    struct Job {
        NonnullRefPtr<PF::Response> response;
        P::ResponseHead head;
        u64 credit;
        bool scheduled, metadata, slow;
    };

    void schedule(u32 id)
    {
        auto job = m_jobs.find(id);
        if (!active || job == m_jobs.end() || job->value.scheduled)
            return;
        job->value.scheduled = true;
        auto weak = make_weak_ptr();
        Core::deferred_invoke([weak, id] {
            if (auto self = weak.strong_ref())
                self->pump(id);
        });
    }

    void pump(u32 id)
    {
        auto entry = m_jobs.find(id);
        if (!active || entry == m_jobs.end())
            return;
        auto response = entry->value.response;
        entry->value.scheduled = false;
        if (!entry->value.metadata) {
            entry->value.metadata = true;
            auto encoded = P::encode_file_response_head(entry->value.head);
            if (encoded.is_error()
                || !send(*response, P::FilePacketKind::Metadata, id, 0,
                    encoded.is_error() ? ReadonlyBytes {} : encoded.value().bytes())) {
                response->stop();
                return;
            }

            schedule(id);
            return;
        }

        if (entry->value.slow
            || (!entry->value.head.head && entry->value.head.content_length
                && !entry->value.credit))
            return;
        auto chunk = m_files->pull(91, id,
            entry->value.head.head || !entry->value.head.content_length
                ? 1
                : min<u64>(entry->value.credit, P::file_chunk_limit),
            MonotonicTime::now());
        if (chunk.is_error()) {
            response->stop();
            return;
        }

        entry->value.credit -= chunk.value().bytes.size();
        auto bytes = chunk.release_value();
        if (!bytes.bytes.is_empty()
            && !send(*response, P::FilePacketKind::Chunk, id, bytes.offset, bytes.bytes))
            return;
        if (bytes.complete) {
            if (!response->finished())
                (void)send(*response, P::FilePacketKind::Complete, id,
                    bytes.offset + bytes.bytes.size(), {});
            m_jobs.remove(id);
        } else
            schedule(id);
    }

    static bool send(
        PF::Response& response, P::FilePacketKind kind, u32 id, u64 offset, ReadonlyBytes bytes)
    {
        auto packet = P::encode_file_packet({ kind, 91, id, offset, bytes });
        return !packet.is_error() && !response.receive(packet.value()).is_error();
    }

    ByteString m_path;
    NonnullRefPtr<P::FileRequests> m_files;
    HashMap<u32, Job> m_jobs;
    u32 m_next { 0 };
};

ErrorOr<JS::Value> run(JS::Realm& realm, Utf16View source)
{
    // spin_until deliberately clears its caller's JS stack while pumping.
    Web::HTML::TemporaryExecutionContext context { realm,
        Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes };
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error())
        return Error::from_string_literal("Invalid dispatcher fixture script");
    GC::Root script { parsed.release_value() };
    auto result = realm.vm().run(*script);
    if (result.is_error())
        return Error::from_string_literal("Dispatcher script threw synchronously");
    return result.release_value();
}

ErrorOr<void> wait(JS::Realm& realm, Utf16View predicate)
{
    auto deadline = Core::Timer::create_single_shot(AK::Duration::from_seconds(10), [] { });
    deadline->start();
    auto text = Utf16String::from_utf16(predicate);
    auto& vm = realm.vm();
    Web::HTML::main_thread_event_loop().spin_until(
        GC::create_function(realm.heap(), [&realm, text, deadline] {
            auto result = run(realm, text);
            return result.is_error() || (result.value().is_boolean() && result.value().as_bool())
                || !deadline->is_active();
        }));
    auto result = TRY(run(realm, predicate));
    if (!result.is_boolean() || !result.as_bool()) {
        auto detail = run(realm, u"globalThis.dispatchResult"sv);
        if (!detail.is_error())
            warnln("JavaScript dispatch result: {}",
                detail.value().to_utf16_string_without_side_effects());
        return Error::from_string_literal("JavaScript fetch dispatch did not settle correctly");
    }

    vm.heap().collect_garbage();
    return {};
}
}

ErrorOr<void> check_fetch_dispatch(JS::Realm& realm, Web::DOM::Document& document, StringView path)
{
    Array schemes { P::OriginScheme { "app"_string, true, true },
        P::OriginScheme { "cors"_string, true, true, true } };
    TRY(P::install_origin_schemes(schemes));
    auto url = URL::Parser::basic_parse("app://bundle/index.html"sv);
    VERIFY(url.has_value());
    document.set_url(*url);
    document.set_origin(url->origin());
    auto& settings = document.relevant_settings_object();
    settings.creation_url = *url;
    settings.top_level_creation_url = *url;
    settings.top_level_origin = url->origin();

    TRY(run(realm,
        u"globalThis.dispatchResult='pending'; fetch('app://bundle/body').then(()=>dispatchResult='bad',()=>dispatchResult='denied');"sv));
    TRY(wait(realm, u"dispatchResult==='denied'"sv));
    auto backend = adopt_ref(*new Backend(path));
    auto weak = backend->make_weak_ptr();
    auto registration = TRY(PF::Registration::install(
        [weak](auto const& params) -> ErrorOr<NonnullRefPtr<PF::Response>> {
            if (auto owner = weak.strong_ref())
                return owner->start(params);
            return Error::from_string_literal("Fixture provider has ended");
        }));
    if (!PF::Registration::install([](auto const&) -> ErrorOr<NonnullRefPtr<PF::Response>> {
            return Error::from_string_literal("unused");
        }).is_error())
        return Error::from_string_literal("Duplicate provider was admitted");

    TRY(run(realm, uR"JS(
        globalThis.dispatchResult='pending';
        fetch('app://bundle/body').then(async response=>{
            if(response.status!==200 || response.headers.get('cache-control')!=='no-store' || response.headers.has('set-cookie')) throw Error('headers');
            const bytes=new Uint8Array(await response.arrayBuffer());
            if(bytes.length!==4194323 || !bytes.every((v,i)=>v===((i*37+11)&255))) throw Error('bytes');
            dispatchResult='ok';
        }).catch(error=>dispatchResult='error:'+error.message);
    )JS"sv));
    TRY(wait(realm, u"dispatchResult==='ok'"sv));
    if (backend->pending() || backend->files())
        return Error::from_string_literal("Completed JavaScript fetch retained file jobs");
    TRY(run(realm,
        u"dispatchResult='pending'; fetch('app://bundle/body',{method:'HEAD'}).then(async r=>dispatchResult=(r.headers.get('content-length')==='4194323'&&(await r.arrayBuffer()).byteLength===0)?'head':'bad').catch(()=>dispatchResult='bad');"sv));
    TRY(wait(realm, u"dispatchResult==='head'"sv));
    // Exercise upstream CORS filtering with actual native response bodies, not a mock fetch.
    TRY(run(realm, uR"JS(
        dispatchResult='pending';
        (async()=>{
            for (const route of ['wild','exact']) {
                const r=await fetch('cors://target/'+route);
                if(r.type!=='cors' || r.url!=='cors://target/'+route || r.status!==200 ||
                    r.headers.get('x-public')!=='visible' || r.headers.has('x-hidden') ||
                    r.headers.has('set-cookie') || r.headers.has('access-control-allow-origin')) throw Error('CORS filter');
                const bytes=new Uint8Array(await r.arrayBuffer());
                if(bytes.length!==4194323 || !bytes.every((v,i)=>v===((i*37+11)&255))) throw Error('CORS bytes');
            }
            const head=await fetch('cors://target/exact',{method:'HEAD',credentials:'omit'});
            if(head.type!=='cors' || (await head.arrayBuffer()).byteLength!==0) throw Error('CORS HEAD');
            for(const route of ['deny','mismatch','duplicate']) {
                let denied=false;
                try { await fetch('cors://target/'+route); } catch { denied=true; }
                if(!denied) throw Error('CORS unauthorized response');
            }
            dispatchResult='cors';
        })().catch(error=>dispatchResult='error:'+error.message);
    )JS"sv));
    TRY(wait(realm, u"dispatchResult==='cors'"sv));
    if (backend->pending() || backend->files() || backend->unauthorized_credit)
        return Error::from_string_literal("CORS refusal retained file jobs or granted body credit");
    auto accepted = backend->calls;
    for (auto source : { u"fetch('app://other/body',{mode:'no-cors'})"sv,
             u"fetch('app://bundle/body',{method:'POST'})"sv,
             u"fetch('app://bundle/body',{headers:{Range:'bytes=0-1'}})"sv,
             u"fetch('unknown://bundle/body')"sv, u"fetch('app://other/body')"sv,
             u"fetch('cors://target/wild',{credentials:'include'})"sv,
             u"fetch('cors://target/wild',{mode:'no-cors'})"sv,
             u"fetch('cors://target/wild',{mode:'same-origin'})"sv,
             u"fetch('cors://target/wild',{headers:{'X-Private':'test'}})"sv,
             u"fetch('cors://target/wild',{method:'POST'})"sv,
             u"fetch('cors://target/wild',{headers:{Range:'bytes=0-1'}})"sv }) {
        auto script = Utf16String::formatted(
            "dispatchResult='pending'; "
            "{}.then(()=>dispatchResult='bad',()=>dispatchResult='denied');",
            source);
        TRY(run(realm, script));
        TRY(wait(realm, u"dispatchResult==='denied'"sv));
    }

    if (backend->calls != accepted)
        return Error::from_string_literal("Ineligible request reached native provider");
    TRY(run(realm,
        u"dispatchResult='pending'; globalThis.headersReceived=false; globalThis.fetchAbort=new AbortController(); fetch('app://bundle/slow',{signal:fetchAbort.signal}).then(r=>{headersReceived=true;return r.arrayBuffer()}).then(()=>dispatchResult='bad',error=>dispatchResult='aborted:'+error.name);"sv));
    TRY(wait(realm, u"headersReceived===true"sv));
    if (backend->pending() != 1 || backend->files() != 1)
        return Error::from_string_literal("Abort fixture has no live native file job");
    TRY(run(realm, u"fetchAbort.abort();"sv));
    TRY(wait(realm, u"dispatchResult==='aborted:AbortError'"sv));
    if (backend->pending() || backend->files())
        return Error::from_string_literal("Aborted JavaScript fetch retained file jobs");
    TRY(run(realm,
        u"dispatchResult='pending'; fetch('data:text/plain,upstream').then(r=>r.text()).then(text=>dispatchResult=text,()=>dispatchResult='bad');"sv));
    TRY(wait(realm, u"dispatchResult==='upstream'"sv));
    registration->revoke();
    backend->shutdown();
    TRY(run(realm,
        u"dispatchResult='pending'; fetch('app://bundle/body').then(()=>dispatchResult='bad',()=>dispatchResult='denied');"sv));
    TRY(wait(realm,
        u"dispatchResult==='denied' && ['process','require','firefly','electron'].every(k=>typeof globalThis[k]==='undefined')"sv));

    // Reentrant revocation must retain the provider during the callback, cancel
    // its returned body and reject fetch rather than expose a stale response.
    auto reentrant = adopt_ref(*new Backend(path));
    PF::Registration* installed = nullptr;
    {
        auto scoped = TRY(PF::Registration::install(
            [&](auto const& params) -> ErrorOr<NonnullRefPtr<PF::Response>> {
                auto response = TRY(reentrant->start(params));
                installed->revoke();
                return response;
            }));
        installed = scoped.ptr();
        TRY(run(realm,
            u"dispatchResult='pending'; fetch('app://bundle/body').then(()=>dispatchResult='bad',()=>dispatchResult='denied');"sv));
        TRY(wait(realm, u"dispatchResult==='denied'"sv));
        if (reentrant->pending() || reentrant->files() || reentrant->cancellations != 1)
            return Error::from_string_literal("Reentrant revocation retained native file jobs");
    }

    // Destroying an unused registration must release the slot as well.
    for (size_t i = 0; i < 2; ++i) {
        auto scoped = TRY(
            PF::Registration::install([](auto const&) -> ErrorOr<NonnullRefPtr<PF::Response>> {
                return Error::from_string_literal("Unused lifecycle fixture");
            }));
    }

    outln("PASS actual JavaScript fetch/arrayBuffer through native scheme seam: 4194323 file "
          "bytes, HEAD, CORS origin/exposure/credentials/preflight checks, forbidden-header "
          "filtering, scope refusals, abort, upstream data, "
          "reentrant revocation and absent "
          "capabilities");
    return {};
}
