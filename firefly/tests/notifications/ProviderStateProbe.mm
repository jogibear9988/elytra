/** Black-box production macOS provider state tests. Every OS-center entry is intercepted. */
#include "native/notifications/Provider.h"
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#import <objc/runtime.h>
#include <string>
#include <vector>

using Firefly::Notifications::Event;
using Firefly::Notifications::Options;
using Firefly::Notifications::Provider;
using SettingsCallback = void (^)(UNNotificationSettings*);
using AuthorizationCallback = void (^)(BOOL, NSError*);
using CategoriesCallback = void (^)(NSSet<UNNotificationCategory*>*);
using AddCallback = void (^)(NSError*);

@interface TestNotificationBundle : NSObject
@property (nonatomic, readonly) NSString* bundleIdentifier;
@property (nonatomic, readonly) NSURL* bundleURL;
@property (nonatomic, readonly) NSURL* executableURL;
@property (nonatomic, readonly) NSString* bundlePath;
@end
@implementation TestNotificationBundle

- (NSString*)bundleIdentifier
{
    return @"org.elytra.controlled-notification-test";
}

- (NSURL*)bundleURL
{
    return [NSURL fileURLWithPath:@"/controlled/ProviderTest.app"];
}

- (NSURL*)executableURL
{
    return [NSURL fileURLWithPath:@"/controlled/ProviderTest.app/Contents/MacOS/probe"];
}

- (NSString*)bundlePath
{
    return @"/controlled/ProviderTest.app";
}

- (id)objectForInfoDictionaryKey:(NSString*)key
{
    return [key isEqual:@"CFBundlePackageType"] ? @"APPL" : nil;
}

@end

@interface TestNotificationSettings : NSObject
@property (nonatomic) UNAuthorizationStatus authorizationStatus;
@end
@implementation TestNotificationSettings
@end

@interface TestNotification : NSObject
@property (nonatomic, strong) UNNotificationRequest* request;
@end
@implementation TestNotification
@end

@interface TestNotificationResponse : NSObject
@property (nonatomic, strong) TestNotification* notification;
@property (nonatomic, copy) NSString* actionIdentifier;
@end
@implementation TestNotificationResponse
@end

/** Intentionally NSObject, never a real UNUserNotificationCenter instance or subclass. */
@interface TestNotificationCenter : NSObject
@property (nonatomic, weak) id<UNUserNotificationCenterDelegate> delegate;
@property (nonatomic, strong) NSMutableArray* settingsCallbacks;
@property (nonatomic, strong) NSMutableArray* authorizationCallbacks;
@property (nonatomic, strong) NSMutableArray* categoryCallbacks;
@property (nonatomic, strong) NSMutableArray* addCallbacks;
@property (nonatomic, strong) NSMutableArray<UNNotificationRequest*>* requests;
@property (nonatomic, strong) NSMutableArray<NSString*>* removedPending;
@property (nonatomic, strong) NSMutableArray<NSString*>* removedDelivered;
@property (nonatomic, copy) NSSet<UNNotificationCategory*>* categories;
@property (nonatomic) BOOL autoCategories;
@property (nonatomic) NSUInteger authorizationCalls;
@end
@implementation TestNotificationCenter

- (instancetype)init
{
    self = [super init];
    if (self) {
        _settingsCallbacks = [NSMutableArray array];
        _authorizationCallbacks = [NSMutableArray array];
        _categoryCallbacks = [NSMutableArray array];
        _addCallbacks = [NSMutableArray array];
        _requests = [NSMutableArray array];
        _removedPending = [NSMutableArray array];
        _removedDelivered = [NSMutableArray array];
        _categories = [NSSet set];
        _autoCategories = YES;
    }

    return self;
}

- (void)getNotificationSettingsWithCompletionHandler:(SettingsCallback)callback
{
    [self.settingsCallbacks addObject:[callback copy]];
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(AuthorizationCallback)callback
{
    (void)options;
    self.authorizationCalls++;
    [self.authorizationCallbacks addObject:[callback copy]];
}

- (void)getNotificationCategoriesWithCompletionHandler:(CategoriesCallback)callback
{
    if (self.autoCategories)
        callback(self.categories);
    else
        [self.categoryCallbacks addObject:[callback copy]];
}

- (void)setNotificationCategories:(NSSet<UNNotificationCategory*>*)categories
{
    self.categories = categories;
}

- (void)addNotificationRequest:(UNNotificationRequest*)request
         withCompletionHandler:(AddCallback)callback
{
    [self.requests addObject:request];
    [self.addCallbacks addObject:[callback copy]];
}

- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray<NSString*>*)identifiers
{
    [self.removedPending addObjectsFromArray:identifiers];
}

- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray<NSString*>*)identifiers
{
    [self.removedDelivered addObjectsFromArray:identifiers];
}

@end

namespace {
TestNotificationBundle* bundle;
TestNotificationCenter* center;
bool passed = true;

id intercepted_bundle(id, SEL)
{
    return bundle;
}

id intercepted_center(id, SEL)
{
    return center;
}

void check(bool condition, char const* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL notification provider state: %s\n", message);
        passed = false;
    }
}

/** Originals are saved for restoration only. No test path calls either original IMP. */
class Interception {
public:
    Interception()
    {
        m_bundle = class_getClassMethod(NSBundle.class, @selector(mainBundle));
        m_center = class_getClassMethod(
            UNUserNotificationCenter.class, @selector(currentNotificationCenter));
        if (!m_bundle || !m_center)
            std::abort();
        m_original_bundle
            = method_setImplementation(m_bundle, reinterpret_cast<IMP>(intercepted_bundle));
        m_original_center
            = method_setImplementation(m_center, reinterpret_cast<IMP>(intercepted_center));
    }

    ~Interception()
    {
        method_setImplementation(m_center, m_original_center);
        method_setImplementation(m_bundle, m_original_bundle);
    }

private:
    Method m_bundle {}, m_center {};
    IMP m_original_bundle {}, m_original_center {};
};

void settings(UNAuthorizationStatus status)
{
    check(center.settingsCallbacks.count > 0, "controlled settings callback exists");
    if (!center.settingsCallbacks.count)
        return;
    SettingsCallback callback = center.settingsCallbacks.firstObject;
    [center.settingsCallbacks removeObjectAtIndex:0];
    auto* value = [TestNotificationSettings new];
    value.authorizationStatus = status;
    callback(reinterpret_cast<UNNotificationSettings*>(value));
}

void authorization(bool granted)
{
    check(center.authorizationCallbacks.count > 0, "controlled authorization callback exists");
    if (!center.authorizationCallbacks.count)
        return;
    AuthorizationCallback callback = center.authorizationCallbacks.firstObject;
    [center.authorizationCallbacks removeObjectAtIndex:0];
    callback(granted, nil);
}

void added(NSError* error = nil)
{
    check(center.addCallbacks.count > 0, "controlled add callback exists");
    if (!center.addCallbacks.count)
        return;
    AddCallback callback = center.addCallbacks.firstObject;
    [center.addCallbacks removeObjectAtIndex:0];
    callback(error);
}

void drain()
{
    while (center.settingsCallbacks.count)
        settings(UNAuthorizationStatusDenied);
    while (center.authorizationCallbacks.count)
        authorization(false);
    while (center.categoryCallbacks.count) {
        CategoriesCallback callback = center.categoryCallbacks.firstObject;
        [center.categoryCallbacks removeObjectAtIndex:0];
        callback(center.categories);
    }

    while (center.addCallbacks.count)
        added();
}

void reset()
{
    check(center.delegate == nil, "prior provider released its delegate");
    drain();
    [center.requests removeAllObjects];
    [center.removedPending removeAllObjects];
    [center.removedDelivered removeAllObjects];
    center.authorizationCalls = 0;
    center.autoCategories = YES;
}

Options options()
{
    return { "Controlled title", "Controlled body", true, { "First", "Second" } };
}

bool event(std::vector<Event> const& events, uint64_t token, char const* type)
{
    return std::any_of(events.begin(), events.end(),
        [&](auto const& item) { return item.token == token && item.type == type; });
}

void response(UNNotificationRequest* request, NSString* action)
{
    auto* notification = [TestNotification new];
    notification.request = request;
    auto* value = [TestNotificationResponse new];
    value.notification = notification;
    value.actionIdentifier = action;
    __block bool completed = false;
    [center.delegate userNotificationCenter:reinterpret_cast<UNUserNotificationCenter*>(center)
             didReceiveNotificationResponse:reinterpret_cast<UNNotificationResponse*>(value)
                      withCompletionHandler:^{ completed = true; }];
    check(completed, "delegate always acknowledges controlled response");
}

void run()
{
    auto* foreign_category =
        [UNNotificationCategory categoryWithIdentifier:@"foreign.application.category"
                                               actions:@[]
                                     intentIdentifiers:@[]
                                               options:0];
    center.categories = [NSSet setWithObject:foreign_category];
    check(Provider::supported(), "controlled app identity is supported");
    check(center.authorizationCalls == 0 && center.settingsCallbacks.count == 0,
        "support query neither requests authorization nor queries settings");
    std::vector<Event> events;
    auto create = [&] {
        return Provider::create([&](Event value) { events.push_back(std::move(value)); });
    };

    {
        auto provider = create();
        check(center.delegate == nil && center.authorizationCalls == 0,
            "construction has no delegate/auth side effect");
        provider->show(1, options());
        provider->show(2, options());
        check(center.settingsCallbacks.count == 1, "concurrent shows coalesce settings query");
        settings(UNAuthorizationStatusNotDetermined);
        check(center.authorizationCalls == 1, "show explicitly requests authorization once");
        provider->show(3, options());
        check(center.authorizationCalls == 1, "shows during authorization do not prompt again");
        authorization(true);
        check(center.requests.count == 3 && events.empty(), "schedule awaits actual add callbacks");
        check([center.categories containsObject:foreign_category], "foreign categories preserved");
        for (UNNotificationRequest* request in center.requests)
            check(request.content.sound == nil && request.content.title.length > 0,
                "actual notification value objects contain title/silent policy");
        while (center.addCallbacks.count)
            added();
        check(events.size() == 3 && event(events, 1, "show") && event(events, 2, "show")
                && event(events, 3, "show"),
            "native add acceptance emits show, not simulated visible presentation");
        auto* request = center.requests.firstObject;
        auto* notification = [TestNotification new];
        notification.request = request;
        __block UNNotificationPresentationOptions presentation
            = UNNotificationPresentationOptionNone;
        [center.delegate userNotificationCenter:reinterpret_cast<UNUserNotificationCenter*>(center)
                        willPresentNotification:reinterpret_cast<UNNotification*>(notification)
                          withCompletionHandler:^(
                              UNNotificationPresentationOptions value) { presentation = value; }];
        check((presentation & UNNotificationPresentationOptionBanner)
                && !(presentation & UNNotificationPresentationOptionSound),
            "silent foreground presentation options derive from matching native request");
        response(request, @"action.1");
        check(events.back().type == "action" && events.back().index == 1,
            "raw action identifier mapped to valid action index");
        auto count = events.size();
        response(request, UNNotificationDefaultActionIdentifier);
        check(events.size() == count, "old terminal response is ignored");
        response(center.requests[1], UNNotificationDefaultActionIdentifier);
        check(events.back().type == "click", "raw default action mapped to click");
        response(center.requests[2], UNNotificationDismissActionIdentifier);
        check(events.back().type == "close", "raw dismissal mapped to close");
        provider->stop();
        check(center.delegate == nil && center.categories.count == 1 &&
                [center.categories containsObject:foreign_category],
            "stop releases own delegate/categories and preserves foreign category");
    }

    puts("PASS macOS notification provider controlled authorization coalescing, real value objects "
         "and delegate responses");
    reset();
    events.clear();
    {
        auto provider = create();
        provider->show(1, options());
        provider->close(1);
        settings(UNAuthorizationStatusNotDetermined);
        check(center.authorizationCalls == 0 && center.requests.count == 0 && events.empty(),
            "canceled settings generation never requests authorization");
        provider->show(2, options());
        settings(UNAuthorizationStatusDenied);
        check(event(events, 2, "failed") && center.requests.count == 0,
            "denied authorization produces failure without add");
        provider->stop();
    }

    reset();
    events.clear();
    {
        auto provider = create();
        provider->show(1, options());
        settings(UNAuthorizationStatusAuthorized);
        auto* old = center.requests.lastObject;
        auto* old_category = old.content.categoryIdentifier;
        provider->show(1, options());
        settings(UNAuthorizationStatusAuthorized);
        auto* current = center.requests.lastObject;
        check(![old.identifier isEqual:current.identifier],
            "replacement uses unique request generation");
        check(![old_category isEqual:current.content.categoryIdentifier],
            "replacement isolates notification action category generation");
        added();
        check(events.empty() && [center.removedDelivered containsObject:old.identifier],
            "late old add completion removes request without stale show");
        provider->close(1);
        added();
        check(events.empty() && [center.removedPending containsObject:current.identifier] &&
                [center.removedDelivered containsObject:current.identifier],
            "late add after close removes both native pending and delivered requests");
        provider->stop();
        auto replacement = create();
        replacement->show(1, options());
        settings(UNAuthorizationStatusProvisional);
        check(![center.requests.lastObject.content.categoryIdentifier isEqual:old_category],
            "new provider session cannot collide with prior category generation");
        auto* failure = [NSError errorWithDomain:@"controlled" code:1 userInfo:nil];
        added(failure);
        check(event(events, 1, "failed"), "native add error is not successful delivery");
        replacement->stop();
    }

    puts("PASS macOS notification provider canceled/denied generations, late-add cleanup and "
         "category isolation");
    reset();
    events.clear();
    {
        // Delay actual category callbacks across two provider generations. The
        // old stopped apply must not overwrite the new generation's registration.
        center.autoCategories = NO;
        auto old_provider = create();
        old_provider->show(1, options());
        settings(UNAuthorizationStatusAuthorized);
        check(center.categoryCallbacks.count == 1 && center.requests.count == 0,
            "submission waits for real category snapshot callback");
        old_provider->stop();
        auto new_provider = create();
        new_provider->show(2, options());
        settings(UNAuthorizationStatusAuthorized);
        check(center.categoryCallbacks.count == 1,
            "category read/merge/write transactions serialize across generations");
        for (size_t index = 0; index < 3; ++index) {
            check(center.categoryCallbacks.count == 1,
                "queued category transaction becomes runnable");
            if (!center.categoryCallbacks.count)
                break;
            CategoriesCallback callback = center.categoryCallbacks.firstObject;
            [center.categoryCallbacks removeObjectAtIndex:0];
            callback(center.categories);
        }

        check(center.requests.count == 1 && [center.categories containsObject:foreign_category],
            "stale transaction cannot remove foreign or new-generation category");
        added();
        check(events.size() == 1 && event(events, 2, "show"),
            "only live generation schedules and acknowledges delivery");
        new_provider->stop();
        drain();
        check(center.categories.count == 1 && [center.categories containsObject:foreign_category],
            "queued cleanup removes only owned generation categories");
    }

    reset();
    events.clear();
    {
        auto* foreign = [NSObject new];
        center.delegate = reinterpret_cast<id<UNUserNotificationCenterDelegate>>(foreign);
        check(!Provider::supported(), "foreign delegate prevents supported delivery");
        auto provider = create();
        provider->show(1, options());
        check(event(events, 1, "failed") && center.settingsCallbacks.count == 0,
            "foreign delegate rejected before permission query");
        provider->stop();
        check(center.delegate == foreign, "stop does not steal unrelated delegate");
        center.delegate = nil;
    }

    reset();
    events.clear();
    {
        auto provider = create();
        for (uint64_t token = 1; token <= 64; ++token)
            provider->show(token, options());
        provider->show(65, options());
        check(event(events, 65, "failed") && center.settingsCallbacks.count == 1,
            "active record quota64 is bounded independently of one coalesced query");
        provider->stop();
        auto count = events.size();
        settings(UNAuthorizationStatusNotDetermined);
        check(events.size() == count && center.authorizationCalls == 0,
            "stopped settings completion cannot prompt or emit");
    }

    reset();
    events.clear();
    {
        // Each stopped generation retains a genuine outstanding native callback.
        // Close/stop alone must not release its global callback ticket prematurely.
        std::vector<std::shared_ptr<Provider>> pending;
        for (uint64_t index = 0; index < 64; ++index) {
            auto provider = create();
            provider->show(1, options());
            provider->stop();
            pending.push_back(std::move(provider));
        }

        check(center.settingsCallbacks.count == 64,
            "64 outstanding native queries consume callback quota");
        auto refused = create();
        refused->show(100, options());
        check(event(events, 100, "failed") && center.settingsCallbacks.count == 64,
            "stop cannot prematurely reclaim an in-flight OS callback ticket");
        refused->stop();
        settings(UNAuthorizationStatusDenied);
        auto admitted = create();
        admitted->show(101, options());
        check(!event(events, 101, "failed") && center.settingsCallbacks.count == 64,
            "actual callback completion releases exactly one budget ticket");
        admitted->stop();
        drain();
        for (auto const& provider : pending)
            provider->stop();
    }

    puts("PASS macOS notification provider foreign-delegate ownership, quota and real-callback "
         "ticket lifetime");
    reset();
}
}

int main()
{
    @autoreleasepool {
        bundle = [TestNotificationBundle new];
        center = [TestNotificationCenter new];
        // Interception precedes every production-provider call; original IMPs are
        // never invoked. Unsupported selectors fail loudly, with no OS fallback.
        Interception interception;
        @try {
            run();
            drain();
        } @catch (NSException* exception) {
            fprintf(stderr, "Controlled peer missing selector/state: %s\n",
                exception.description.UTF8String);
            return 1;
        }

        if (!passed)
            return 1;
        puts("Controlled Objective-C center only: no OS center, authorization prompt, alert or "
             "platform acceptance");
        return 0;
    }
}
