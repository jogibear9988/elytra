/** Actual native queries and passive ownership; no writes, posts or permission requests. */
#include "native/system-preferences/Provider.h"
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <cstdio>

namespace {
bool matches(Firefly::SystemPreferences::ValueResult const& result, id expected)
{
    if (!result.code.empty()) {
        fprintf(stderr, "%s: %s\n", result.code.c_str(), result.message.c_str());
        return false;
    }

    auto data = [NSData dataWithBytes:result.json.data() length:result.json.size()];
    id decoded = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [decoded isKindOfClass:NSDictionary.class] && [decoded count] == 1 &&
        [decoded isEqual:@ { @"value" : expected }];
}

NSString* direct_status(AVMediaType type)
{
    switch ([AVCaptureDevice authorizationStatusForMediaType:type]) {
    case AVAuthorizationStatusNotDetermined:
        return @"not-determined";
    case AVAuthorizationStatusRestricted:
        return @"restricted";
    case AVAuthorizationStatusDenied:
        return @"denied";
    case AVAuthorizationStatusAuthorized:
        return @"granted";
    default:
        return @"unknown";
    }
}

int run()
{
    auto provider = Firefly::SystemPreferences::Provider::create();
    if (!matches(provider->media_access_status("camera"), direct_status(AVMediaTypeVideo))
        || !matches(provider->media_access_status("microphone"), direct_status(AVMediaTypeAudio))
        || !matches(provider->media_access_status("screen"),
            CGPreflightScreenCaptureAccess() ? @"granted" : @"unknown"))
        return 1;
    auto defaults = NSUserDefaults.standardUserDefaults;
    if (!matches(provider->get_user_default("AppleActionOnDoubleClick", "string"),
            [defaults stringForKey:@"AppleActionOnDoubleClick"] ?: @"")
        || !matches(provider->get_user_default("NSUseImprovedLayoutPass", "boolean"),
            @([defaults boolForKey:@"NSUseImprovedLayoutPass"])))
        return 1;
    int id = 0;
    for (auto const* kind : { "workspace", "distributed", "local" }) {
        auto observe = provider->subscribe(id, kind, "ElytraReadOnlyPassiveProbe", [](auto) { });
        if (!observe.code.empty() || !provider->unsubscribe(id, kind).code.empty())
            return 1;
        ++id;
    }

    provider->stop();
    if (provider->media_access_status("camera").code != "ERR_FIREFLY_APP_EXITED")
        return 1;
    puts("PASS actual native permission/default values independently match direct "
         "AVFoundation/CoreGraphics/NSUserDefaults queries and passive three-center "
         "install/remove; no writes, posts, requests or transitions");
    return 0;
}
}

int main()
{
    @autoreleasepool {
        return run();
    }
}
