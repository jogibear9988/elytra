#include "Provider.h"
#import <AppKit/NSDocumentController.h>
#import <Foundation/Foundation.h>

namespace Firefly::RecentDocuments {
bool available()
{
    return [NSThread isMainThread];
}

Result clear()
{
    if (!available())
        return { "ERR_FIREFLY_RECENT_DOCUMENTS_THREAD",
            "Recent documents require the application main thread" };

    @autoreleasepool {
        @try {
            NSDocumentController* controller = [NSDocumentController sharedDocumentController];
            if (!controller)
                return { "ERR_FIREFLY_RECENT_DOCUMENTS_FAILED",
                    "Native recent document controller is unavailable" };

            [controller clearRecentDocuments:nil];
            return {};
        } @catch (NSException*) {
            return { "ERR_FIREFLY_RECENT_DOCUMENTS_FAILED",
                "Native recent document clearing failed" };
        }
    }
}

Result add(std::string const& absolute_path)
{
    if (!available())
        return { "ERR_FIREFLY_RECENT_DOCUMENTS_THREAD",
            "Recent documents require the application main thread" };

    @autoreleasepool {
        NSString* path = [[NSString alloc] initWithBytes:absolute_path.data()
                                                  length:absolute_path.size()
                                                encoding:NSUTF8StringEncoding];
        if (!path || ![path isAbsolutePath] || path.length == 0)
            return { "ERR_INVALID_ARG_VALUE", "Recent document path must be valid absolute UTF-8" };
        @try {
            NSURL* url = [NSURL fileURLWithPath:path];
            NSDocumentController* controller = [NSDocumentController sharedDocumentController];
            if (!url || !controller)
                return { "ERR_FIREFLY_RECENT_DOCUMENTS_FAILED",
                    "Native recent document URL/controller is unavailable" };

            [controller noteNewRecentDocumentURL:url];
            return {};
        } @catch (NSException*) {
            return { "ERR_FIREFLY_RECENT_DOCUMENTS_FAILED",
                "Native recent document registration failed" };
        }
    }
}
}
