/** Owned global keyboard registrations; native callbacks carry tokens only, never VM objects. */
#pragma once
#include <functional>
#include <memory>
#include <string>

namespace Firefly::GlobalShortcut {
struct Result {
    bool value {};
    std::string code;
    std::string message;
};

struct Chord {
    std::string key;
    // Shift1, Control2, Alt4, Meta/Command8. No media/AltGr or platform fallback.
    unsigned modifiers {};
};

using EventCallback = std::function<void(int)>;

class Provider {
public:
    static std::unique_ptr<Provider> create(EventCallback);

    virtual ~Provider() = default;
    virtual Result register_shortcut(int token, Chord const&) = 0;
    virtual Result unregister_shortcut(int token) = 0;
    virtual void stop() = 0;
};
}
