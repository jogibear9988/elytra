/** Owner-thread global hotkey registrations with weak queued delivery from native providers. */
#pragma once
#include "Provider.h"
#include <functional>
#include <memory>
#include <string>

class QObject;

namespace JS {
class Realm;
class Object;
}

namespace Firefly::GlobalShortcut {
using ProviderFactory = std::function<std::unique_ptr<Provider>(EventCallback)>;

class Controller {
public:
    struct State;
    explicit Controller(std::shared_ptr<State>);

    ~Controller();

    void stop();

private:
    std::shared_ptr<State> m_state;
};

// Factory injection is native test-host only; production checks the actual desktop backend.
std::shared_ptr<Controller> install_host(
    JS::Realm&, JS::Object&, QObject&, std::function<void(std::string)>, ProviderFactory = {});
}
