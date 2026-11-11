/** Map actual FetchController credit/cancellation to bounded native IPC, without file IO. */
#include "Client.h"
#include "Dispatch.h"
#include "native/protocol/Diagnostics.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/FilePacket.h"
#include <AK/HashTable.h>
#include <AK/NumericLimits.h>
#include <LibCore/EventLoop.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/SourceTextModule.h>
#include <LibURL/Parser.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/ImportMap.h>
#include <LibWeb/HTML/Scripting/ModuleMap.h>
#include <LibWeb/HTML/Scripting/ModuleScript.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly::ProtocolFetch {
namespace {
    // Queued requests retain fetch state and bounded URLs, never native files.
    constexpr size_t document_job_limit = 8192;
    constexpr size_t process_job_limit = 16384;
    constexpr size_t document_url_limit = 32 * 1024 * 1024;
    constexpr size_t process_url_limit = 64 * 1024 * 1024;
    size_t process_jobs;
    size_t process_active;
    size_t process_url_bytes;
    Vector<WeakPtr<Client>> clients;

    struct ModuleCounts {
        size_t missing {}, loading {}, fetched {}, errors {}, callbacks {};
    };

    size_t scheme_class(URL::URL const& url)
    {
        if (url.scheme() == "blob"sv)
            return 0;
        if (Protocols::is_standard_scheme(url.scheme()))
            return 1;
        if (url.scheme() == "file"sv)
            return 2;
        return 3;
    }

    GC::Ptr<Web::HTML::ModuleScript> observe_module(
        Web::HTML::ModuleMap const& map, URL::URL const& url, Utf16View type, ModuleCounts& counts)
    {
        auto entry = map.get(url, type);
        if (!entry.has_value()) {
            ++counts.missing;
            return nullptr;
        }
        if (auto* waiting = entry->get_pointer<Web::HTML::ModuleMap::CallbackList>()) {
            ++counts.loading;
            counts.callbacks += waiting->size();
            return nullptr;
        }
        auto script = entry->get<GC::Ref<Web::HTML::ModuleScript>>();
        ++counts.fetched;
        if (!script->parse_error().is_null() || !script->error_to_rethrow().is_null()
            || script->record().has<Empty>())
            ++counts.errors;
        return script.ptr();
    }

    void observe_graph(Web::DOM::Document& document, URL::URL const& root, unsigned after)
    {
        if (!document.is_fully_active() || !document.window())
            return;
        auto& map = document.relevant_settings_object().module_map();
        auto const& imports = document.window()->import_map();
        ModuleCounts counts[4];
        HashTable<ByteString> targets;
        HashTable<JS::SourceTextModule const*> records;
        Vector<GC::Root<Web::HTML::ModuleScript>> work;
        size_t edges = 0, key_bytes = 0, skipped = 0;
        bool truncated = false;
        auto add = [&](URL::URL const& url, Utf16View type) {
            if (truncated)
                return;
            auto serialized = url.serialize();
            auto key = ByteString::formatted("{}\n{}", serialized, type);
            if (targets.contains(key))
                return;
            if (targets.size() >= 8192 || key.length() > 65536
                || key.length() > 8 * 1024 * 1024 - key_bytes) {
                truncated = true;
                return;
            }
            key_bytes += key.length();
            targets.set(move(key));
            if (auto script = observe_module(map, url, type, counts[scheme_class(url)]))
                work.append(GC::Root { *script });
        };
        add(root, u"javascript-or-wasm"sv);
        for (size_t index = 0; index < work.size() && !truncated; ++index) {
            auto& script = *work[index];
            auto record = script.record();
            auto* source = record.get_pointer<GC::Ref<JS::SourceTextModule>>();
            if (!source || records.contains(source->ptr()) || !script.base_url().has_value())
                continue;
            records.set(source->ptr());
            auto base = *script.base_url();
            auto base_text = Utf16String::from_utf8_with_replacement_character(base.serialize());
            auto requests = (*source)->requested_modules();
            for (auto const& request : requests) {
                if (++edges > 65536) {
                    truncated = true;
                    break;
                }
                auto type = u"javascript-or-wasm"_utf16;
                bool unsupported = false, typed = false;
                for (auto const& attribute : request.attributes) {
                    if (attribute.key != u"type"sv || typed
                        || !(attribute.value == u"css"sv || attribute.value == u"json"sv
                            || attribute.value == u"text"sv)) {
                        unsupported = true;
                        break;
                    }
                    typed = true;
                    type = attribute.value;
                }
                // Scoped/prefix import maps are explicitly outside this observer's
                // subset; never report their targets as missing from an approximation.
                for (auto const& scope : imports.scopes())
                    if (scope.key == base_text
                        || (scope.key.utf16_view().ends_with('/')
                            && base_text.utf16_view().starts_with(scope.key.utf16_view())))
                        unsupported = true;
                if (unsupported) {
                    ++skipped;
                    continue;
                }
                auto specifier = request.module_specifier.view();
                auto url = specifier.starts_with("/"sv) || specifier.starts_with("./"sv)
                        || specifier.starts_with("../"sv)
                    ? URL::Parser::basic_parse(specifier, base)
                    : URL::Parser::basic_parse(specifier);
                auto normalized = url.has_value()
                    ? Utf16String::from_utf8_with_replacement_character(url->serialize())
                    : Utf16String::from_utf16(specifier);
                if (auto* exact = imports.imports().find(normalized.utf16_view())) {
                    if (exact->has_value())
                        add(**exact, type.utf16_view());
                    else
                        ++skipped;
                    continue;
                }
                bool prefix = false;
                for (auto const& mapping : imports.imports())
                    if (mapping.key.utf16_view().ends_with('/')
                        && normalized.utf16_view().starts_with(mapping.key.utf16_view()))
                        prefix = true;
                if (!url.has_value() || prefix) {
                    ++skipped;
                    continue;
                }
                add(*url, type.utf16_view());
            }
        }
        static constexpr char const* schemes[] = { "blob", "custom", "file", "other" };
        std::fprintf(stderr,
            "FIREFLY_MODULE_GRAPH after=%u records=%zu targets=%zu edges=%zu skipped=%zu "
            "key_bytes=%zu truncated=%u vm_stack=%zu\n",
            after, records.size(), targets.size(), edges, skipped, key_bytes, truncated ? 1u : 0u,
            JS::VM::the().execution_context_stack().size());
        for (size_t i = 0; i < 4; ++i)
            std::fprintf(stderr,
                "FIREFLY_MODULE_GRAPH_SCHEME after=%u scheme=%s missing=%zu loading=%zu "
                "fetched=%zu errors=%zu callbacks=%zu\n",
                after, schemes[i], counts[i].missing, counts[i].loading, counts[i].fetched,
                counts[i].errors, counts[i].callbacks);
        ModuleCounts mapped[4];
        size_t mapped_count = 0;
        for (auto const& mapping : imports.imports()) {
            if (++mapped_count > 8192)
                break;
            if (mapping.value.has_value())
                (void)observe_module(map, *mapping.value, u"javascript-or-wasm"sv,
                    mapped[scheme_class(*mapping.value)]);
        }
        for (size_t i = 0; i < 4; ++i)
            std::fprintf(stderr,
                "FIREFLY_IMPORT_MAP_SCHEME after=%u scheme=%s missing=%zu loading=%zu "
                "fetched=%zu errors=%zu callbacks=%zu\n",
                after, schemes[i], mapped[i].missing, mapped[i].loading, mapped[i].fetched,
                mapped[i].errors, mapped[i].callbacks);
    }
}

void Client::wake_clients()
{
    clients.remove_all_matching([](auto const& weak) { return !weak; });
    // Each deferred pump admits at most one new job before rejoining the end of
    // the event queue, so one busy document cannot monopolize released slots.
    for (auto const& weak : clients)
        if (auto client = weak.strong_ref())
            client->schedule();
}

void Client::remove_job(u32 id)
{
    auto removed = m_jobs.take(id);
    if (!removed.has_value())
        return;
    --process_jobs;
    auto bytes = removed->url.length();
    m_waiting_url_bytes -= bytes;
    process_url_bytes -= bytes;
    if (removed->sent) {
        --m_active;
        --process_active;
    }
    wake_clients();
}

Client::Client(u64 epoch, Web::DOM::Document& document)
    : m_document(document)
    , m_origin(document.origin())
    , m_epoch(epoch)
{
}

ErrorOr<NonnullRefPtr<Client>> Client::create(
    NonnullOwnPtr<IPC::Transport> transport, u64 epoch, Web::DOM::Document& document)
{
    auto owner = adopt_ref(*new Client(epoch, document));
    if (!epoch || !owner->scope_is_active())
        return Error::from_string_literal("Protocol channel requires an admitted active document");
    TRY(owner->initialize(move(transport)));
    return owner;
}

ErrorOr<NonnullRefPtr<Client>> Client::create_navigation(NonnullOwnPtr<IPC::Transport> transport,
    u64 epoch, Web::HTML::LocalNavigable& navigable, URL::URL url, Utf16String id)
{
    if (!epoch)
        return Error::from_string_literal("Invalid navigation channel epoch");
    auto navigation = TRY(Navigation::create(navigable, move(url), move(id)));
    auto owner = adopt_ref(*new Client(epoch, *navigable.active_document()));
    owner->m_navigation = move(navigation);
    TRY(owner->initialize(move(transport)));
    return owner;
}

ErrorOr<void> Client::initialize(NonnullOwnPtr<IPC::Transport> transport)
{
    auto weak = make_weak_ptr();
    m_channel = TRY(Bytes::Channel::create(move(transport),
        {
            .on_data =
                [weak](ReadonlyBytes bytes) {
                    if (auto self = weak.strong_ref())
                        self->receive(bytes);
                },
            .on_writable =
                [weak] {
                    if (auto self = weak.strong_ref())
                        self->schedule();
                },
            .on_closed =
                [weak](auto) {
                    if (auto self = weak.strong_ref())
                        self->revoke();
                },
        }));
    m_timer = Core::Timer::create_repeating(AK::Duration::from_seconds(1), [weak] {
        if (auto self = weak.strong_ref())
            self->sweep();
    });
    m_timer->start();
    clients.append(make_weak_ptr());
    return {};
}

Client::~Client()
{
    revoke();
}

bool Client::scope_is_active() const
{
    if (m_navigation)
        return !m_revoked && m_navigation->active();
    if (m_revoked || !m_document || !m_document->is_fully_active() || !m_document->navigable()
        || !m_document->navigable()->is_top_level_traversable()
        || !m_origin.is_same_origin(m_document->origin()))
        return false;
    if (Protocols::file_policy_snapshot().active)
        return true; // The caller also checks exact native document/settings identity per request.
    return !m_origin.is_opaque() && m_origin.is_same_origin(m_document->url().origin())
        && Protocols::is_standard_scheme(m_document->url().scheme());
}

void Client::revoke()
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_revoked)
        return;
    Protocols::diagnostic(Protocols::DiagnosticStage::ClientRevoke, m_jobs.size());
    m_revoked = true;
    if (m_timer)
        m_timer->stop();
    if (m_channel)
        m_channel->close();
    auto jobs = move(m_jobs);
    process_jobs -= jobs.size();
    process_active -= m_active;
    process_url_bytes -= m_waiting_url_bytes;
    m_active = 0;
    m_waiting_url_bytes = 0;
    for (auto& entry : jobs)
        if (entry.value.response)
            entry.value.response->stop();
    wake_clients();
    m_navigation = nullptr;
    m_document = nullptr;
}

ErrorOr<NonnullRefPtr<Response>> Client::fetch(
    Web::Fetch::Infrastructure::FetchParams const& params)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return Error::from_string_literal("Protocol document channel is revoked");
    }

    auto const& request = *params.request();
    auto const* address = request.client().has_value()
        ? request.client()->get_pointer<Web::HTML::EnvironmentSettingsObjectAddress>()
        : nullptr;
    // Origin equality cannot distinguish two live same-origin documents. Compare
    // the native local settings identity too; remote/worker clients are refused.
    bool admitted = m_navigation ? m_navigation->admit(params)
                                 : allows_request(params) && address
            && *address == m_document->relevant_settings_object().address()
            && request.origin().has<URL::Origin>()
            && request.origin().get<URL::Origin>().is_same_origin(m_origin);
    if (!admitted || m_jobs.size() >= document_job_limit || process_jobs >= process_job_limit
        || m_next == NumericLimits<u32>::max()) {
        Protocols::diagnostic(admitted ? Protocols::DiagnosticStage::ClientQuota
                                       : Protocols::DiagnosticStage::ClientScope,
            m_jobs.size());
        return Error::from_string_literal("Protocol request is outside the admitted scope");
    }
    auto url = request.current_url().serialize();
    if (url.bytes().size() > Protocols::file_request_url_limit
        || url.bytes().size() > document_url_limit - m_waiting_url_bytes
        || url.bytes().size() > process_url_limit - process_url_bytes) {
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientUrl, m_jobs.size());
        return Error::from_string_literal("Protocol request URL exceeds the transport limit");
    }
    auto id = ++m_next;
    auto weak = make_weak_ptr();
    auto response = TRY(Response::create(params, m_epoch, id,
        {
            .credit =
                [weak, id](u64 count) {
                    if (auto self = weak.strong_ref())
                        self->credit(id, count);
                },
            .cancel =
                [weak, id] {
                    if (auto self = weak.strong_ref())
                        self->cancel(id);
                },
            .completed =
                [weak, id] {
                    if (auto self = weak.strong_ref())
                        self->remove_job(id);
                },
        },
        m_navigation ? Protocols::navigation_body_limit : Protocols::file_size_limit));
    TRY(m_jobs.try_set(id,
        Job { response, url.to_byte_string(), MonotonicTime::now() + AK::Duration::from_seconds(30),
            0, request.method() == "HEAD"sv }));
    ++process_jobs;
    Protocols::diagnostic(Protocols::DiagnosticStage::ClientAdmitted, m_jobs.size());
    m_waiting_url_bytes += url.bytes().size();
    process_url_bytes += url.bytes().size();
    schedule();
    return response;
}

void Client::credit(u32 id, u64 count)
{
    if (m_revoked)
        return;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end() || found->value.cancelled)
        return;
    if (!count || count > Protocols::file_chunk_limit - found->value.credit) {
        revoke();
        return;
    }

    found->value.credit += count;
    schedule();
}

void Client::cancel(u32 id)
{
    if (m_revoked)
        return;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end())
        return;
    if (!found->value.sent) {
        remove_job(id);
        return;
    }
    // Keep this bounded retired job until a terminal frame consumes any already
    // in-flight data. Release its VM owner immediately, without granting credit.
    found->value.cancelled = true;
    found->value.response = nullptr;
    found->value.credit = 0;
    schedule();
}

void Client::receive(ReadonlyBytes bytes)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto packet = Protocols::decode_file_packet(bytes);
    if (!scope_is_active() || packet.is_error() || packet.value().epoch != m_epoch) {
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientPacket, m_jobs.size());
        revoke();
        return;
    }

    auto id = packet.value().request;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end() || !found->value.sent) {
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientUnknown, m_jobs.size());
        revoke();
        return;
    }

    if (found->value.cancelled) {
        if (packet.value().kind == Protocols::FilePacketKind::Cancel
            || packet.value().kind == Protocols::FilePacketKind::Complete)
            remove_job(id);
        return;
    }

    auto response = found->value.response;
    auto result = response->receive(bytes);
    if (result.is_error()) {
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientResponse, m_jobs.size());
        revoke();
        return;
    }
    // Response callbacks may cancel/revoke and invalidate the original map entry.
    if (!m_revoked && response->finished())
        remove_job(id);
}

void Client::schedule()
{
    if (m_revoked || m_scheduled || m_jobs.is_empty())
        return;
    m_scheduled = true;
    auto weak = make_weak_ptr();
    Core::deferred_invoke([weak] {
        if (auto self = weak.strong_ref())
            self->pump();
    });
}

void Client::pump()
{
    VERIFY(m_thread == AK::ThreadID::current());
    m_scheduled = false;
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return;
    }

    if (m_channel->pending_send_count() >= Bytes::max_pending)
        return;
    Optional<u32> oldest_waiting;
    if (m_active < Protocols::document_file_request_limit
        && process_active < Protocols::process_file_request_limit) {
        for (auto const& entry : m_jobs)
            if (!entry.value.sent && (!oldest_waiting.has_value() || entry.key < *oldest_waiting))
                oldest_waiting = entry.key;
    }
    Optional<u32> first, after;
    for (auto const& entry : m_jobs) {
        auto const& job = entry.value;
        bool start = oldest_waiting.has_value() && entry.key == *oldest_waiting;
        if (!(start || (job.sent && ((job.cancelled && !job.cancel_sent) || job.credit))))
            continue;
        if (!first.has_value() || entry.key < *first)
            first = entry.key;
        if (entry.key > m_last_pumped && (!after.has_value() || entry.key < *after))
            after = entry.key;
    }

    auto id = after.has_value() ? after : first;
    if (!id.has_value())
        return;
    m_last_pumped = *id;
    auto& job = m_jobs.find(*id)->value;
    if (MonotonicTime::now() >= job.deadline) {
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientDeadline, m_jobs.size());
        revoke();
        return;
    }
    using Kind = Protocols::FileRequestKind;
    auto kind = !job.sent ? (job.head ? Kind::Head : Kind::Get)
        : job.cancelled   ? Kind::Cancel
                          : Kind::Credit;
    auto wire = Protocols::encode_file_request({ kind, m_epoch, *id,
        kind == Kind::Credit ? job.credit : 0, !job.sent ? job.url.bytes() : ReadonlyBytes {} });
    if (wire.is_error() || m_channel->send(wire.value()).is_error()) {
        revoke();
        return;
    }

    if (!job.sent) {
        ++m_active;
        ++process_active;
        Protocols::diagnostic(Protocols::DiagnosticStage::ClientSent, m_active);
        m_waiting_url_bytes -= job.url.length();
        process_url_bytes -= job.url.length();
        job.sent = true;
        job.url = {};
    }
    if (kind == Kind::Credit)
        job.credit = 0;
    if (kind == Kind::Cancel)
        job.cancel_sent = true;
    schedule();
}

void Client::sweep()
{
    if (auto after = m_diagnostics.due()) {
        size_t waiting = 0, credit_ready = 0;
        for (auto const& entry : m_jobs) {
            waiting += !entry.value.sent;
            credit_ready += entry.value.sent && !!entry.value.credit;
        }
        std::fprintf(stderr,
            "FIREFLY_QUEUE_SNAPSHOT stage=protocol-client after=%u jobs=%zu active=%zu "
            "waiting=%zu credit_ready=%zu url_bytes=%zu native_frames=%zu "
            "process_jobs=%zu process_active=%zu current=%u\n",
            after, m_jobs.size(), m_active, waiting, credit_ready, m_waiting_url_bytes,
            m_channel ? m_channel->pending_send_count() : 0, process_jobs, process_active,
            scope_is_active() ? 1u : 0u);
        if (!m_navigation && m_document && m_document->is_fully_active()) {
            struct ObservedModule {
                char const* label;
                StringView relative;
            };

            static constexpr ObservedModule modules[] = { { "bootstrap", "./workbench.js"sv },
                { "workbench-desktop", "../../../workbench/workbench.desktop.main.js"sv },
                { "workbench-common", "../../../workbench/workbench.common.main.js"sv },
                { "desktop-main", "../../../workbench/electron-browser/desktop.main.js"sv },
                // Code-OSS imports through appRoot/out even when the bootstrap
                // HTML uses src. Keep URL identities distinct despite the disk alias.
                { "out-workbench-desktop",
                    "../../../../../out/vs/workbench/workbench.desktop.main.js"sv },
                { "out-workbench-common",
                    "../../../../../out/vs/workbench/workbench.common.main.js"sv },
                { "out-desktop-main",
                    "../../../../../out/vs/workbench/electron-browser/desktop.main.js"sv } };
            auto& map = m_document->relevant_settings_object().module_map();
            for (auto const& observed : modules) {
                auto url = URL::Parser::basic_parse(observed.relative, m_document->url());
                if (url.has_value()
                    && observed.relative
                        == "../../../../../out/vs/workbench/workbench.desktop.main.js"sv)
                    observe_graph(*m_document, *url, after);
                auto entry = url.has_value() ? map.get(*url, u"javascript-or-wasm"sv)
                                             : Optional<Web::HTML::ModuleMap::Entry const&> {};
                char const* status = url.has_value() ? "missing" : "unresolved";
                size_t callbacks = 0;
                bool empty = false, parse_error = false, rethrow = false, environment = false;
                if (entry.has_value()) {
                    if (auto* waiting = entry->get_pointer<Web::HTML::ModuleMap::CallbackList>()) {
                        status = "loading";
                        callbacks = waiting->size();
                    } else {
                        status = "fetched";
                        auto script = entry->get<GC::Ref<Web::HTML::ModuleScript>>();
                        empty = script->record().has<Empty>();
                        parse_error = !script->parse_error().is_null();
                        rethrow = !script->error_to_rethrow().is_null();
                        auto record = script->record();
                        if (auto* source = record.get_pointer<GC::Ref<JS::SourceTextModule>>())
                            environment = !!(*source)->environment();
                    }
                }
                std::fprintf(stderr,
                    "FIREFLY_MODULE_SNAPSHOT label=%s after=%u state=%s callbacks=%zu "
                    "record_empty=%u parse_error=%u rethrow=%u source_environment=%u\n",
                    observed.label, after, status, callbacks, empty ? 1u : 0u,
                    parse_error ? 1u : 0u, rethrow ? 1u : 0u, environment ? 1u : 0u);
            }
        }
    }
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return;
    }

    auto now = MonotonicTime::now();
    for (auto const& entry : m_jobs)
        if (now >= entry.value.deadline) {
            revoke();
            return;
        }
}
}
