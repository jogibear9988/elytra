/** Owner-thread request admission and weak native completion routing, independent of VM lifetime.
 */
#pragma once
#include <functional>
#include <memory>
#include <string>

class QObject;

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Shell {
class Controller {
public:
    struct State;
    explicit Controller(std::shared_ptr<State>);

    ~Controller();

    void stop();

private:
    std::shared_ptr<State> m_state;
};

std::shared_ptr<Controller> install_host(
    JS::Realm&, JS::Object&, QObject&, std::function<void(std::string)>);
}
