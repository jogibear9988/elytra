/** Exact Carbon provider against controlled C symbols. No OS hotkey registration or keypress. */
#include "native/global-shortcut/Provider.h"
#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

using Firefly::GlobalShortcut::Provider;

namespace {
struct Entry {
    UInt32 code;
    UInt32 modifiers;
    EventHotKeyID id;
};

std::unordered_map<EventHotKeyRef, Entry> native_keys;
EventHandlerUPP handler;
EventHotKeyID incoming {};
UInt32 event_class = kEventClassKeyboard;
UInt32 event_kind = kEventHotKeyPressed;
OSStatus event_status = noErr;
OSStatus install_status = noErr;
bool layout_available = true;
bool conflict = false;
bool fail_removal = false;
bool fail_handler_removal = false;
unsigned registrations {}, installations {}, handler_removals {}, translations {};
Entry last {};
NSData* layout_data;
std::function<void()> during_register;
bool passed = true;

void check(bool value, char const* message)
{
    if (!value) {
        fprintf(stderr, "FAIL controlled globalShortcut provider: %s\n", message);
        passed = false;
    }
}

void deliver(EventHotKeyID id)
{
    incoming = id;
    if (handler)
        handler(nullptr, reinterpret_cast<EventRef>(uintptr_t { 1 }), nullptr);
}
}

extern "C" EventTargetRef GetApplicationEventTarget(void)
{
    return reinterpret_cast<EventTargetRef>(uintptr_t { 1 });
}

extern "C" OSStatus InstallEventHandler(EventTargetRef, EventHandlerUPP callback, ItemCount count,
    EventTypeSpec const* types, void* refcon, EventHandlerRef* output)
{
    ++installations;
    check(count == 1 && types[0].eventClass == kEventClassKeyboard
            && types[0].eventKind == kEventHotKeyPressed && !refcon,
        "native app handler exacttype with no provider refcon");
    if (install_status != noErr)
        return install_status;
    handler = callback;
    *output = reinterpret_cast<EventHandlerRef>(uintptr_t { 2 });
    return noErr;
}

extern "C" OSStatus RemoveEventHandler(EventHandlerRef)
{
    ++handler_removals;
    return fail_handler_removal ? static_cast<OSStatus>(paramErr) : static_cast<OSStatus>(noErr);
}

extern "C" UInt32 GetEventClass(EventRef)
{
    return event_class;
}

extern "C" UInt32 GetEventKind(EventRef)
{
    return event_kind;
}

extern "C" OSStatus GetEventParameter(EventRef, EventParamName name, EventParamType type,
    EventParamType*, ByteCount size, ByteCount*, void* output)
{
    check(name == kEventParamDirectObject && type == typeEventHotKeyID && size == sizeof(incoming),
        "exact native event-token extraction");
    if (event_status != noErr)
        return event_status;
    std::memcpy(output, &incoming, sizeof(incoming));
    return noErr;
}

extern "C" OSStatus RegisterEventHotKey(UInt32 code, UInt32 modifiers, EventHotKeyID id,
    EventTargetRef, OptionBits options, EventHotKeyRef* output)
{
    ++registrations;
    check(options == kEventHotKeyExclusive,
        "exclusive native OS ownership, never shared notification success");
    if (conflict)
        return eventHotKeyExistsErr;
    auto ref = reinterpret_cast<EventHotKeyRef>(uintptr_t { 1000 } + id.id);
    *output = ref;
    last = { code, modifiers, id };
    native_keys.emplace(ref, last);
    if (during_register)
        during_register();
    return noErr;
}

extern "C" OSStatus UnregisterEventHotKey(EventHotKeyRef ref)
{
    if (fail_removal)
        return paramErr;
    check(native_keys.erase(ref) == 1, "remove only actual controlled owned native token");
    return noErr;
}

extern "C" TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void)
{
    if (!layout_available)
        return nullptr;
    return reinterpret_cast<TISInputSourceRef>(
        const_cast<void*>(CFRetain((__bridge CFTypeRef)layout_data)));
}

extern "C" void* TISGetInputSourceProperty(TISInputSourceRef, CFStringRef property)
{
    check(
        CFEqual(property, kTISPropertyUnicodeKeyLayoutData), "current TIS Unicode layout queried");
    return (__bridge void*)layout_data;
}

extern "C" UInt8 LMGetKbdType(void)
{
    return 77;
}

extern "C" OSStatus UCKeyTranslate(UCKeyboardLayout const*, UInt16 code, UInt16 action,
    UInt32 modifiers, UInt32 keyboard, OptionBits options, UInt32*, UniCharCount capacity,
    UniCharCount* length, UniChar output[])
{
    ++translations;
    check(action == kUCKeyActionDown && keyboard == 77
            && options == (1u << kUCKeyTranslateNoDeadKeysBit) && capacity >= 1,
        "translation uses real keyboard type and bounded non-dead lookup");
    *length = 1;
    bool shift = modifiers & (shiftKey >> 8);
    bool option = modifiers & (optionKey >> 8);
    // Deliberately not US keycodes: A lives at10, B at11, digit1 requiresShift.
    switch (code) {
    case 10:
        output[0] = shift ? 'A' : 'a';
        break;
    case 11:
        output[0] = shift ? 'B' : 'b';
        break;
    case 12:
        output[0] = shift ? '+' : '=';
        break;
    case 13:
        output[0] = shift ? '1' : '&';
        break;
    case 14:
        output[0] = option ? '?' : '/';
        break;
    default:
        *length = 0;
    }

    return noErr;
}

int main()
{
    @autoreleasepool {
        unsigned char byte = 0;
        layout_data = [NSData dataWithBytes:&byte length:1];
        std::vector<int> pressed;
        auto provider = Provider::create([&](int token) { pressed.push_back(token); });
        check(registrations == 0 && installations == 0,
            "construction registers no physical shortcuts");
        check(provider->register_shortcut(1, { "A", 8 }).value && last.code == 10
                && last.modifiers == cmdKey,
            "selected non-US layout resolves letter withoutUSfallback");
        auto old = last.id;
        deliver(old);
        check(pressed == std::vector<int> { 1 }, "native app event delivers owned logical token");
        auto calls = registrations;
        check(!provider->register_shortcut(2, { "a", 8 }).value && registrations == calls,
            "physical letter alias cannot double-own native chord");
        check(provider->register_shortcut(3, { "+", 2 }).value && last.code == 12
                && last.modifiers == (controlKey | shiftKey),
            "literalPlus uses selected layout requiredShift");
        check(!provider->register_shortcut(4, { "=", 3 }).value,
            "explicitShift/Equals physicalalias conflicts");
        check(provider->register_shortcut(5, { "1", 8 }).value && last.code == 13
                && (last.modifiers & shiftKey),
            "digit mapping follows current layout rather thanUSassumption");
        check(provider->register_shortcut(6, { "?", 2 }).value && last.code == 14
                && (last.modifiers & optionKey),
            "selected layout Option layer can produce punctuation");
        layout_available = false;
        check(
            provider->register_shortcut(7, { "Z", 8 }).code == "ERR_FIREFLY_GLOBAL_SHORTCUT_LAYOUT",
            "missing selected layout explicit");
        check(provider->register_shortcut(8, { "Enter", 8 }).value,
            "specialkey maps withoutinventing textlayout");
        layout_available = true;
        check(provider->register_shortcut(9, { "MediaPlayPause", 8 }).code
                == "ERR_FIREFLY_UNSUPPORTED_API",
            "media/AX capture explicitly unsupported");
        calls = registrations;
        conflict = true;
        check(!provider->register_shortcut(10, { "F1", 8 }).value && registrations == calls + 1,
            "real native conflict returnsfalse");
        conflict = false;
        auto before = pressed.size();
        auto foreign = old;
        foreign.signature ^= 1;
        deliver(foreign);
        event_status = paramErr;
        deliver(old);
        event_status = noErr;
        event_kind = kEventHotKeyReleased;
        deliver(old);
        event_kind = kEventHotKeyPressed;
        check(pressed.size() == before, "foreign/malformed/wrongkind events ignored");
        check(provider->unregister_shortcut(1).value, "unregister releases exact owned ref");
        deliver(old);
        check(pressed.size() == before, "stale event after unregister dropped");
        check(provider->register_shortcut(11, { "A", 8 }).value && last.id.id != old.id,
            "new registration never reuses Carbon eventidentity");
        provider->stop();
        provider->stop();
        deliver(last.id);
        check(pressed.size() == before && native_keys.empty(),
            "stop removes all owned native hotkeys and revokes lateevents");
        check(provider->register_shortcut(12, { "F1", 8 }).code == "ERR_FIREFLY_APP_EXITED",
            "stop permanently revokes admission");
        provider.reset();
        deliver(old);
        check(pressed.size() == before,
            "static nativehandler after providerdestruction cannot retain callbackcontext");

        std::unique_ptr<Provider> reentrant;
        reentrant = Provider::create([&](int) { reentrant->stop(); });
        check(reentrant->register_shortcut(1, { "F2", 8 }).value, "reentrant fixture admitted");
        deliver(last.id);
        check(native_keys.empty(), "stop from native callback is iterator/lifetime safe");
        reentrant.reset();
        auto revoked = Provider::create([](int) { });
        during_register = [&] { revoked->stop(); };
        check(revoked->register_shortcut(1, { "F2", 8 }).code == "ERR_FIREFLY_APP_EXITED"
                && native_keys.empty(),
            "registration that finishes after reentrant stop is cleaned and neverreportedlive");
        during_register = {};
        revoked.reset();

        fail_removal = true;
        for (unsigned index = 0; index < 64; ++index) {
            auto generation = Provider::create([](int) { });
            check(generation
                      ->register_shortcut(1, { "F" + std::to_string(index / 16 + 1), index % 16 })
                      .value,
                "bounded failedcleanup generation admitted");
            generation->stop();
        }

        auto limited = Provider::create([](int) { });
        calls = registrations;
        check(limited->register_shortcut(1, { "F5", 8 }).code == "ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT"
                && registrations == calls,
            "failednativecleanup cannot causeunbounded registrationchurn");
        fail_removal = false;
        check(limited->register_shortcut(2, { "F5", 8 }).value,
            "successful actualcleanup restoresboundedquota");
        limited->stop();
        check(native_keys.empty(), "all actual controllednativehandles released");
        if (!passed)
            return 1;
        puts("PASS exact Carbon globalShortcut provider controlled "
             "selected-layout/exclusive-registration/conflict/event/reentry/stale/cleanup-quota "
             "ownership; no actual OS registration, keypress or AX prompt");
        return 0;
    }
}
