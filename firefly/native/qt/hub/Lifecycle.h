/** Owner-thread embedded Hub configuration for composed upstream startup sources; no Qt or VM
 * dependency. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Optional.h>

namespace Core {
class EventLoop;
}

namespace Firefly::HubLifecycle {
struct Configuration {
    Core::EventLoop& loop;
    ByteString binary_directory;
};

void install(Configuration&);

void clear(Configuration&);

Core::EventLoop* embedded_event_loop();

Optional<ByteString> embedded_binary_directory();
}
