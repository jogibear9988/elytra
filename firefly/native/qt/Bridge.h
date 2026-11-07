/**
 * Connects Mite main-process APIs to native Qt windows and Ladybird views. Queue callbacks
 * onto the VM owner thread and disconnect them before runtime teardown.
 */

#pragma once
#include <LibGC/Ptr.h>
#include <memory>
#include <optional>

namespace JS {
class VM;
class Realm;
class Object;
}

namespace Core {
class EventLoop;
}

namespace Mite {
class EventLoop;
}

namespace Firefly {
// Construct, call and destroy on the VM/Qt owner thread. VM, realm and both event
// loops outlive the bridge; the private implementation disconnects its hooks first.
class Bridge {
public:
    Bridge(JS::VM&, JS::Realm&, Mite::EventLoop&, Core::EventLoop&, std::optional<int>&);

    ~Bridge();

    // Install/root the trusted main-process host; it is never a renderer capability.
    GC::Ref<JS::Object> exports();

    // Queue lifecycle delivery after the main entry has installed its JS handler.
    void ready();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
