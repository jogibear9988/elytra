/** Native notification scheduling and response events; never carries VM or Qt objects. */
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Firefly::Notifications {
struct Options {
    std::string title;
    std::string body;
    bool silent { false };
    std::vector<std::string> actions;
};

struct Event {
    uint64_t token;
    std::string type;
    std::string message;
    int index { -1 };
};

using EventCallback = std::function<void(Event)>;

class Provider {
public:
    // Read-only OS/bundle/delegate preflight, not notification authorization.
    static bool supported();

    static std::shared_ptr<Provider> create(EventCallback);

    virtual ~Provider() = default;
    // show may request OS authorization. 'show' acknowledges successful native
    // submission, not visible presentation. Native callbacks carry plain data.
    virtual void show(uint64_t, Options) = 0;
    // Removal is best effort; an already displayed OS notification cannot be
    // synchronously rolled back. Late add completions remove their request again.
    virtual void close(uint64_t) = 0;
    virtual void stop() = 0;
};
}
