/** Native observation only: no JavaScript callbacks or GC roots survive in this state. */
#pragma once
#include <LibWeb/Export.h>
#include <atomic>
#include <cstdint>
#include <memory>

namespace JS {
class VM;
class Realm;
}

namespace Web::HTML {
class MessagePort;
}

namespace Firefly::PortObservation {
struct State {
    explicit State(uint64_t identifier)
        : token(identifier)
    {
    }

    uint64_t const token;
    std::atomic<uint64_t> dequeued { 0 };
    std::atomic<bool> closed { false };
};

// Called only on the creating VM's owner thread, after native VM identity checks.
// The helper must revoke before destroying context. Validators perform no JS/IO.
using Validator = bool (*)(void* context, Web::HTML::MessagePort&, JS::Realm& actual_realm);

WEB_API std::shared_ptr<State> create(JS::VM&, void* context, Validator);

WEB_API bool observe(Web::HTML::MessagePort&, std::shared_ptr<State> const&, bool consume);

WEB_API void revoke(std::shared_ptr<State> const&);
}
