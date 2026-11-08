/** Modern passive Network.framework path snapshots and bounded system-proxy inspection. */
#include "NetworkState.h"
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <cmath>
#include <mutex>
#include <optional>

namespace Firefly {
namespace {
    struct State {
        std::mutex mutex;
        bool stopped {};
        std::optional<bool> online;
        bool invalid {};
    };

    NetworkStateResult failure(char const* code, char const* message)
    {
        return { false, code, message };
    }

    struct ProxySnapshot {
        size_t budget { 4096 };
        size_t byte_budget { 1048576 };
        bool configured {};
        bool valid { true };

        bool text(NSString* value, size_t limit)
        {
            if (value.length > limit || ![value canBeConvertedToEncoding:NSUTF8StringEncoding])
                return false;
            auto bytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
            if (bytes > limit || bytes > byte_budget)
                return false;
            byte_budget -= bytes;
            return true;
        }

        void visit(id value, unsigned depth = 0)
        {
            if (!valid || depth > 8 || !budget) {
                valid = false;
                return;
            }

            --budget;
            if (![value isKindOfClass:NSDictionary.class]) {
                valid = false;
                return;
            }

            if ([value count] > budget) {
                valid = false;
                return;
            }

            for (id key in value) {
                if (!valid || !budget) {
                    valid = false;
                    return;
                }

                --budget;
                if (![key isKindOfClass:NSString.class] || !text(key, 1024)) {
                    valid = false;
                    return;
                }

                id item = [value objectForKey:key];
                if ([key isEqual:@"__SCOPED__"]) {
                    if (![item isKindOfClass:NSDictionary.class] || [item count] > budget) {
                        valid = false;
                        return;
                    }

                    for (id interface in item) {
                        if (!valid || !budget) {
                            valid = false;
                            return;
                        }

                        if (![interface isKindOfClass:NSString.class] || !text(interface, 1024)) {
                            valid = false;
                            return;
                        }

                        visit([item objectForKey:interface], depth + 1);
                    }
                } else if ([key isEqual:@"__SUPPLEMENTAL__"]) {
                    if (![item isKindOfClass:NSArray.class] || [item count] > budget) {
                        valid = false;
                        return;
                    }

                    for (id configuration in item)
                        visit(configuration, depth + 1);
                } else if ([key hasSuffix:@"Enable"]
                    && ([key isEqual:@"HTTPEnable"] || [key isEqual:@"HTTPSEnable"] ||
                        [key isEqual:@"SOCKSEnable"] || [key isEqual:@"FTPEnable"] ||
                        [key isEqual:@"GopherEnable"] || [key isEqual:@"RTSPEnable"] ||
                        [key isEqual:@"ProxyAutoConfigEnable"] ||
                        [key isEqual:@"ProxyAutoDiscoveryEnable"])) {
                    if (![item isKindOfClass:NSNumber.class] || !std::isfinite([item doubleValue])
                        || ([item doubleValue] != 0 && [item doubleValue] != 1)) {
                        valid = false;
                        return;
                    }

                    configured = configured || [item boolValue];
                } else if ([key isEqual:@"ExceptionsList"] ||
                    [key isEqual:@"SupplementalMatchDomains"] ||
                    [key isEqual:@"SupplementalMatchOrders"]) {
                    if (![item isKindOfClass:NSArray.class] || [item count] > budget) {
                        valid = false;
                        return;
                    }

                    for (id entry in item) {
                        if (!valid || !budget) {
                            valid = false;
                            return;
                        }

                        --budget;
                        if ([entry isKindOfClass:NSString.class]) {
                            if (!text(entry, 65536)) {
                                valid = false;
                                return;
                            }
                        } else if (![entry isKindOfClass:NSNumber.class]
                            || !std::isfinite([entry doubleValue])) {
                            valid = false;
                            return;
                        }
                    }
                } else if ([key isEqual:@"ExcludeSimpleHostnames"] || [key isEqual:@"FTPPassive"]
                    || ([key isEqual:@"HTTPPort"] || [key isEqual:@"HTTPSPort"] ||
                        [key isEqual:@"SOCKSPort"] || [key isEqual:@"FTPPort"] ||
                        [key isEqual:@"GopherPort"] || [key isEqual:@"RTSPPort"])
                    || [key isEqual:@"__FLAGS__"] || [key isEqual:@"__ORDER__"]) {
                    if (![item isKindOfClass:NSNumber.class]
                        || !std::isfinite([item doubleValue])) {
                        valid = false;
                        return;
                    }
                } else if ([key isEqual:@"HTTPProxy"] || [key isEqual:@"HTTPSProxy"] ||
                    [key isEqual:@"SOCKSProxy"] || [key isEqual:@"FTPProxy"] ||
                    [key isEqual:@"GopherProxy"] || [key isEqual:@"RTSPProxy"] ||
                    [key isEqual:@"ProxyAutoConfigURLString"] ||
                    [key isEqual:@"ProxyAutoConfigJavaScript"]) {
                    if (![item isKindOfClass:NSString.class] || !text(item, 65536)) {
                        valid = false;
                        return;
                    }
                } else {
                    // New routing keys cannot silently become a direct connection.
                    valid = false;
                    return;
                }
            }
        }
    };

    class MacNetworkState final : public NetworkState {
    public:
        MacNetworkState()
            : state(std::make_shared<State>())
        {
            monitor = nw_path_monitor_create();
            queue = dispatch_queue_create("org.elytra.network-state", DISPATCH_QUEUE_SERIAL);
            if (!monitor || !queue) {
                state->invalid = true;
                return;
            }

            auto weak = std::weak_ptr(state);
            nw_path_monitor_set_update_handler(monitor, ^(nw_path_t path) {
                auto owner = weak.lock();
                if (!owner)
                    return;
                std::lock_guard lock(owner->mutex);
                if (owner->stopped)
                    return;
                switch (nw_path_get_status(path)) {
                case nw_path_status_satisfied:
                case nw_path_status_satisfiable:
                    // Satisfiable permits attachment on a future connection attempt;
                    // neither true state guarantees access to an Internet destination.
                    owner->online = true;
                    owner->invalid = false;
                    break;
                case nw_path_status_unsatisfied:
                    owner->online = false;
                    owner->invalid = false;
                    break;
                default:
                    owner->online.reset();
                    owner->invalid = true;
                    break;
                }
            });
            nw_path_monitor_set_queue(monitor, queue);
            nw_path_monitor_start(monitor);
        }

        ~MacNetworkState() override
        {
            stop();
        }

        NetworkStateResult is_online() override
        {
            std::lock_guard lock(state->mutex);
            if (state->stopped)
                return failure("ERR_FIREFLY_APP_EXITED", "Network state stopped");
            if (state->invalid)
                return failure(
                    "ERR_FIREFLY_NETWORK_STATE_DATA", "Native network state unavailable");
            if (!state->online.has_value())
                return failure(
                    "ERR_FIREFLY_NETWORK_STATE_PENDING", "Initial native network path is pending");
            return { *state->online, {}, {} };
        }

        NetworkStateResult has_proxy() override
        {
            {
                std::lock_guard lock(state->mutex);
                if (state->stopped)
                    return failure("ERR_FIREFLY_APP_EXITED", "Network state stopped");
            }

            @autoreleasepool {
                @try {
                    auto snapshot = SCDynamicStoreCopyProxies(nullptr);
                    if (!snapshot)
                        return failure("ERR_FIREFLY_NETWORK_STATE_DATA",
                            "System proxy configuration unavailable");
                    NSDictionary* value = CFBridgingRelease(snapshot);
                    ProxySnapshot parsed;
                    parsed.visit(value);
                    if (!parsed.valid)
                        return failure("ERR_FIREFLY_NETWORK_STATE_DATA",
                            "System proxy configuration exceeds supported bounds or schema");
                    return { parsed.configured, {}, {} };
                } @catch (NSException*) {
                    return failure(
                        "ERR_FIREFLY_NETWORK_STATE_DATA", "System proxy configuration unavailable");
                }
            }
        }

        void stop() override
        {
            {
                std::lock_guard lock(state->mutex);
                if (state->stopped)
                    return;
                state->stopped = true;
                state->online.reset();
            }

            if (monitor)
                nw_path_monitor_cancel(monitor);
        }

    private:
        std::shared_ptr<State> state;
        nw_path_monitor_t monitor {};
        dispatch_queue_t queue {};
    };
}

std::unique_ptr<NetworkState> NetworkState::create()
{
    return std::make_unique<MacNetworkState>();
}
}
