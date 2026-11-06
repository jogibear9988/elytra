/** Owns the libuv check-phase FIFO and its rooted JavaScript callbacks. */
#pragma once
#include <memory>

namespace JS {
class VM;
class Realm;
}

namespace Mite {
class EventLoop;

class ImmediateQueue {
public:
    ImmediateQueue(EventLoop&, JS::VM&);

    ~ImmediateQueue();

    void install(JS::Realm&);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
