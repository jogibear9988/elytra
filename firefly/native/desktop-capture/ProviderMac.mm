/** ScreenCaptureKit metadata only: real native IDs, bounded snapshots and no image capture. */
#include "Provider.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

namespace Firefly::DesktopCapture {
namespace {
    constexpr unsigned max_inflight = 8;
    constexpr size_t max_sources = 1024;
    constexpr size_t max_name_bytes = 4096;
    constexpr size_t max_metadata_bytes = 1024 * 1024;
    // Process-wide admission outlives any bridge/request timeout. An admitted
    // request releases its slot only when the OS actually invokes completion.
    std::atomic<unsigned> inflight { 0 };

    Result failure(char const* code, char const* message)
    {
        return { {}, code, message };
    }

    bool admit()
    {
        auto current = inflight.load(std::memory_order_relaxed);
        while (current < max_inflight) {
            if (inflight.compare_exchange_weak(current, current + 1, std::memory_order_relaxed))
                return true;
        }

        return false;
    }

    struct Operation {
        explicit Operation(Completion value)
            : complete(std::move(value))
        {
        }

        Completion complete;
        std::atomic<bool> completed { false };

        void finish(Result result)
        {
            if (completed.exchange(true, std::memory_order_relaxed))
                return;
            inflight.fetch_sub(1, std::memory_order_relaxed);
            auto callback = std::move(complete);
            callback(std::move(result));
        }
    };

    bool name_bytes(NSString* value, std::string& result)
    {
        if (!value) {
            result.clear();
            return true;
        }

        if (value.length > max_name_bytes)
            return false;
        auto* bytes = [value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
        if (!bytes || bytes.length > max_name_bytes)
            return false;
        if (bytes.length)
            result.assign(static_cast<char const*>(bytes.bytes), bytes.length);
        else
            result.clear();
        return true;
    }

    Result snapshot(SCShareableContent* content, NSError* error) API_AVAILABLE(macos(12.3))
    {
        if (error) {
            if ([error.domain isEqualToString:SCStreamErrorDomain]
                && error.code == SCStreamErrorUserDeclined)
                return failure("ERR_FIREFLY_SCREEN_CAPTURE_PERMISSION",
                    "Screen recording permission was not granted");
            if ([error.domain isEqualToString:SCStreamErrorDomain]
                && error.code == SCStreamErrorMissingEntitlements)
                return failure("ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE",
                    "ScreenCaptureKit requires unavailable application entitlements");
            return failure("ERR_FIREFLY_DESKTOP_CAPTURE",
                "ScreenCaptureKit could not enumerate desktop sources");
        }

        if (!content)
            return failure(
                "ERR_FIREFLY_DESKTOP_CAPTURE", "ScreenCaptureKit returned no source snapshot");
        if (content.displays.count > max_sources || content.windows.count > max_sources
            || content.displays.count + content.windows.count > max_sources)
            return failure("ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT",
                "Desktop source count exceeds the native metadata budget");
        Result result;
        size_t metadata_bytes = 0;
        std::unordered_set<std::string> identities;
        auto append = [&](Source source) {
            metadata_bytes += source.id.size() + source.name.size() + source.display_id.size();
            if (metadata_bytes > max_metadata_bytes || !identities.insert(source.id).second)
                return false;
            result.sources.push_back(std::move(source));
            return true;
        };

        for (SCDisplay* display in content.displays) {
            // SCDisplay.displayID is CGDirectDisplayID, matching NSScreenNumber
            // used by Firefly's screen provider. No list index becomes an ID.
            if (!display.displayID)
                return failure("ERR_FIREFLY_DESKTOP_CAPTURE",
                    "ScreenCaptureKit returned an invalid display identity");
            auto identity = std::to_string(display.displayID);
            auto name = content.displays.count == 1
                ? std::string("Entire Screen")
                : "Screen " + std::to_string(result.sources.size() + 1);
            if (!append({ "screen:" + identity + ":0", std::move(name), identity }))
                return failure("ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT",
                    "Desktop source metadata exceeds its budget or repeats an identity");
        }

        for (SCWindow* window in content.windows) {
            if (!window.windowID)
                return failure("ERR_FIREFLY_DESKTOP_CAPTURE",
                    "ScreenCaptureKit returned an invalid window identity");
            std::string name;
            if (!name_bytes(window.title, name))
                return failure("ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT",
                    "Desktop window title exceeds its UTF-8 metadata budget");
            // The third ID component is a native-window registry token, not an
            // owning-process flag. Raw SCK window IDs have no such registration.
            auto identity = "window:" + std::to_string(window.windowID) + ":0";
            if (!append({ std::move(identity), std::move(name), {} }))
                return failure("ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT",
                    "Desktop source metadata exceeds its budget or repeats an identity");
        }

        return result;
    }
}

bool preflight()
{
    if (@available(macOS 12.3, *))
        return CGPreflightScreenCaptureAccess();
    return false;
}

void enumerate(Completion complete)
{
    if (!complete)
        return;
    if (@available(macOS 12.3, *)) {
        if (!admit()) {
            complete(failure("ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT",
                "Too many native desktop enumerations remain in flight"));
            return;
        }

        auto operation = std::make_shared<Operation>(std::move(complete));
        [SCShareableContent
            getShareableContentExcludingDesktopWindows:YES
                                   onScreenWindowsOnly:YES
                                     completionHandler:^(
                                         SCShareableContent* content, NSError* error) {
                                         @autoreleasepool {
                                             operation->finish(snapshot(content, error));
                                         }
                                     }];
        return;
    }

    complete(failure("ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE",
        "ScreenCaptureKit source enumeration requires macOS 12.3 or later"));
}
}
