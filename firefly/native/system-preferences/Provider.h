/** Native preferences, passive notifications and permission queries; no VM/Qt objects. */
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Firefly::SystemPreferences {
struct Result {
    std::string code;
    std::string message;
};

struct ValueResult {
    std::string code;
    std::string message;
    std::string json;
};

struct Event {
    int id {};
    std::string name;
    std::string user_info_json;
    std::string object;
    std::string code;
    std::string message;
};

using EventCallback = std::function<void(Event)>;

class Provider {
public:
    static std::unique_ptr<Provider> create();

    virtual ~Provider() = default;
    virtual ValueResult get_user_default(std::string const& name, std::string const& type) = 0;
    virtual Result set_user_default(
        std::string const& name, std::string const& type, std::string const& json_value)
        = 0;
    virtual ValueResult media_access_status(std::string const& type) = 0;
    virtual Result subscribe(
        int id, std::string const& kind, std::optional<std::string> const& name, EventCallback)
        = 0;
    virtual Result unsubscribe(int id, std::string const& kind) = 0;
    virtual void stop() = 0;
};
}
