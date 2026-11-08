/** Exclusive Carbon hotkeys mapped against the selected TIS keyboard layout, without AX/media
 * access. */
#include "Provider.h"
#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

namespace Firefly::GlobalShortcut {
namespace {
    constexpr UInt32 signature = 0x46664753; // FfGS, private process-wide Carbon namespace.

    struct State {
        EventCallback callback;
        std::unordered_map<int, UInt32> owned;
        int last {};
        std::atomic<bool> stopped {};
    };

    struct Registration {
        std::weak_ptr<State> owner;
        int token;
        UInt32 keycode;
        UInt32 modifiers;
        EventHotKeyRef ref {};
        bool active {};
    };

    struct Registry {
        EventHandlerRef handler {};
        UInt32 next { 1 };
        std::unordered_map<UInt32, Registration> keys;
    };

    Registry& registry()
    {
        // Carbon retains the static callback, not a provider pointer. Failed native
        // removals remain bounded/retired here instead of becoming dangling refcons.
        static auto* value = new Registry;
        return *value;
    }

    Result error(char const* code, char const* message)
    {
        return { false, code, message };
    }

    Result native_error(char const* operation, OSStatus status)
    {
        return { false, "ERR_FIREFLY_GLOBAL_SHORTCUT_NATIVE",
            std::string(operation) + " (OSStatus " + std::to_string(status) + ")" };
    }

    OSStatus event(EventHandlerCallRef, EventRef incoming, void*)
    {
        if (!NSThread.isMainThread || GetEventClass(incoming) != kEventClassKeyboard
            || GetEventKind(incoming) != kEventHotKeyPressed)
            return eventNotHandledErr;
        EventHotKeyID id {};
        auto status = GetEventParameter(incoming, kEventParamDirectObject, typeEventHotKeyID,
            nullptr, sizeof(id), nullptr, &id);
        if (status != noErr || id.signature != signature)
            return eventNotHandledErr;
        auto found = registry().keys.find(id.id);
        if (found == registry().keys.end() || !found->second.active)
            return eventNotHandledErr;
        auto state = found->second.owner.lock();
        if (!state || state->stopped || !state->callback)
            return eventNotHandledErr;
        auto token = found->second.token;
        // Copy before invoking: the callback can unregister itself, stop the provider
        // or create a new one. No map iterator/refcon survives that reentrant call.
        auto callback = state->callback;
        callback(token);
        return noErr;
    }

    void remove_handler_if_empty()
    {
        auto& shared = registry();
        if (shared.keys.empty() && shared.handler && RemoveEventHandler(shared.handler) == noErr)
            shared.handler = nullptr;
    }

    void retry_retired()
    {
        auto& shared = registry();
        for (auto it = shared.keys.begin(); it != shared.keys.end();) {
            auto owner = it->second.owner.lock();
            if (!owner || owner->stopped)
                it->second.active = false;
            // A live owner's failed unregister remains its explicit retry; do
            // not make a later unrelated registration silently remove ownership.
            if (it->second.active || (owner && !owner->stopped)) {
                ++it;
                continue;
            }

            if (UnregisterEventHotKey(it->second.ref) == noErr)
                it = shared.keys.erase(it);
            else
                ++it;
        }

        remove_handler_if_empty();
    }

    struct Key {
        UInt32 code {};
        UInt32 extra {};
    };

    std::optional<Key> special(std::string const& value)
    {
        static std::unordered_map<std::string, UInt32> const keys { { "Space", kVK_Space },
            { "Tab", kVK_Tab }, { "Enter", kVK_Return }, { "Escape", kVK_Escape },
            { "Backspace", kVK_Delete }, { "Delete", kVK_ForwardDelete }, { "Insert", kVK_Help },
            { "Home", kVK_Home }, { "End", kVK_End }, { "PageUp", kVK_PageUp },
            { "PageDown", kVK_PageDown }, { "Up", kVK_UpArrow }, { "Down", kVK_DownArrow },
            { "Left", kVK_LeftArrow }, { "Right", kVK_RightArrow }, { "F1", kVK_F1 },
            { "F2", kVK_F2 }, { "F3", kVK_F3 }, { "F4", kVK_F4 }, { "F5", kVK_F5 },
            { "F6", kVK_F6 }, { "F7", kVK_F7 }, { "F8", kVK_F8 }, { "F9", kVK_F9 },
            { "F10", kVK_F10 }, { "F11", kVK_F11 }, { "F12", kVK_F12 }, { "F13", kVK_F13 },
            { "F14", kVK_F14 }, { "F15", kVK_F15 }, { "F16", kVK_F16 }, { "F17", kVK_F17 },
            { "F18", kVK_F18 }, { "F19", kVK_F19 }, { "F20", kVK_F20 } };
        auto found = keys.find(value);
        return found == keys.end() ? std::optional<Key> {} : Key { found->second, 0 };
    }

    Result resolve(Chord const& chord, Key& key)
    {
        if (auto named = special(chord.key)) {
            key = *named;
            return { true, {}, {} };
        }

        if (chord.key.size() != 1 || static_cast<unsigned char>(chord.key[0]) < 33
            || static_cast<unsigned char>(chord.key[0]) > 126)
            return error("ERR_FIREFLY_UNSUPPORTED_API",
                "Global shortcut key is unsupported (including media/AltGr keys)");
        auto wanted = chord.key[0];
        bool letter = (wanted >= 'A' && wanted <= 'Z') || (wanted >= 'a' && wanted <= 'z');
        if (wanted >= 'A' && wanted <= 'Z')
            wanted += 'a' - 'A';
        auto source = TISCopyCurrentKeyboardLayoutInputSource();
        if (!source)
            return error(
                "ERR_FIREFLY_GLOBAL_SHORTCUT_LAYOUT", "Selected keyboard layout unavailable");
        auto data = static_cast<CFDataRef>(
            TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
        if (!data || CFGetTypeID(data) != CFDataGetTypeID() || CFDataGetLength(data) == 0) {
            CFRelease(source);
            return error("ERR_FIREFLY_GLOBAL_SHORTCUT_LAYOUT",
                "Selected input source has no Unicode keyboard layout");
        }

        auto layout = reinterpret_cast<UCKeyboardLayout const*>(CFDataGetBytePtr(data));
        bool translated = false;
        // No US-table fallback. Punctuation/digits may require Shift/Option on the
        // selected layout. Explicit modifiers remain additional physical modifiers.
        // Letters ignore Shift/Option casing; Command-aware layout mappings support
        // selected non-Latin layouts whose command layer exposes Latin shortcuts.
        UInt32 command = chord.modifiers & 8 ? cmdKey : 0;
        UInt32 alternatives[] { command, command | shiftKey, command | optionKey,
            command | shiftKey | optionKey };
        for (auto modifiers : alternatives) {
            if (letter && (modifiers & optionKey))
                continue;
            for (UInt16 code = 0; code < 128; ++code) {
                UInt32 dead {};
                UniCharCount length {};
                UniChar output[4] {};
                auto status = UCKeyTranslate(layout, code, kUCKeyActionDown,
                    (modifiers >> 8) & 0xff, LMGetKbdType(), 1 << kUCKeyTranslateNoDeadKeysBit,
                    &dead, 4, &length, output);
                if (status != noErr)
                    continue;
                translated = true;
                if (length != 1)
                    continue;
                auto character = output[0];
                if (letter && character >= 'A' && character <= 'Z')
                    character += 'a' - 'A';
                if (character != static_cast<unsigned char>(wanted))
                    continue;
                key = { code, letter ? 0u : modifiers & (shiftKey | optionKey) };
                CFRelease(source);
                return { true, {}, {} };
            }
        }

        CFRelease(source);
        return error(
            translated ? "ERR_FIREFLY_UNSUPPORTED_API" : "ERR_FIREFLY_GLOBAL_SHORTCUT_LAYOUT",
            "Key cannot be resolved on the selected keyboard layout");
    }

    UInt32 carbon_modifiers(unsigned bits)
    {
        return (bits & 1 ? shiftKey : 0) | (bits & 2 ? controlKey : 0) | (bits & 4 ? optionKey : 0)
            | (bits & 8 ? cmdKey : 0);
    }

    class MacProvider final : public Provider {
    public:
        explicit MacProvider(EventCallback callback)
            : state(std::make_shared<State>())
        {
            state->callback = std::move(callback);
        }

        ~MacProvider() override
        {
            stop();
        }

        Result register_shortcut(int token, Chord const& chord) override
        {
            if (state->stopped)
                return error("ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped");
            if (!NSThread.isMainThread)
                return error("ERR_FIREFLY_GLOBAL_SHORTCUT_NATIVE",
                    "Global shortcuts require the native owner thread");
            if (token <= state->last || token <= 0 || (chord.modifiers & ~15u) || !state->callback)
                return error("ERR_INVALID_ARG_VALUE", "Invalid global shortcut token/modifiers");
            state->last = token;
            Key key;
            auto mapped = resolve(chord, key);
            if (state->stopped)
                return error(
                    "ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped during layout resolution");
            if (!mapped.code.empty())
                return mapped;
            auto modifiers = carbon_modifiers(chord.modifiers) | key.extra;
            retry_retired();
            auto& shared = registry();
            for (auto const& [id, entry] : shared.keys) {
                (void)id;
                if (entry.keycode == key.code && entry.modifiers == modifiers)
                    return { false, {}, {} };
            }

            if (shared.keys.size() >= 64 || shared.next > 0x7fffffffu)
                return error("ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT",
                    "Native global shortcut ownership limit reached");
            auto target = GetApplicationEventTarget();
            if (!target)
                return error(
                    "ERR_FIREFLY_GLOBAL_SHORTCUT_NATIVE", "Application event target unavailable");
            if (!shared.handler) {
                EventTypeSpec type { kEventClassKeyboard, kEventHotKeyPressed };
                auto status
                    = InstallEventHandler(target, event, 1, &type, nullptr, &shared.handler);
                if (status != noErr)
                    return native_error("InstallEventHandler", status);
            }

            if (state->stopped) {
                remove_handler_if_empty();
                return error("ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped during admission");
            }

            EventHotKeyRef ref {};
            EventHotKeyID id { signature, shared.next++ };
            auto status
                = RegisterEventHotKey(key.code, modifiers, id, target, kEventHotKeyExclusive, &ref);
            if (status != noErr || !ref || state->stopped) {
                if (ref) {
                    // Even an unusual partial native failure/reentrant stop may
                    // leave ownership. Retire it before cleanup; failed removal
                    // remains charged to the bounded registry until retry.
                    auto removed = UnregisterEventHotKey(ref);
                    if (removed != noErr) {
                        shared.keys.emplace(
                            id.id, Registration { state, token, key.code, modifiers, ref, false });
                        state->owned.emplace(token, id.id);
                    }
                }

                remove_handler_if_empty();
                if (state->stopped)
                    return error(
                        "ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped during registration");
                return { false, {}, {} };
            }

            shared.keys.emplace(
                id.id, Registration { state, token, key.code, modifiers, ref, true });
            state->owned.emplace(token, id.id);
            return { true, {}, {} };
        }

        Result unregister_shortcut(int token) override
        {
            if (state->stopped)
                return error("ERR_FIREFLY_APP_EXITED", "Global shortcuts stopped");
            if (!NSThread.isMainThread)
                return error("ERR_FIREFLY_GLOBAL_SHORTCUT_NATIVE",
                    "Global shortcuts require the native owner thread");
            auto found = state->owned.find(token);
            if (found == state->owned.end())
                return { false, {}, {} };
            auto id = found->second;
            auto native = registry().keys.find(id);
            if (native == registry().keys.end()) {
                state->owned.erase(token);
                return { false, {}, {} };
            }

            native->second.active = false;
            auto status = UnregisterEventHotKey(native->second.ref);
            if (status != noErr)
                return native_error("UnregisterEventHotKey", status);
            registry().keys.erase(id);
            state->owned.erase(token);
            remove_handler_if_empty();
            return { true, {}, {} };
        }

        void stop() override
        {
            state->stopped = true;
            // Carbon is main-thread-only. Off-owner destruction can still
            // atomically revoke callbacks; a later owner admission retries its
            // bounded retired native handles, without a dangling provider pointer.
            if (!NSThread.isMainThread)
                return;
            state->callback = {};
            for (auto const& [token, id] : state->owned) {
                (void)token;
                auto found = registry().keys.find(id);
                if (found != registry().keys.end())
                    found->second.active = false;
            }

            state->owned.clear();
            if (NSThread.isMainThread)
                retry_retired();
        }

    private:
        std::shared_ptr<State> state;
    };
}

std::unique_ptr<Provider> Provider::create(EventCallback callback)
{
    return std::make_unique<MacProvider>(std::move(callback));
}
}
