/** Plain owner-thread borrowing; configuration outlives every callback from the genuine Hub. */
#include "Lifecycle.h"

namespace Firefly::HubLifecycle {
namespace {
    Configuration* configuration;
}

void install(Configuration& value)
{
    VERIFY(!configuration);
    VERIFY(!value.binary_directory.is_empty());
    configuration = &value;
}

void clear(Configuration& value)
{
    VERIFY(configuration == &value);
    configuration = nullptr;
}

Core::EventLoop* embedded_event_loop()
{
    return configuration ? &configuration->loop : nullptr;
}

Optional<ByteString> embedded_binary_directory()
{
    if (!configuration)
        return {};
    return configuration->binary_directory;
}
}
