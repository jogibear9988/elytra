/** Exact network-state provider with controlled C API peers; no original monitor or proxy query
 * runs. */
#include "NetworkState.h"
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
NSObject* monitor_peer;
NSObject* path_peer;
nw_path_monitor_update_handler_t update;
NSDictionary* proxy_snapshot;
nw_path_status_t path_status = nw_path_status_invalid;
unsigned starts {}, cancels {}, queries {};
bool passed = true;

void check(bool value, char const* description)
{
    if (!value) {
        fprintf(stderr, "FAIL controlled native network state: %s\n", description);
        passed = false;
    }
}

void deliver(nw_path_status_t status)
{
    path_status = status;
    update((nw_path_t)path_peer);
}
}

extern "C" nw_path_monitor_t nw_path_monitor_create(void)
{
    return (nw_path_monitor_t)monitor_peer;
}

extern "C" void nw_path_monitor_set_update_handler(
    nw_path_monitor_t, nw_path_monitor_update_handler_t value)
{
    update = [value copy];
}

extern "C" void nw_path_monitor_set_queue(nw_path_monitor_t, dispatch_queue_t queue)
{
    if (!queue)
        std::abort();
}

extern "C" void nw_path_monitor_start(nw_path_monitor_t)
{
    ++starts;
}

extern "C" void nw_path_monitor_cancel(nw_path_monitor_t)
{
    ++cancels;
}

extern "C" nw_path_status_t nw_path_get_status(nw_path_t)
{
    return path_status;
}

extern "C" CFDictionaryRef SCDynamicStoreCopyProxies(SCDynamicStoreRef)
{
    ++queries;
    return (__bridge_retained CFDictionaryRef)proxy_snapshot;
}

int main()
{
    @autoreleasepool {
        monitor_peer = [NSObject new];
        path_peer = [NSObject new];
        proxy_snapshot = @ {};
        auto provider = Firefly::NetworkState::create();
        check(starts == 1 && queries == 0,
            "create starts only a passive monitor, no requests or proxy writes");
        check(provider->is_online().code == "ERR_FIREFLY_NETWORK_STATE_PENDING",
            "unknown initial state cannot invent connectivity");
        std::thread worker([] {
            @autoreleasepool {
                deliver(nw_path_status_satisfied);
            }
        });
        worker.join();
        check(provider->is_online().code.empty() && provider->is_online().value,
            "native worker snapshot reaches owner query safely");
        deliver(nw_path_status_unsatisfied);
        check(provider->is_online().code.empty() && !provider->is_online().value,
            "unsatisfied network is offline");
        deliver(nw_path_status_satisfiable);
        check(provider->is_online().value,
            "on-demand attachment is potentially online without starting connection");
        deliver(nw_path_status_invalid);
        check(provider->is_online().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "invalid native path stays explicit");
        check(provider->has_proxy().code.empty() && !provider->has_proxy().value,
            "empty real-shape configuration is direct");
        for (NSString* flag in @[
                 @"HTTPEnable", @"HTTPSEnable", @"SOCKSEnable", @"ProxyAutoConfigEnable",
                 @"ProxyAutoDiscoveryEnable"
             ]) {
            proxy_snapshot = @{flag : @1};
            check(provider->has_proxy().code.empty() && provider->has_proxy().value,
                "enabled system proxy/PAC/WPAD cannot silently become direct");
        }

        proxy_snapshot = @{@"__SCOPED__" : @ { @"en0" : @ { @"HTTPEnable" : @1 } }};
        check(provider->has_proxy().value, "scoped proxy detected");
        proxy_snapshot = @{@"__SUPPLEMENTAL__" : @[ @{@"SOCKSEnable" : @1} ]};
        check(provider->has_proxy().value, "supplemental proxy detected");
        proxy_snapshot = @{
            @"HTTPEnable" : @0,
            @"HTTPProxy" : @"disabled.invalid",
            @"HTTPPort" : @8080,
            @"ExceptionsList" : @[ @"localhost" ],
            @"ExcludeSimpleHostnames" : @1
        };

        check(provider->has_proxy().code.empty() && !provider->has_proxy().value,
            "disabled stored proxy metadata does not activate routing");
        proxy_snapshot = @ { @"HTTPEnable" : @"1" };
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "malformed native enable flag fails closed");
        proxy_snapshot = @{@"FutureProxyEnable" : @1};
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "unknown routing key fails closed");
        auto large = [NSMutableArray array];
        for (unsigned index = 0; index < 4097; ++index)
            [large addObject:@"entry"];
        proxy_snapshot = @ { @"ExceptionsList" : large };
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA", "node count bounded");
        id nested = @ {};
        for (unsigned index = 0; index < 10; ++index)
            nested = @{@"__SUPPLEMENTAL__" : @[ nested ]};
        proxy_snapshot = nested;
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "recursive snapshots bounded");
        auto huge = [@"x" stringByPaddingToLength:65537 withString:@"x" startingAtIndex:0];
        proxy_snapshot = @ { @"HTTPProxy" : huge };
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "individual string bounded");
        auto aggregate = [NSMutableArray array];
        auto piece = [@"x" stringByPaddingToLength:65536 withString:@"x" startingAtIndex:0];
        for (unsigned index = 0; index < 17; ++index)
            [aggregate addObject:piece];
        proxy_snapshot = @ { @"ExceptionsList" : aggregate };
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "aggregate proxy string bytes bounded");
        proxy_snapshot = nil;
        check(provider->has_proxy().code == "ERR_FIREFLY_NETWORK_STATE_DATA",
            "missing system snapshot cannot mean no proxy");
        provider->stop();
        provider->stop();
        check(cancels == 1, "stop cancels native monitor exactly once");
        deliver(nw_path_status_satisfied);
        check(provider->is_online().code == "ERR_FIREFLY_APP_EXITED"
                && provider->has_proxy().code == "ERR_FIREFLY_APP_EXITED",
            "late path update cannot revive terminal provider");
        provider.reset();
        deliver(nw_path_status_unsatisfied);
        check(cancels == 1, "late callback after destructor holds only weak native state");
        if (!passed)
            return 1;
        puts("PASS exact native network-state controlled initial/worker/status/stop lifetime and "
             "bounded fail-closed proxy/PAC/WPAD snapshots; no OS connection or configuration "
             "operations");
        return 0;
    }
}
