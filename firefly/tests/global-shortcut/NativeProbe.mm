/** Opt-in actual Carbon ownership acceptance. No window, key injection, callback claim or AX
 * request. */
#include "native/global-shortcut/Provider.h"
#import <Foundation/Foundation.h>
#include <cstdio>

int main()
{
    @autoreleasepool {
        using Firefly::GlobalShortcut::Chord;
        using Firefly::GlobalShortcut::Provider;
        // Never try a different user shortcut or a non-exclusive registration if
        // unavailable. Ordinary F19 avoids the media/Accessibility path entirely.
        Chord const chord { "F19", 1 | 2 | 4 };
        unsigned callbacks {};
        auto first = Provider::create([&](int) { ++callbacks; });
        auto second = Provider::create([&](int) { ++callbacks; });
        auto acquire = first->register_shortcut(1, chord);
        if (!acquire.code.empty() || !acquire.value) {
            fprintf(stderr,
                "Reserved Ctrl+Alt+Shift+F19 unavailable; no alternative/stealing attempted: %s "
                "%s\n",
                acquire.code.c_str(), acquire.message.c_str());
            return 1;
        }

        auto duplicate = second->register_shortcut(1, chord);
        if (!duplicate.code.empty() || duplicate.value)
            return 1;
        auto released = first->unregister_shortcut(1);
        if (!released.code.empty() || !released.value)
            return 1;
        auto transferred = second->register_shortcut(2, chord);
        if (!transferred.code.empty() || !transferred.value)
            return 1;
        second->stop();
        auto recovered = first->register_shortcut(2, chord);
        if (!recovered.code.empty() || !recovered.value)
            return 1;
        first->stop();
        if (callbacks) {
            fprintf(stderr, "Unexpected actual keypress during ownership-only probe\n");
            return 1;
        }

        puts("PASS actual exclusive Carbon Ctrl+Alt+Shift+F19 ownership, physical-duplicate "
             "refusal, unregister/reacquire and stop cleanup; no window, key injection, callback "
             "delivery or AX claim");
        return 0;
    }
}
