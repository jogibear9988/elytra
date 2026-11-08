/** Passive OS power observations; no sleep blockers, input capture or VM/Qt objects. */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Firefly::PowerMonitor {
enum class Query { IdleTime, IdleState, ThermalState, BatteryPower, SpeedLimit };

struct Result {
    double number {};
    std::string text;
    bool flag {};
    std::string code;
    std::string message;
};

Result query(Query, int threshold = 60);

struct Event {
    std::string type;
    std::string state;
    int limit { -1 };
};

using EventCallback = std::function<void(Event)>;

class Provider {
public:
    static std::shared_ptr<Provider> create(EventCallback);
    // Snapshot of native event availability; never invents CPU limit support.
    static std::vector<std::string> capabilities();

    virtual ~Provider() = default;
    // Called on the main owner thread. Failure rolls back partial subscriptions.
    virtual Result start() = 0;
    virtual void stop() = 0;
};
}
