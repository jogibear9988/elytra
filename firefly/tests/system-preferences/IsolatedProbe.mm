/** Opt-in native defaults persistence and process-local notification acceptance.
 * Only the uniquely owned defaults suite is modified. No distributed/workspace
 * notifications, user preferences, authorization requests or GUI operations.
 */
#include "native/system-preferences/Provider.h"
#import <Foundation/Foundation.h>
#include <cstdio>
#import <objc/runtime.h>
#include <vector>

namespace {
NSUserDefaults* isolated_defaults;

id suite_defaults(id, SEL)
{
    return isolated_defaults;
}

class Fixture {
public:
    Fixture()
    {
        suite =
            [@"org.elytra.isolated-provider-probe." stringByAppendingString:NSUUID.UUID.UUIDString];
        isolated_defaults = [[NSUserDefaults alloc] initWithSuiteName:suite];
        method = class_getClassMethod(NSUserDefaults.class, @selector(standardUserDefaults));
        original = method_setImplementation(method, reinterpret_cast<IMP>(suite_defaults));
    }

    ~Fixture()
    {
        cleanup();
    }

    bool cleanup()
    {
        if (cleaned)
            return true;
        // Remove exactly this fixture's domain before restoring the factory.
        [isolated_defaults removePersistentDomainForName:suite];
        bool success = [isolated_defaults synchronize] &&
            [isolated_defaults persistentDomainForName:suite].count == 0;
        method_setImplementation(method, original);
        isolated_defaults = nil;
        cleaned = true;
        return success;
    }

    bool cleaned {};
    NSString* suite;
    Method method;
    IMP original;
};

bool matches(Firefly::SystemPreferences::ValueResult const& result, id expected)
{
    if (!result.code.empty())
        return false;
    auto data = [NSData dataWithBytes:result.json.data() length:result.json.size()];
    id value = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [value isKindOfClass:NSDictionary.class] && [value isEqual:@ { @"value" : expected }];
}

int run()
{
    Fixture fixture;
    if (!isolated_defaults)
        return 1;
    auto provider = Firefly::SystemPreferences::Provider::create();
    if (!provider->set_user_default("ownedBool", "boolean", "true").code.empty()
        || !provider->set_user_default("ownedString", "string", "\"Grüße isolated\"").code.empty()
        || ![isolated_defaults boolForKey:@"ownedBool"]
        || ![[isolated_defaults stringForKey:@"ownedString"] isEqual:@"Grüße isolated"])
        return 1;
    // This explicitly flushes only the probe-owned suite. A fresh native suite
    // instance reads the persistent domain, rather than the provider's cached data.
    if (![isolated_defaults synchronize])
        return 1;
    auto fresh_store = [[NSUserDefaults alloc] initWithSuiteName:fixture.suite];
    auto persisted = [fresh_store persistentDomainForName:fixture.suite];
    if (![persisted[@"ownedBool"] isEqual:@YES]
        || ![persisted[@"ownedString"] isEqual:@"Grüße isolated"])
        return 1;
    isolated_defaults = fresh_store;
    auto fresh_provider = Firefly::SystemPreferences::Provider::create();
    if (!matches(fresh_provider->get_user_default("ownedBool", "boolean"), @YES)
        || !matches(fresh_provider->get_user_default("ownedString", "string"), @"Grüße isolated"))
        return 1;
    std::vector<Firefly::SystemPreferences::Event> events;
    auto name =
        [@"org.elytra.local-provider-probe." stringByAppendingString:NSUUID.UUID.UUIDString];
    auto native_name = std::string(name.UTF8String);
    if (!provider
            ->subscribe(
                0, "local", native_name, [&](auto event) { events.push_back(std::move(event)); })
            .code.empty())
        return 1;
    auto center = NSNotificationCenter.defaultCenter;
    [center postNotificationName:name
                          object:@"owned sender Grüße"
                        userInfo:@{
                            @"owned" : @YES,
                            @"text" : @"Grüße"
                        }];
    if (events.size() != 1 || !events[0].code.empty() || events[0].name != native_name
        || events[0].object != "owned sender Grüße")
        return 1;
    auto data = [NSData dataWithBytes:events[0].user_info_json.data()
                               length:events[0].user_info_json.size()];
    id info = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![info isEqual:@{@"owned" : @YES, @"text" : @"Grüße"}])
        return 1;
    if (!provider->unsubscribe(0, "local").code.empty())
        return 1;
    [center postNotificationName:name object:nil userInfo:nil];
    if (events.size() != 1)
        return 1;
    if (!provider
            ->subscribe(
                1, "local", native_name, [&](auto event) { events.push_back(std::move(event)); })
            .code.empty())
        return 1;
    provider->stop();
    [center postNotificationName:name object:nil userInfo:nil];
    if (events.size() != 1)
        return 1;
    fresh_provider->stop();
    if (!fixture.cleanup())
        return 1;
    puts("PASS actual isolated NSUserDefaults typed suite write/fresh-provider readback and "
         "genuine process-local notification delivery/unsubscribe/stop; owned suite removed, no "
         "personal preferences or distributed/workspace posts");
    return 0;
}
}

int main()
{
    @autoreleasepool {
        @try {
            return run();
        } @catch (NSException* exception) {
            fprintf(stderr, "Isolated native provider probe failed: %s\n",
                exception.description.UTF8String);
            return 1;
        }
    }
}
