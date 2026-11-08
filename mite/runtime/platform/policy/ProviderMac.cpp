/** macOS enterprise policy: exact CF scalar types, managed unions and bounded async snapshots. */
#include "Provider.h"
#include <AK/ByteBuffer.h>
#include <AK/ScopeGuard.h>
#include <AK/Utf8View.h>
#include <CoreServices/CoreServices.h>
#include <atomic>
#include <cstring>
#include <dispatch/dispatch.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#ifdef MITE_POLICY_PROVIDER_TESTING
#include "ProviderMacTesting.h"
#endif

namespace Mite::Policy {
#ifndef MITE_POLICY_PROVIDER_TESTING
class PreferencesPeer {
public:
    virtual ~PreferencesPeer() = default;
    virtual CFPropertyListRef copy_value(CFStringRef, CFStringRef) = 0;
    virtual bool is_forced(CFStringRef, CFStringRef) = 0;
    virtual ErrorOr<void> start(dispatch_queue_t, std::function<void()>) = 0;
    virtual void stop() = 0;
};
#endif
namespace {
    class SystemPreferences final : public PreferencesPeer {
    public:
        ~SystemPreferences() override
        {
            stop();
        }

        CFPropertyListRef copy_value(CFStringRef key, CFStringRef product) override
        {
            return CFPreferencesCopyAppValue(key, product);
        }

        bool is_forced(CFStringRef key, CFStringRef product) override
        {
            return CFPreferencesAppValueIsForced(key, product);
        }

        ErrorOr<void> start(dispatch_queue_t queue, std::function<void()> changed) override
        {
            m_changed = std::move(changed);
            CFStringRef directory = CFSTR("/Library/Managed Preferences/");
            auto paths = CFArrayCreate(kCFAllocatorDefault,
                reinterpret_cast<void const**>(&directory), 1, &kCFTypeArrayCallBacks);
            if (!paths)
                return Error::from_errno(ENOMEM);
            ScopeGuard release = [&] { CFRelease(paths); };
            FSEventStreamContext context { 0, this, nullptr, nullptr, nullptr };
            m_stream = FSEventStreamCreate(
                kCFAllocatorDefault,
                [](ConstFSEventStreamRef, void* info, size_t, void*,
                    FSEventStreamEventFlags const[], FSEventStreamEventId const[]) {
                    auto* peer = static_cast<SystemPreferences*>(info);
                    if (peer->m_changed)
                        peer->m_changed();
                },
                &context, paths, kFSEventStreamEventIdSinceNow, 1.0, kFSEventStreamCreateFlagNone);
            if (!m_stream)
                return Error::from_errno(EIO);
            FSEventStreamSetDispatchQueue(m_stream, queue);
            m_started = FSEventStreamStart(m_stream);
            if (!m_started)
                return Error::from_errno(EACCES);
            return {};
        }

        void stop() override
        {
            if (m_stream) {
                // Apple requires successful Start before Stop; a failed Start
                // still leaves a scheduled stream to invalidate and release.
                // https://developer.apple.com/documentation/coreservices/1447673-fseventstreamstop
                if (m_started)
                    FSEventStreamStop(m_stream);
                m_started = false;
                FSEventStreamInvalidate(m_stream);
                FSEventStreamRelease(m_stream);
                m_stream = nullptr;
            }

            m_changed = {};
        }

    private:
        FSEventStreamRef m_stream {};
        bool m_started {};
        std::function<void()> m_changed;
    };

    struct Entry {
        Definition definition;
        CFStringRef key {};
    };

    struct State : std::enable_shared_from_this<State> {
        std::shared_ptr<PreferencesPeer> peer;
        dispatch_queue_t queue {};
        CFStringRef product {};
        std::vector<Entry> entries;
        std::atomic<bool> stopped {}, queued {}, dirty {}, failed {};
        std::mutex mutex;
        Optional<Snapshot> latest;
        Optional<Error> failure;

        ~State()
        {
            for (auto const& entry : entries)
                CFRelease(entry.key);
            if (product)
                CFRelease(product);
            if (queue)
                dispatch_release(queue);
        }
    };

    bool valid_name(ByteString const& value)
    {
        return !value.is_empty() && value.length() <= name_limit && !value.contains('\0')
            && Utf8View(value).validate();
    }

    ErrorOr<Optional<Value>> read_value(State& state, Entry const& entry)
    {
        auto const& definition = entry.definition;
        // policy-watcher 1.7 gates unions (including one-element arrays), but not scalar
        // definitions.
        if (definition.union_type && !state.peer->is_forced(entry.key, state.product))
            return Optional<Value> {};
        auto value = state.peer->copy_value(entry.key, state.product);
        if (!value)
            return Optional<Value> {};
        ScopeGuard release = [&] { CFRelease(value); };
        auto accepts = [&](Type type) {
            for (auto candidate : definition.types)
                if (candidate == type)
                    return true;
            return false;
        };

        auto type = CFGetTypeID(value);
        if (type == CFBooleanGetTypeID() && accepts(Type::Boolean))
            return Optional<Value> { Value { value == kCFBooleanTrue } };
        if (type == CFNumberGetTypeID() && accepts(Type::Number)) {
            long long number {};
            if (CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberLongLongType, &number))
                return Optional<Value> { Value { static_cast<double>(number) } };
        }

        if (type == CFStringGetTypeID() && accepts(Type::String)) {
            auto string = static_cast<CFStringRef>(value);
            auto length = CFStringGetLength(string);
            if (length < 0 || static_cast<size_t>(length) > value_limit)
                return Error::from_errno(EFBIG);
            auto maximum = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
            if (maximum < 0 || static_cast<size_t>(maximum) > value_limit * 3)
                return Error::from_errno(EFBIG);
            auto buffer = TRY(ByteBuffer::create_uninitialized(static_cast<size_t>(maximum) + 1));
            if (!CFStringGetCString(string, reinterpret_cast<char*>(buffer.data()), buffer.size(),
                    kCFStringEncodingUTF8))
                return Optional<Value> {};
            // Preserve the pinned addon's CFStringGetCString/strlen string semantics.
            auto count = std::strlen(reinterpret_cast<char const*>(buffer.data()));
            if (count > value_limit)
                return Error::from_errno(EFBIG);
            return Optional<Value> { Value {
                ByteString(reinterpret_cast<char const*>(buffer.data()), count) } };
        }

        return Optional<Value> {};
    }

    size_t json_string_cost(ByteString const& value)
    {
        size_t count = 2;
        for (auto byte : value.bytes())
            count += byte < 0x20 ? 6 : (byte == '"' || byte == '\\') ? 2 : 1;
        return count;
    }

    ErrorOr<Snapshot> snapshot(State& state)
    {
        Snapshot result;
        size_t bytes = 128; // Leave room for the host's bounded envelope and JSON punctuation.
        for (auto const& entry : state.entries) {
            auto value = TRY(read_value(state, entry));
            if (!value.has_value())
                continue;
            bytes += json_string_cost(entry.definition.name) + 2;
            bytes += value->visit([](ByteString const& text) { return json_string_cost(text); },
                [](double) -> size_t { return 32; }, [](bool) -> size_t { return 5; });
            if (bytes > snapshot_limit)
                return Error::from_errno(EFBIG);
            TRY(result.try_set(entry.definition.name, value.release_value()));
        }

        return result;
    }

    void enqueue(std::shared_ptr<State> const&);

    void refresh(void* context)
    {
        auto state = *static_cast<std::shared_ptr<State>*>(context);
        delete static_cast<std::shared_ptr<State>*>(context);
        state->dirty.store(false, std::memory_order_release);
        if (!state->stopped.load(std::memory_order_acquire)
            && !state->failed.load(std::memory_order_acquire)) {
            auto result = snapshot(*state);
            std::lock_guard lock(state->mutex);
            if (!state->stopped.load(std::memory_order_acquire)) {
                state->latest = {};
                state->failure = {};
                if (result.is_error()) {
                    state->failure = result.release_error();
                    state->failed.store(true, std::memory_order_release);
                } else
                    state->latest = result.release_value();
            }
        }

        state->queued.store(false, std::memory_order_release);
        if (state->dirty.exchange(false, std::memory_order_acq_rel)
            && !state->stopped.load(std::memory_order_acquire))
            enqueue(state);
    }

    void enqueue(std::shared_ptr<State> const& state)
    {
        if (state->stopped.load(std::memory_order_acquire)
            || state->failed.load(std::memory_order_acquire))
            return;
        state->dirty.store(true, std::memory_order_release);
        if (!state->queued.exchange(true, std::memory_order_acq_rel))
            dispatch_async_f(state->queue, new std::shared_ptr<State>(state), refresh);
    }

    class MacProvider final : public Provider {
    public:
        explicit MacProvider(std::shared_ptr<State> state)
            : m_state(std::move(state))
        {
        }

        ~MacProvider() override
        {
            stop();
        }

        ErrorOr<Optional<Snapshot>> take_snapshot() override
        {
            std::lock_guard lock(m_state->mutex);
            if (m_state->stopped.load(std::memory_order_acquire))
                return Optional<Snapshot> {};
            if (m_state->failure.has_value())
                return m_state->failure.release_value();
            return std::exchange(m_state->latest, {});
        }

        void stop() override
        {
            auto state = m_state;
            if (state->stopped.exchange(true, std::memory_order_acq_rel)) {
                // A worker may have initiated stop. An owner-side repeat still
                // waits until that already-revoked native operation has unwound.
                if (dispatch_get_specific(state.get()) != state.get())
                    dispatch_sync_f(state->queue, nullptr, [](void*) { });
                return;
            }

            auto stop = [](void* context) {
                auto& state = *static_cast<State*>(context);
                state.peer->stop();
                std::lock_guard lock(state.mutex);
                state.latest = {};
                state.failure = {};
            };

            if (dispatch_get_specific(state.get()) == state.get())
                stop(state.get());
            else
                dispatch_sync_f(state->queue, state.get(), stop);
        }

    private:
        std::shared_ptr<State> m_state;
    };

    ErrorOr<NonnullOwnPtr<Provider>> create(
        ByteString product, Vector<Definition> definitions, std::shared_ptr<PreferencesPeer> peer)
    {
        if (!valid_name(product) || definitions.size() > definition_limit)
            return Error::from_errno(EINVAL);
        auto state = std::make_shared<State>();
        state->peer = std::move(peer);
        state->product = CFStringCreateWithBytes(kCFAllocatorDefault,
            reinterpret_cast<UInt8 const*>(product.characters()), product.length(),
            kCFStringEncodingUTF8, false);
        if (!state->product)
            return Error::from_errno(EINVAL);
        for (auto& definition : definitions) {
            if (!valid_name(definition.name) || definition.types.is_empty()
                || definition.types.size() > 3
                || (!definition.union_type && definition.types.size() != 1))
                return Error::from_errno(EINVAL);
            for (auto const& entry : state->entries)
                if (entry.definition.name == definition.name)
                    return Error::from_errno(EINVAL);
            for (size_t i = 0; i < definition.types.size(); ++i) {
                auto type = definition.types[i];
                if (type != Type::String && type != Type::Number && type != Type::Boolean)
                    return Error::from_errno(EINVAL);
                for (size_t j = 0; j < i; ++j)
                    if (definition.types[j] == type)
                        return Error::from_errno(EINVAL);
            }

            auto key = CFStringCreateWithBytes(kCFAllocatorDefault,
                reinterpret_cast<UInt8 const*>(definition.name.characters()),
                definition.name.length(), kCFStringEncodingUTF8, false);
            if (!key)
                return Error::from_errno(EINVAL);
            state->entries.push_back({ std::move(definition), key });
        }

        state->queue = dispatch_queue_create("org.elytra.mite.policy", DISPATCH_QUEUE_SERIAL);
        if (!state->queue)
            return Error::from_errno(ENOMEM);
        dispatch_queue_set_specific(state->queue, state.get(), state.get(), nullptr);
        auto provider = TRY(adopt_nonnull_own_or_enomem(new (nothrow) MacProvider(state)));
        auto started = state->peer->start(state->queue, [weak = std::weak_ptr(state)] {
            if (auto current = weak.lock())
                enqueue(current);
        });
        if (started.is_error()) {
            state->stopped.store(true, std::memory_order_release);
            dispatch_sync_f(state->queue, state.get(),
                [](void* value) { static_cast<State*>(value)->peer->stop(); });
            return started.release_error();
        }

        enqueue(state); // The first real snapshot is always produced on the native worker.
        return provider;
    }
}

ErrorOr<NonnullOwnPtr<Provider>> Provider::create(
    ByteString product, Vector<Definition> definitions)
{
    return Mite::Policy::create(
        std::move(product), std::move(definitions), std::make_shared<SystemPreferences>());
}
#ifdef MITE_POLICY_PROVIDER_TESTING
ErrorOr<NonnullOwnPtr<Provider>> create_provider_for_testing(
    ByteString product, Vector<Definition> definitions, std::shared_ptr<PreferencesPeer> peer)
{
    return create(std::move(product), std::move(definitions), std::move(peer));
}
#endif
}
