/** Genuine UserNotifications scheduling. Authorization is requested only by show(). */
#include "Provider.h"
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>
#include <atomic>
#include <deque>
#include <limits>
#include <unordered_map>
#include <utility>

namespace Firefly::Notifications {
struct State;
}

@interface ElytraNotificationDelegate : NSObject <UNUserNotificationCenterDelegate> {
    std::weak_ptr<Firefly::Notifications::State> _state;
}

- (instancetype)initWithState:(std::weak_ptr<Firefly::Notifications::State>)state;
@end

namespace Firefly::Notifications {
namespace {
    constexpr size_t limit = 64;
    std::atomic<unsigned> inflight { 0 };
    NSString* const category_base = @"org.elytra.firefly.notifications.v1.";

    struct Ticket {
        std::atomic<bool> finished { false };

        void release()
        {
            if (!finished.exchange(true))
                inflight.fetch_sub(1);
        }
    };

    std::shared_ptr<Ticket> admit()
    {
        auto count = inflight.load();
        while (count < limit)
            if (inflight.compare_exchange_weak(count, count + 1))
                return std::make_shared<Ticket>();
        return {};
    }

    void on_main(std::function<void()> function)
    {
        if (NSThread.isMainThread)
            function();
        else
            dispatch_async(dispatch_get_main_queue(), ^{ function(); });
    }

    NSString* text(std::string const& value)
    {
        return [[NSString alloc] initWithBytes:value.data()
                                        length:value.size()
                                      encoding:NSUTF8StringEncoding];
    }

    bool identity()
    {
        auto* bundle = NSBundle.mainBundle;
        auto* identifier = bundle.bundleIdentifier;
        return identifier.length && identifier.length <= 255 &&
            [bundle.bundleURL.pathExtension.lowercaseString isEqualToString:@"app"] &&
            [[bundle objectForInfoDictionaryKey:@"CFBundlePackageType"] isEqual:@"APPL"] &&
            [bundle.executableURL.path
                hasPrefix:[bundle.bundlePath stringByAppendingString:@"/Contents/MacOS/"]];
    }

    void remove(UNUserNotificationCenter* center, NSString* request)
    {
        [center removePendingNotificationRequestsWithIdentifiers:@[ request ]];
        [center removeDeliveredNotificationsWithIdentifiers:@[ request ]];
    }

    // Serialize our read/merge/write transactions across provider generations.
    // Foreign categories in the actual OS snapshot are always retained.
    using CategoriesCallback = std::function<void(NSSet<UNNotificationCategory*>*)>;

    struct CategoryTask {
        UNUserNotificationCenter* __strong center;
        CategoriesCallback apply;
    };

    std::deque<std::shared_ptr<CategoryTask>> category_tasks;
    bool category_busy {};

    void next_categories()
    {
        if (category_busy || category_tasks.empty())
            return;
        category_busy = true;
        auto task = category_tasks.front();
        category_tasks.pop_front();
        auto ticket = admit();
        if (!ticket) {
            task->apply(nil);
            category_busy = false;
            next_categories();
            return;
        }

        [task->center getNotificationCategoriesWithCompletionHandler:^(
            NSSet<UNNotificationCategory*>* existing) {
            ticket->release();
            on_main([task, existing] {
                task->apply(existing);
                category_busy = false;
                next_categories();
            });
        }];
    }

    bool categories(UNUserNotificationCenter* center, CategoriesCallback apply)
    {
        if (category_tasks.size() + (category_busy ? 1 : 0) >= limit)
            return false;
        category_tasks.push_back(std::make_shared<CategoryTask>(center, std::move(apply)));
        next_categories();
        return true;
    }

    void cleanup_categories(UNUserNotificationCenter* center, NSString* prefix)
    {
        categories(center, [center, prefix](NSSet<UNNotificationCategory*>* existing) {
            if (!existing)
                return;
            auto* retained = [NSMutableSet set];
            for (UNNotificationCategory* category in existing)
                if (![category.identifier hasPrefix:prefix])
                    [retained addObject:category];
            [center setNotificationCategories:retained];
        });
    }

}

struct State : std::enable_shared_from_this<State> {
    struct Record {
        Options options;
        uint64_t generation;
        NSString* __strong request;
        bool submitted { false };
    };

    explicit State(EventCallback value)
        : callback(std::move(value))
    {
    }

    EventCallback callback;
    UNUserNotificationCenter* __strong center {};
    ElytraNotificationDelegate* __strong delegate {};
    NSString* __strong session { NSUUID.UUID.UUIDString };
    NSString* __strong category_prefix { [NSString
        stringWithFormat:@"%@%@.generation.", category_base, session] };
    std::unordered_map<uint64_t, Record> records;
    uint64_t generation {};
    bool stopped {}, authorizing {}, categories_pending {}, permission_ready {};

    bool owns_delegate() const
    {
        return center && center.delegate == delegate;
    }

    void emit(uint64_t token, char const* type, char const* message = "", int index = -1)
    {
        if (stopped || !callback)
            return;
        auto notify = callback;
        notify({ token, type, message, index });
    }

    void failed(uint64_t token, uint64_t expected, char const* reason)
    {
        auto found = records.find(token);
        if (found == records.end() || found->second.generation != expected)
            return;
        remove(center, found->second.request);
        records.erase(found);
        emit(token, "failed", reason);
    }

    void fail_waiting(char const* reason)
    {
        std::vector<std::pair<uint64_t, uint64_t>> waiting;
        for (auto const& [token, record] : records)
            if (!record.submitted)
                waiting.emplace_back(token, record.generation);
        for (auto [token, version] : waiting)
            failed(token, version, reason);
    }

    bool has_waiting() const
    {
        for (auto const& [token, record] : records)
            if (!record.submitted)
                return true;
        return false;
    }

    void authorize()
    {
        if (stopped || authorizing || !has_waiting())
            return;
        auto ticket = admit();
        if (!ticket) {
            fail_waiting("Native notification request budget exhausted");
            return;
        }

        authorizing = true;
        permission_ready = false;
        auto weak = weak_from_this();
        [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
            ticket->release();
            auto status = settings.authorizationStatus;
            on_main([weak, status] {
                auto state = weak.lock();
                if (!state || state->stopped)
                    return;
                if (!state->owns_delegate()) {
                    state->authorizing = false;
                    state->fail_waiting("Another native notification delegate owns delivery");
                    return;
                }

                if (status == UNAuthorizationStatusAuthorized
                    || status == UNAuthorizationStatusProvisional) {
                    state->authorizing = false;
                    state->permission_ready = true;
                    state->sync_categories();
                } else if (status == UNAuthorizationStatusNotDetermined) {
                    state->request_authorization();
                } else {
                    state->authorizing = false;
                    state->fail_waiting("Notification permission was not granted");
                }
            });
        }];
    }

    void request_authorization()
    {
        // A caller may close the last request while the settings query is in
        // flight. Do not open an authorization dialog for canceled generations.
        if (stopped || !has_waiting()) {
            authorizing = false;
            return;
        }

        auto ticket = admit();
        if (!ticket) {
            authorizing = false;
            fail_waiting("Native notification request budget exhausted");
            return;
        }

        auto weak = weak_from_this();
        [center
            requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                          completionHandler:^(BOOL granted, NSError* error) {
                              ticket->release();
                              bool allowed = granted && !error;
                              on_main([weak, allowed] {
                                  auto state = weak.lock();
                                  if (!state || state->stopped)
                                      return;
                                  state->authorizing = false;
                                  if (allowed && state->owns_delegate()) {
                                      state->permission_ready = true;
                                      state->sync_categories();
                                  } else
                                      state->fail_waiting(
                                          "Notification permission was not granted");
                              });
                          }];
    }

    UNNotificationCategory* category(Record const& record)
    {
        auto* actions = [NSMutableArray array];
        for (size_t index = 0; index < record.options.actions.size(); ++index)
            [actions
                addObject:[UNNotificationAction
                              actionWithIdentifier:[NSString stringWithFormat:@"action.%zu", index]
                                             title:text(record.options.actions[index])
                                           options:0]];
        return [UNNotificationCategory
            categoryWithIdentifier:[NSString stringWithFormat:@"%@%llu", category_prefix,
                                       static_cast<unsigned long long>(record.generation)]
                           actions:actions
                 intentIdentifiers:@[]
                           options:UNNotificationCategoryOptionCustomDismissAction];
    }

    void sync_categories()
    {
        if (stopped || categories_pending)
            return;
        categories_pending = true;
        auto weak = weak_from_this();
        bool queued = categories(center, [weak](NSSet<UNNotificationCategory*>* existing) {
            auto state = weak.lock();
            if (!state || state->stopped)
                return;
            state->categories_pending = false;
            if (!existing || !state->owns_delegate()) {
                state->fail_waiting("Native notification category registration unavailable");
                return;
            }

            auto* merged = [NSMutableSet set];
            for (UNNotificationCategory* category in existing)
                if (![category.identifier hasPrefix:state->category_prefix])
                    [merged addObject:category];
            for (auto const& [token, record] : state->records)
                [merged addObject:state->category(record)];
            [state->center setNotificationCategories:merged];
            if (!state->permission_ready)
                return;
            std::vector<std::pair<uint64_t, uint64_t>> waiting;
            for (auto const& [token, record] : state->records)
                if (!record.submitted)
                    waiting.emplace_back(token, record.generation);
            for (auto [token, version] : waiting)
                state->submit(token, version);
        });
        if (!queued) {
            categories_pending = false;
            fail_waiting("Native notification category budget exhausted");
        }
    }

    void submit(uint64_t token, uint64_t version)
    {
        auto found = records.find(token);
        if (stopped || found == records.end() || found->second.generation != version
            || found->second.submitted)
            return;
        auto ticket = admit();
        if (!ticket) {
            failed(token, version, "Native notification request budget exhausted");
            return;
        }

        auto& record = found->second;
        record.submitted = true;
        auto* content = [[UNMutableNotificationContent alloc] init];
        content.title = text(record.options.title);
        content.body = text(record.options.body);
        content.sound = record.options.silent ? nil : UNNotificationSound.defaultSound;
        content.categoryIdentifier = [NSString stringWithFormat:@"%@%llu", category_prefix,
            static_cast<unsigned long long>(record.generation)];
        auto* identifier = record.request;
        auto* request = [UNNotificationRequest requestWithIdentifier:identifier
                                                             content:content
                                                             trigger:nil];
        auto* notification_center = center;
        auto weak = weak_from_this();
        [center
            addNotificationRequest:request
             withCompletionHandler:^(NSError* error) {
                 ticket->release();
                 bool accepted = !error;
                 on_main([weak, token, version, identifier, notification_center, accepted] {
                     auto state = weak.lock();
                     if (!state || state->stopped) {
                         remove(notification_center, identifier);
                         return;
                     }

                     auto current = state->records.find(token);
                     if (current == state->records.end() || current->second.generation != version) {
                         remove(notification_center, identifier);
                         return;
                     }

                     if (!accepted || !state->owns_delegate()) {
                         state->failed(token, version, "Native notification submission failed");
                         return;
                     }

                     state->emit(token, "show");
                 });
             }];
    }

    void show(uint64_t token, Options options)
    {
        if (stopped)
            return;
        if (!token || token > 2147483647 || options.actions.size() > 4
            || options.title.size() > 4096 || options.body.size() > 65536 || !text(options.title)
            || !text(options.body)) {
            emit(token, "failed", "Invalid native notification options");
            return;
        }

        for (auto const& action : options.actions)
            if (action.size() > 1024 || !text(action)) {
                emit(token, "failed", "Invalid native notification action");
                return;
            }

        if (!Provider::supported()) {
            emit(token, "failed",
                "Native notifications require an application bundle and available delegate");
            return;
        }

        if (!center)
            center = UNUserNotificationCenter.currentNotificationCenter;
        if (center.delegate && center.delegate != delegate) {
            emit(token, "failed", "Another native notification delegate owns delivery");
            return;
        }

        if (!delegate) {
            delegate = [[ElytraNotificationDelegate alloc] initWithState:weak_from_this()];
            center.delegate = delegate;
        }

        if (auto previous = records.find(token); previous != records.end()) {
            remove(center, previous->second.request);
            records.erase(previous);
        }

        if (records.size() >= limit || generation == std::numeric_limits<uint64_t>::max()) {
            emit(token, "failed", "Native notification budget exhausted");
            return;
        }

        auto version = ++generation;
        auto* request = [NSString stringWithFormat:@"%@.%llu.%llu", session,
            static_cast<unsigned long long>(token), static_cast<unsigned long long>(version)];
        records.emplace(token, Record { std::move(options), version, request, false });
        authorize();
    }

    void close(uint64_t token)
    {
        auto found = records.find(token);
        if (stopped || found == records.end())
            return;
        remove(center, found->second.request);
        records.erase(found);
        sync_categories();
    }

    UNNotificationPresentationOptions presentation(NSString* identifier)
    {
        if (stopped || !owns_delegate())
            return UNNotificationPresentationOptionNone;
        for (auto const& [token, record] : records)
            if ([record.request isEqualToString:identifier])
                return UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList
                    | (record.options.silent ? 0 : UNNotificationPresentationOptionSound);
        return UNNotificationPresentationOptionNone;
    }

    void response(NSString* identifier, NSString* action)
    {
        if (stopped || !owns_delegate())
            return;
        for (auto const& [token, record] : records) {
            if (![record.request isEqualToString:identifier])
                continue;
            int index = -1;
            if ([action hasPrefix:@"action."])
                for (size_t candidate = 0; candidate < record.options.actions.size(); ++candidate)
                    if ([action
                            isEqualToString:[NSString stringWithFormat:@"action.%zu", candidate]])
                        index = static_cast<int>(candidate);
            auto id = token;
            bool dismissed = [action isEqualToString:UNNotificationDismissActionIdentifier];
            bool clicked = [action isEqualToString:UNNotificationDefaultActionIdentifier];
            if (!dismissed && !clicked && index < 0)
                return;
            remove(center, record.request);
            records.erase(token);
            sync_categories();
            emit(id, dismissed ? "close" : clicked ? "click" : "action", "", index);
            return;
        }
    }

    void stop()
    {
        if (stopped)
            return;
        stopped = true;
        callback = {};
        for (auto const& [token, record] : records)
            remove(center, record.request);
        records.clear();
        if (owns_delegate())
            center.delegate = nil;
        if (center && delegate)
            cleanup_categories(center, category_prefix);
        delegate = nil;
    }
};

namespace {
    class MacProvider final : public Provider {
    public:
        explicit MacProvider(EventCallback callback)
            : m_state(std::make_shared<State>(std::move(callback)))
        {
        }

        ~MacProvider() override
        {
            stop();
        }

        void show(uint64_t token, Options options) override
        {
            on_main([state = m_state, token, options = std::move(options)]() mutable {
                state->show(token, std::move(options));
            });
        }

        void close(uint64_t token) override
        {
            on_main([state = m_state, token] { state->close(token); });
        }

        void stop() override
        {
            on_main([state = m_state] { state->stop(); });
        }

    private:
        std::shared_ptr<State> m_state;
    };
}

bool Provider::supported()
{
    if (@available(macOS 11.0, *)) {
        if (!identity())
            return false;
        auto* delegate = UNUserNotificationCenter.currentNotificationCenter.delegate;
        return !delegate || [delegate isKindOfClass:ElytraNotificationDelegate.class];
    }

    return false;
}

std::shared_ptr<Provider> Provider::create(EventCallback callback)
{
    return std::make_shared<MacProvider>(std::move(callback));
}
}

@implementation ElytraNotificationDelegate

- (instancetype)initWithState:(std::weak_ptr<Firefly::Notifications::State>)state
{
    self = [super init];
    if (self)
        _state = std::move(state);
    return self;
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    (void)center;
    auto weak = _state;
    NSString* identifier = notification.request.identifier;
    Firefly::Notifications::on_main([weak, identifier, completionHandler] {
        auto state = weak.lock();
        completionHandler(
            state ? state->presentation(identifier) : UNNotificationPresentationOptionNone);
    });
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler
{
    (void)center;
    auto weak = _state;
    NSString* identifier = response.notification.request.identifier;
    NSString* action = response.actionIdentifier;
    Firefly::Notifications::on_main([weak, identifier, action, completionHandler] {
        if (auto state = weak.lock())
            state->response(identifier, action);
        completionHandler();
    });
}

@end
