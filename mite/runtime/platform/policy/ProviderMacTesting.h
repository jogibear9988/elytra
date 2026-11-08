/** Compile-time-only native peers exercise the production reader/queue/lifetime core. */
#pragma once
#if !defined(__APPLE__) || !defined(MITE_POLICY_PROVIDER_TESTING)
#error Policy testing peers are unavailable in production builds
#endif
#include "Provider.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <functional>
#include <memory>

namespace Mite::Policy {
class PreferencesPeer {
public:
    virtual ~PreferencesPeer() = default;
    // Returned values follow CF's Copy ownership convention.
    virtual CFPropertyListRef copy_value(CFStringRef key, CFStringRef product) = 0;
    virtual bool is_forced(CFStringRef key, CFStringRef product) = 0;
    virtual ErrorOr<void> start(dispatch_queue_t, std::function<void()> changed) = 0;
    virtual void stop() = 0;
};

ErrorOr<NonnullOwnPtr<Provider>> create_provider_for_testing(
    ByteString product, Vector<Definition>, std::shared_ptr<PreferencesPeer>);
}
