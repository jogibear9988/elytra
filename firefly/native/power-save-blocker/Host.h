/** Main-thread, VM-independent ownership of native sleep inhibition requests. */
#pragma once
#include <memory>

namespace JS {
class Realm;
class Object;
}

namespace Firefly::PowerSaveBlocker {
class Controller {
public:
    struct State;
    explicit Controller(std::shared_ptr<State>);

    ~Controller();

    void stop();

private:
    std::shared_ptr<State> m_state;
};

std::shared_ptr<Controller> install_host(JS::Realm&, JS::Object&);
}
