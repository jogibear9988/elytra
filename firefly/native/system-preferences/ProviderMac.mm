/** Owned native defaults/notification bridge. No requests for media permission. */
#include "Provider.h"
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace Firefly::SystemPreferences {
namespace {
    constexpr size_t json_limit = 65536;

    NSString* text(std::string const& input, size_t limit = 4096)
    {
        if (input.size() > limit || input.find('\0') != std::string::npos)
            return nil;
        return [[NSString alloc] initWithBytes:input.data()
                                        length:input.size()
                                      encoding:NSUTF8StringEncoding];
    }

    std::string utf8(NSString* value)
    {
        auto data = [value dataUsingEncoding:NSUTF8StringEncoding];
        return data ? std::string(static_cast<char const*>(data.bytes), data.length)
                    : std::string {};
    }

    Result error(char const* message)
    {
        return { "ERR_FIREFLY_SYSTEM_PREFERENCES_NATIVE", message };
    }

    ValueResult value_error(char const* code, char const* message)
    {
        return { code, message, {} };
    }

    ValueResult encode(id value)
    {
        auto data = [NSJSONSerialization dataWithJSONObject:@ { @"value" : value }
                                                    options:0
                                                      error:nil];
        if (!data || data.length > json_limit)
            return value_error("ERR_FIREFLY_SYSTEM_PREFERENCES_DATA",
                "Native preference data exceeds supported bounds");
        return { {}, {}, std::string(static_cast<char const*>(data.bytes), data.length) };
    }

    // Match Electron's native description fallback while bounding recursion, node
    // count and serialized bytes. Cycles cannot recurse indefinitely.
    id snapshot(id value, unsigned depth, size_t& budget, bool& valid)
    {
        if (!valid || !budget || depth > 8) {
            valid = false;
            return nil;
        }

        --budget;
        if ([value isKindOfClass:NSString.class]) {
            if ([value lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > json_limit)
                valid = false;
            return value;
        }

        if ([value isKindOfClass:NSNumber.class]) {
            if (!std::isfinite([value doubleValue]))
                valid = false;
            return value;
        }

        if ([value isKindOfClass:NSArray.class]) {
            if ([value count] > budget) {
                valid = false;
                return nil;
            }

            auto output = [NSMutableArray array];
            for (id item in value) {
                id copied = snapshot(item, depth + 1, budget, valid);
                if (!valid)
                    return nil;
                [output addObject:copied];
            }

            return output;
        }

        if ([value isKindOfClass:NSDictionary.class]) {
            if ([value count] > budget) {
                valid = false;
                return nil;
            }

            auto output = [NSMutableDictionary dictionary];
            for (id key in value) {
                NSString* name = [key isKindOfClass:NSString.class] ? key : [key description];
                if (!name || [name lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > 4096) {
                    valid = false;
                    return nil;
                }

                id copied = snapshot([value objectForKey:key], depth + 1, budget, valid);
                if (!valid)
                    return nil;
                output[name] = copied;
            }

            return output;
        }

        NSString* description = [value description];
        if (!description)
            description = @"";
        if ([description lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > json_limit)
            valid = false;
        return description;
    }

    std::atomic<unsigned> observer_count { 0 };

    struct ObserverLease {
        std::atomic<bool> released { false };

        void release()
        {
            if (!released.exchange(true))
                observer_count.fetch_sub(1);
        }
    };

    std::shared_ptr<ObserverLease> admit_observer()
    {
        auto count = observer_count.load();
        while (count < 64) {
            if (observer_count.compare_exchange_weak(count, count + 1))
                return std::make_shared<ObserverLease>();
        }

        return {};
    }

    struct Subscription {
        std::string kind;
        NSNotificationCenter* center {};
        id token {};
        bool active { true };
        EventCallback callback;
        std::shared_ptr<ObserverLease> lease;
    };

    struct State {
        std::mutex mutex;
        bool stopped {};
        int last { -1 };
        std::unordered_map<int, Subscription> subscriptions;
    };

    NSNotificationCenter* center_for(std::string const& kind)
    {
        if (kind == "distributed")
            return NSDistributedNotificationCenter.defaultCenter;
        if (kind == "local")
            return NSNotificationCenter.defaultCenter;
        if (kind == "workspace")
            return NSWorkspace.sharedWorkspace.notificationCenter;
        return nil;
    }

    void notify(std::weak_ptr<State> weak, int id, NSNotification* notification)
    {
        auto state = weak.lock();
        if (!state)
            return;
        EventCallback callback;
        {
            std::lock_guard lock(state->mutex);
            auto found = state->subscriptions.find(id);
            if (state->stopped || found == state->subscriptions.end() || !found->second.active)
                return;
            callback = found->second.callback;
        }

        Event event;
        event.id = id;
        @try {
            event.name = utf8(notification.name);
            event.object =
                [notification.object isKindOfClass:NSString.class] ? utf8(notification.object) : "";
            size_t budget = 1024;
            bool valid = event.name.size() <= 4096 && event.object.size() <= 4096
                && event.name.find('\0') == std::string::npos
                && event.object.find('\0') == std::string::npos;
            NSObject* info = snapshot(notification.userInfo ?: @ {}, 0, budget, valid);
            auto bytes
                = valid ? [NSJSONSerialization dataWithJSONObject:info options:0 error:nil] : nil;
            if (!bytes || bytes.length > json_limit) {
                event.code = "ERR_FIREFLY_SYSTEM_PREFERENCES_DATA";
                event.message = "Native notification exceeds snapshot bounds";
            } else
                event.user_info_json
                    = std::string(static_cast<char const*>(bytes.bytes), bytes.length);
        } @catch (NSException*) {
            event.code = "ERR_FIREFLY_SYSTEM_PREFERENCES_DATA";
            event.message = "Native notification snapshot failed";
        }

        callback(std::move(event));
    }

    class MacProvider final : public Provider {
    public:
        MacProvider()
            : state(std::make_shared<State>())
        {
        }

        ~MacProvider() override
        {
            stop();
        }

        ValueResult get_user_default(std::string const& name, std::string const& type) override
        {
            if (stopped())
                return value_error("ERR_FIREFLY_APP_EXITED", "System preferences stopped");
            auto key = text(name);
            if (!key)
                return value_error("ERR_INVALID_ARG_VALUE", "Invalid preference key");
            @try {
                auto defaults = NSUserDefaults.standardUserDefaults;
                if (!defaults)
                    return value_error("ERR_FIREFLY_SYSTEM_PREFERENCES_NATIVE",
                        "Native preference store unavailable");
                if (type == "string") {
                    auto value = [defaults stringForKey:key] ?: @"";
                    if (!text(utf8(value), 65536))
                        return value_error("ERR_FIREFLY_SYSTEM_PREFERENCES_DATA",
                            "Native preference string exceeds supported bounds");
                    return encode(value);
                }

                if (type == "boolean")
                    return encode(@([defaults boolForKey:key]));
                return value_error("ERR_FIREFLY_UNSUPPORTED",
                    "Only string and boolean user defaults are supported");
            } @catch (NSException*) {
                return value_error(
                    "ERR_FIREFLY_SYSTEM_PREFERENCES_NATIVE", "Native preference read failed");
            }
        }

        Result set_user_default(
            std::string const& name, std::string const& type, std::string const& json) override
        {
            if (stopped())
                return { "ERR_FIREFLY_APP_EXITED", "System preferences stopped" };
            auto key = text(name);
            auto input = text(json, json_limit);
            if (!key || !input)
                return { "ERR_INVALID_ARG_VALUE", "Invalid preference value" };
            @try {
                id value = [NSJSONSerialization
                    JSONObjectWithData:[input dataUsingEncoding:NSUTF8StringEncoding]
                               options:NSJSONReadingFragmentsAllowed
                                 error:nil];
                auto defaults = NSUserDefaults.standardUserDefaults;
                if (!defaults)
                    return error("Native preference store unavailable");
                if (type == "string" && [value isKindOfClass:NSString.class]) {
                    if (!text(utf8(value), 65536))
                        return { "ERR_INVALID_ARG_VALUE", "Invalid preference string" };
                    [defaults setObject:value forKey:key];
                } else if (type == "boolean" && [value isKindOfClass:NSNumber.class]
                    && CFGetTypeID((__bridge CFTypeRef)value) == CFBooleanGetTypeID())
                    [defaults setBool:[value boolValue] forKey:key];
                else if (type != "string" && type != "boolean")
                    return { "ERR_FIREFLY_UNSUPPORTED",
                        "Only string and boolean user defaults are supported" };
                else
                    return { "ERR_INVALID_ARG_TYPE", "Preference value does not match its type" };
                return {};
            } @catch (NSException*) {
                return error("Native preference write failed");
            }
        }

        ValueResult media_access_status(std::string const& type) override
        {
            if (stopped())
                return value_error("ERR_FIREFLY_APP_EXITED", "System preferences stopped");
            @try {
                if (type == "screen") {
                    // False cannot distinguish denied from not-yet-requested. Never
                    // request capture or invent a more specific authorization state.
                    return encode(CGPreflightScreenCaptureAccess() ? @"granted" : @"unknown");
                }

                if (type != "camera" && type != "microphone")
                    return value_error("ERR_INVALID_ARG_VALUE", "Invalid media type");
                auto status = [AVCaptureDevice authorizationStatusForMediaType:type == "camera"
                        ? AVMediaTypeVideo
                        : AVMediaTypeAudio];
                switch (status) {
                case AVAuthorizationStatusNotDetermined:
                    return encode(@"not-determined");
                case AVAuthorizationStatusRestricted:
                    return encode(@"restricted");
                case AVAuthorizationStatusDenied:
                    return encode(@"denied");
                case AVAuthorizationStatusAuthorized:
                    return encode(@"granted");
                default:
                    return encode(@"unknown");
                }
            } @catch (NSException*) {
                return value_error(
                    "ERR_FIREFLY_SYSTEM_PREFERENCES_NATIVE", "Native permission query failed");
            }
        }

        Result subscribe(int id, std::string const& kind, std::optional<std::string> const& name,
            EventCallback callback) override
        {
            if (stopped())
                return { "ERR_FIREFLY_APP_EXITED", "System preferences stopped" };
            auto value = name ? text(*name) : nil;
            if (id < 0 || (name && !value))
                return { "ERR_INVALID_ARG_VALUE", "Invalid subscription" };
            if (kind != "distributed" && kind != "workspace" && kind != "local")
                return { "ERR_INVALID_ARG_VALUE", "Invalid notification center" };
            NSNotificationCenter* center = nil;
            @try {
                center = center_for(kind);
            } @catch (NSException*) {
                return error("Native notification center unavailable");
            }

            if (!center)
                return error("Native notification center unavailable");
            std::shared_ptr<ObserverLease> lease;
            {
                std::lock_guard lock(state->mutex);
                if (state->stopped)
                    return { "ERR_FIREFLY_APP_EXITED", "System preferences stopped" };
                if (id <= state->last || state->subscriptions.size() >= 64)
                    return { "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
                        "Subscription identity or quota exhausted" };

                lease = admit_observer();
                if (!lease)
                    return { "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
                        "Native observer quota exhausted" };

                state->last = id;
                state->subscriptions.emplace(
                    id, Subscription { kind, center, nil, true, std::move(callback), lease });
            }

            NSObject* token = nil;
            @try {
                auto weak = std::weak_ptr(state);
                token = [center
                    addObserverForName:value
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* event) { notify(weak, id, event); }];
            } @catch (NSException*) {
            }

            bool retained = false;
            {
                std::lock_guard lock(state->mutex);
                auto found = state->subscriptions.find(id);
                if (found != state->subscriptions.end()) {
                    if (token && !state->stopped && found->second.active) {
                        found->second.token = token;
                        retained = true;
                    } else
                        state->subscriptions.erase(found);
                }
            }

            if (!retained && token) {
                @try {
                    [center removeObserver:token];
                    lease->release();
                } @catch (NSException*) {
                    std::lock_guard lock(state->mutex);
                    state->subscriptions.emplace(
                        id, Subscription { kind, center, token, false, {}, lease });
                }
            }

            if (!token)
                lease->release();
            return retained
                ? Result {}
                : error("Native subscription failed or was revoked during registration");
        }

        Result unsubscribe(int id, std::string const& kind) override
        {
            if (stopped())
                return { "ERR_FIREFLY_APP_EXITED", "System preferences stopped" };
            Subscription removed {};
            {
                std::lock_guard lock(state->mutex);
                auto found = state->subscriptions.find(id);
                if (found == state->subscriptions.end() || found->second.kind != kind)
                    return {};
                found->second.active = false;
                found->second.callback = {};
                removed = found->second;
            }

            @try {
                if (removed.token)
                    [removed.center removeObserver:removed.token];
                if (removed.token)
                    removed.lease->release();
                std::lock_guard lock(state->mutex);
                state->subscriptions.erase(id);
                return {};
            } @catch (NSException*) {
                return error("Native unsubscribe failed");
            }
        }

        void stop() override
        {
            std::unordered_map<int, Subscription> removed;
            {
                std::lock_guard lock(state->mutex);
                state->stopped = true;
                for (auto& [id, subscription] : state->subscriptions) {
                    (void)id;
                    subscription.active = false;
                    subscription.callback = {};
                }

                removed.swap(state->subscriptions);
            }

            for (auto const& [id, subscription] : removed) {
                (void)id;
                @try {
                    if (subscription.token) {
                        [subscription.center removeObserver:subscription.token];
                        subscription.lease->release();
                    }
                } @catch (NSException*) {
                    // Retain failed cleanup for destructor retry; the process-global
                    // lease remains charged even if a center permanently refuses.
                    std::lock_guard lock(state->mutex);
                    state->subscriptions.emplace(id, subscription);
                }
            }
        }

    private:
        bool stopped()
        {
            std::lock_guard lock(state->mutex);
            return state->stopped;
        }

        std::shared_ptr<State> state;
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<MacProvider>();
}
}
