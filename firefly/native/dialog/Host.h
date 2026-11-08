/** Main-process dialog admission and weak owner-thread completion routing. */
#pragma once
#include "Provider.h"
#include <functional>
#include <memory>
#include <string>
class QObject;
class QWidget;

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Dialogs {
using ParentResolver = std::function<QWidget*(int)>;
using ProviderFactory = std::function<std::unique_ptr<Provider>()>;

class Controller {
public:
    struct State;
    explicit Controller(std::shared_ptr<State>);

    ~Controller();

    void stop();
    // GUI dispatch/pumping must defer VM turns throughout a native modal operation.
    bool is_synchronous() const;

    bool has_pending() const;

private:
    std::shared_ptr<State> m_state;
};

std::shared_ptr<Controller> install_host(JS::Realm&, JS::Object&, QObject&,
    std::function<void(std::string)>, ParentResolver, ProviderFactory = {});
}
