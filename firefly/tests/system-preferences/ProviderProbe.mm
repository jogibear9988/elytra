/** Exact provider with controlled native peers; no preferences writes or permission requests reach
 * macOS. */
#include "native/system-preferences/Provider.h"
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#include <cstdio>
#include <cstdlib>
#import <objc/runtime.h>
#include <thread>
#include <vector>

using Firefly::SystemPreferences::Event;
using Firefly::SystemPreferences::Provider;
using ObserverBlock = void (^)(NSNotification*);
@interface DefaultsPeer : NSObject
@property (nonatomic, strong) NSMutableDictionary* values;
@end
@implementation DefaultsPeer

- (NSString*)stringForKey:(NSString*)key
{
    return self.values[key];
}

- (BOOL)boolForKey:(NSString*)key
{
    return [self.values[key] boolValue];
}

- (void)setBool:(BOOL)value forKey:(NSString*)key
{
    self.values[key] = @(value);
}

- (void)setObject:(id)value forKey:(NSString*)key
{
    self.values[key] = value;
}

@end
@interface CenterPeer : NSObject
@property (nonatomic, strong) NSMutableArray* blocks;
@property (nonatomic, strong) NSMutableArray* names;
@property (nonatomic, strong) NSMutableArray* removed;
@property (nonatomic) BOOL failRemoval;
@end
@implementation CenterPeer

- (id)addObserverForName:(NSString*)name
                  object:(id)object
                   queue:(NSOperationQueue*)queue
              usingBlock:(ObserverBlock)block
{
    if (object || queue)
        std::abort();
    [self.blocks addObject:[block copy]];
    [self.names addObject:name ?: NSNull.null];
    return self.blocks.lastObject;
}

- (void)removeObserver:(id)token
{
    if (self.failRemoval)
        @throw [NSException exceptionWithName:@"ControlledRemovalFailure" reason:nil userInfo:nil];
    [self.removed addObject:token];
}

@end
@interface WorkspacePeer : NSObject
@property (nonatomic, strong) CenterPeer* notificationCenter;
@end
@implementation WorkspacePeer
@end

namespace {
DefaultsPeer* defaults;
CenterPeer* distributed;
CenterPeer* local;
WorkspacePeer* workspace;
AVAuthorizationStatus authorization = AVAuthorizationStatusNotDetermined;
bool screen_allowed = false;
bool passed = true;
std::vector<Event> events;

id defaults_peer(id, SEL)
{
    return defaults;
}

id distributed_peer(id, SEL)
{
    return distributed;
}

id local_peer(id, SEL)
{
    return local;
}

id workspace_peer(id, SEL)
{
    return workspace;
}

AVAuthorizationStatus authorization_peer(id, SEL, AVMediaType)
{
    return authorization;
}

struct Replacement {
    Method method;
    IMP original;
};

std::vector<Replacement> replacements;

void replace(Class type, SEL selector, IMP implementation)
{
    auto method = class_getClassMethod(type, selector);
    if (!method)
        std::abort();
    replacements.push_back({ method, method_setImplementation(method, implementation) });
}

void check(bool value, char const* message)
{
    if (!value) {
        fprintf(stderr, "FAIL systemPreferences provider: %s\n", message);
        passed = false;
    }
}

void collect(Event event)
{
    events.push_back(std::move(event));
}

void deliver(CenterPeer* center, NSUInteger index, NSNotification* notification)
{
    ObserverBlock callback = center.blocks[index];
    callback(notification);
}

CenterPeer* make_center()
{
    auto value = [CenterPeer new];
    value.blocks = [NSMutableArray array];
    value.names = [NSMutableArray array];
    value.removed = [NSMutableArray array];
    return value;
}

void run()
{
    auto provider = Provider::create();
    check(provider->get_user_default("missing", "string").json == "{\"value\":\"\"}",
        "missing string follows native empty default");
    check(provider->get_user_default("missing", "boolean").json == "{\"value\":false}",
        "missing boolean follows native false");
    check(provider->set_user_default("owned", "boolean", "true").code.empty() &&
            [defaults.values[@"owned"] boolValue],
        "boolean writer reaches controlled native peer");
    check(provider->set_user_default("text", "string", "\"Grüße\"").code.empty() &&
            [defaults.values[@"text"] isEqual:@"Grüße"],
        "Unicode typed native string writer");
    check(provider->get_user_default("text", "string").json.find("Grüße") != std::string::npos,
        "Unicode native string read");
    check(provider->set_user_default("owned", "boolean", "1").code == "ERR_INVALID_ARG_TYPE",
        "numeric boolean refused");
    check(provider->get_user_default("owned", "integer").code == "ERR_FIREFLY_UNSUPPORTED",
        "unsupported typed defaults explicit");
    authorization = AVAuthorizationStatusNotDetermined;
    check(provider->media_access_status("camera").json == "{\"value\":\"not-determined\"}",
        "camera status without requesting access");
    authorization = AVAuthorizationStatusDenied;
    check(provider->media_access_status("microphone").json == "{\"value\":\"denied\"}",
        "microphone denied maps native value");
    authorization = AVAuthorizationStatusRestricted;
    check(provider->media_access_status("camera").json == "{\"value\":\"restricted\"}",
        "restricted maps native value");
    authorization = AVAuthorizationStatusAuthorized;
    check(provider->media_access_status("camera").json == "{\"value\":\"granted\"}",
        "authorized maps native value");
    check(provider->media_access_status("screen").json == "{\"value\":\"unknown\"}",
        "false preflight cannot distinguish denied from undetermined");
    screen_allowed = true;
    check(provider->media_access_status("screen").json == "{\"value\":\"granted\"}",
        "true screen preflight means granted");
    check(provider->subscribe(0, "distributed", "owned.event", collect).code.empty(),
        "distributed native observer installed");
    check(provider->subscribe(1, "workspace", std::nullopt, collect).code.empty()
            && workspace.notificationCenter.names[0] == NSNull.null,
        "workspace null subscribes without name filter");
    check(provider->subscribe(2, "local", "local.event", collect).code.empty(),
        "local center supported");
    auto notice = [NSNotification notificationWithName:@"owned.event"
                                                object:@"sender Grüße"
                                              userInfo:@{
                                                  @"nested" : @[ @YES, @2, @"Grüße" ]
                                              }];
    std::thread worker([notice] {
        @autoreleasepool {
            deliver(distributed, 0, notice);
        }
    });
    worker.join();
    check(events.size() == 1 && events[0].id == 0 && events[0].object == "sender Grüße"
            && events[0].user_info_json.find("Grüße") != std::string::npos,
        "worker callback carries bounded native snapshots");
    auto huge = [@"x" stringByPaddingToLength:65537 withString:@"x" startingAtIndex:0];
    deliver(local, 0,
        [NSNotification notificationWithName:@"local.event"
                                      object:@1
                                    userInfo:@ { @"large" : huge }]);
    check(events.back().code == "ERR_FIREFLY_SYSTEM_PREFERENCES_DATA",
        "oversize native snapshot explicit failure");
    auto count = events.size();
    check(provider->unsubscribe(0, "workspace").code.empty() && distributed.removed.count == 0,
        "wrong center cannot remove another observer");
    check(provider->unsubscribe(0, "distributed").code.empty() && distributed.removed.count == 1,
        "owned observer removed from original center");
    deliver(distributed, 0, notice);
    check(events.size() == count, "queued callback after unsubscribe dropped");
    check(provider->subscribe(0, "local", "reuse", collect).code
            == "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
        "subscription identities never reused");
    provider->stop();
    deliver(workspace.notificationCenter, 0, notice);
    check(events.size() == count && local.removed.count == 1
            && workspace.notificationCenter.removed.count == 1,
        "stop removes all observers and retires late events");
    check(provider->media_access_status("camera").code == "ERR_FIREFLY_APP_EXITED",
        "stop permanently revokes reads");
    provider.reset();
    deliver(local, 0, notice);
    check(events.size() == count, "callback after provider destruction cannot retain state");
    local.failRemoval = YES;
    std::vector<std::unique_ptr<Provider>> generations;
    for (int id = 0; id < 64; ++id) {
        auto generation = Provider::create();
        check(generation->subscribe(0, "local", "pending.cleanup", collect).code.empty(),
            "native cleanup reservation admitted");
        generation->stop();
        generations.push_back(std::move(generation));
    }

    auto limited = Provider::create();
    check(limited->subscribe(0, "local", "quota", collect).code
            == "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
        "failed cleanup across generations cannot accumulate unbounded observers");
    local.failRemoval = NO;
    for (auto& generation : generations)
        generation->stop();
    generations.clear();
    check(limited->subscribe(1, "local", "recovered", collect).code.empty(),
        "only successful native cleanup releases observer reservations");
    limited->stop();
    puts("PASS exact macOS systemPreferences controlled defaults, media statuses, native-center "
         "ownership, worker snapshots and teardown");
}
}

// The probe supplies only this read-only C symbol, never the real OS preflight.
extern "C" bool CGPreflightScreenCaptureAccess(void)
{
    return screen_allowed;
}

int main()
{
    @autoreleasepool {
        defaults = [DefaultsPeer new];
        defaults.values = [NSMutableDictionary dictionary];
        distributed = make_center();
        local = make_center();
        workspace = [WorkspacePeer new];
        workspace.notificationCenter = make_center();
        replace(NSUserDefaults.class, @selector(standardUserDefaults),
            reinterpret_cast<IMP>(defaults_peer));
        replace(NSDistributedNotificationCenter.class, @selector(defaultCenter),
            reinterpret_cast<IMP>(distributed_peer));
        replace(NSNotificationCenter.class, @selector(defaultCenter),
            reinterpret_cast<IMP>(local_peer));
        replace(
            NSWorkspace.class, @selector(sharedWorkspace), reinterpret_cast<IMP>(workspace_peer));
        replace(AVCaptureDevice.class, @selector(authorizationStatusForMediaType:),
            reinterpret_cast<IMP>(authorization_peer));
        @try {
            run();
        } @catch (NSException* exception) {
            fprintf(stderr, "Controlled peer failure: %s\n", exception.description.UTF8String);
            passed = false;
        }

        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            method_setImplementation(it->method, it->original);
        return passed ? 0 : 1;
    }
}
