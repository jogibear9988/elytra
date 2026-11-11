/** Actual TIS source and Carbon key translation, using the pinned Chromium physical-code table.
 * The mapping data license is retained in vendor/native-keymap/ThirdPartyNotices.txt.
 */
#include "Provider.h"
#include <AK/ScopeGuard.h>
#include <AK/String.h>
#include <Carbon/Carbon.h>
#include <cstring>
#include <vector>

namespace Mite::Keyboard {
namespace {
    struct Key {
        unsigned native;
        char const* code;
    };

#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) { mac, code }
#define DOM_CODE_DECLARATION static constexpr Key keys[] =
#include "../../../../vendor/native-keymap/deps/chromium/dom_code_data.inc"
#undef DOM_CODE_DECLARATION
#undef DOM_CODE

    ErrorOr<String> text(CFStringRef value)
    {
        if (!value || CFGetTypeID(value) != CFStringGetTypeID())
            return Error::from_string_literal("Keyboard source text is unavailable");
        auto length = CFStringGetLength(value);
        if (length > 4096)
            return Error::from_string_literal("Keyboard source text exceeds limit");
        auto maximum = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
        std::vector<UInt8> bytes(static_cast<size_t>(maximum) + 1);
        CFIndex used {};
        if (CFStringGetBytes(value, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, false,
                bytes.data(), maximum, &used)
            != length)
            return Error::from_string_literal("Keyboard source text is not valid UTF-8");
        auto result
            = TRY(String::from_utf8(StringView(reinterpret_cast<char const*>(bytes.data()), used)));
        if (result.contains('\0'))
            return Error::from_string_literal("Keyboard source text contains NUL");
        return result;
    }

    struct Translation {
        String value;
        bool dead;
    };

    ErrorOr<Translation> translate(UCKeyboardLayout const* layout, unsigned code, UInt32 modifiers)
    {
        UInt32 dead {};
        UniChar chars[8] {};
        UniCharCount length {};
        // Keep native dead-key state local to this query; no keyboard input is synthesized.
        auto status = UCKeyTranslate(layout, static_cast<UInt16>(code), kUCKeyActionDown,
            (modifiers >> 8) & 0xff, LMGetKbdType(), 0, &dead, 8, &length, chars);
        if (status != noErr)
            return Error::from_string_literal("Native keyboard translation failed");
        bool is_dead = !length && dead;
        if (is_dead) {
            status = UCKeyTranslate(layout, static_cast<UInt16>(code), kUCKeyActionDown,
                (modifiers >> 8) & 0xff, LMGetKbdType(), 0, &dead, 8, &length, chars);
            if (status != noErr)
                return Error::from_string_literal("Native dead-key translation failed");
        }

        if (!length || (length == 1 && (chars[0] < 32 || chars[0] == 127)))
            return Translation { String {}, false };
        auto value = CFStringCreateWithCharacters(kCFAllocatorDefault, chars, length);
        if (!value)
            return Error::from_string_literal("Native keyboard text allocation failed");
        ScopeGuard release = [&] { CFRelease(value); };
        return Translation { TRY(text(value)), is_dead };
    }
}

ErrorOr<JsonObject> snapshot()
{
    auto selected = TISCopyCurrentKeyboardInputSource();
    if (!selected)
        return Error::from_string_literal("Selected keyboard input source is unavailable");
    ScopeGuard release_selected = [&] { CFRelease(selected); };
    JsonObject info;
    info.set("id"sv,
        TRY(text(static_cast<CFStringRef>(
            TISGetInputSourceProperty(selected, kTISPropertyInputSourceID)))));
    info.set("localizedName"sv,
        TRY(text(static_cast<CFStringRef>(
            TISGetInputSourceProperty(selected, kTISPropertyLocalizedName)))));
    auto languages = static_cast<CFArrayRef>(
        TISGetInputSourceProperty(selected, kTISPropertyInputSourceLanguages));
    if (!languages || CFGetTypeID(languages) != CFArrayGetTypeID() || !CFArrayGetCount(languages))
        return Error::from_string_literal("Selected keyboard input language is unavailable");
    info.set("lang"sv, TRY(text(static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, 0)))));

    auto layout_source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!layout_source)
        return Error::from_string_literal("Selected Unicode keyboard layout is unavailable");
    ScopeGuard release_layout = [&] { CFRelease(layout_source); };
    auto data = static_cast<CFDataRef>(
        TISGetInputSourceProperty(selected, kTISPropertyUnicodeKeyLayoutData));
    if (!data)
        data = static_cast<CFDataRef>(
            TISGetInputSourceProperty(layout_source, kTISPropertyUnicodeKeyLayoutData));
    if (!data || CFGetTypeID(data) != CFDataGetTypeID()
        || CFDataGetLength(data) < static_cast<CFIndex>(sizeof(UCKeyboardLayout)))
        return Error::from_string_literal("Selected source has no Unicode keyboard data");
    auto layout = reinterpret_cast<UCKeyboardLayout const*>(CFDataGetBytePtr(data));
    JsonObject mapping;
    for (auto const& key : keys) {
        if (!key.code || key.native >= 0xffff)
            continue;
        JsonObject entry;
        StringView names[] { "value"sv, "withShift"sv, "withAltGr"sv, "withShiftAltGr"sv };
        char const* dead_names[] { "valueIsDeadKey", "withShiftIsDeadKey", "withAltGrIsDeadKey",
            "withShiftAltGrIsDeadKey" };

        UInt32 modifiers[] { 0, shiftKey, optionKey, shiftKey | optionKey };
        for (size_t index = 0; index < 4; ++index) {
            auto translated = TRY(translate(layout, key.native, modifiers[index]));
            entry.set(names[index], move(translated.value));
            entry.set(StringView(dead_names[index], strlen(dead_names[index])), translated.dead);
        }

        mapping.set(StringView(key.code, strlen(key.code)), move(entry));
    }

    JsonObject result;
    result.set("layout"sv, move(info));
    result.set("map"sv, move(mapping));
    result.set("iso"sv, KBGetLayoutType(LMGetKbdType()) == kKeyboardISO);
    return result;
}
}
