#pragma once
#include <LibGC/Ptr.h>
#include <memory>
#include <optional>
namespace JS { class VM; class Realm; class Object; }
namespace Core { class EventLoop; }
namespace Mite { class EventLoop; }
namespace Firefly {
class Bridge {
public:
    Bridge(JS::VM&, JS::Realm&, Mite::EventLoop&, Core::EventLoop&, std::optional<int>&);
    ~Bridge();
    GC::Ref<JS::Object> exports();
    void ready();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
