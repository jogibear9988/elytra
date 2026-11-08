/** Opt-in actual trash acceptance: one owned temporary file, restored to its original location. */
#include "native/shell/Provider.h"
#import <Foundation/Foundation.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#import <objc/runtime.h>

namespace {
std::mutex mutex;
std::condition_variable completed;
bool done = false;
Firefly::Shell::Result result;
NSURL* owned_source;
NSURL* owned_trashed;
using TrashMethod = BOOL (*)(id, SEL, NSURL*, NSURL**, NSError**);
TrashMethod original_trash;

BOOL observe_trash(id manager, SEL selector, NSURL* url, NSURL** output, NSError** error)
{
    // The sole original OS operation in this probe; capture the exact destination
    // returned by NSFileManager so cleanup never enumerates or touches other trash.
    if (![url isEqual:owned_source])
        std::abort();
    NSURL* destination = nil;
    BOOL success = original_trash(manager, selector, url, &destination, error);
    if (success)
        owned_trashed = destination;
    if (output)
        *output = destination;
    return success;
}

class Fixture {
public:
    Fixture()
    {
        auto name = [@"elytra-shell-owned-" stringByAppendingString:NSUUID.UUID.UUIDString];
        directory =
            [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:name]
                       isDirectory:YES];
        owned_source = [directory URLByAppendingPathComponent:@"Grüße %#.txt"];
    }

    ~Fixture()
    {
        if (!created)
            return;
        auto manager = NSFileManager.defaultManager;
        if (owned_trashed && ![manager fileExistsAtPath:owned_source.path]) {
            NSError* error = nil;
            if (![manager moveItemAtURL:owned_trashed toURL:owned_source error:&error]) {
                fprintf(stderr, "Owned fixture restoration failed; retained at %s: %s\n",
                    owned_trashed.path.UTF8String, error.description.UTF8String);
                return;
            }
        }
        // This directory was created uniquely by this probe and contains no user input.
        [manager removeItemAtURL:directory error:nil];
    }

    NSURL* directory;
    bool created { false };
};
}

int main()
{
    @autoreleasepool {
        Fixture fixture;
        NSError* error = nil;
        auto manager = NSFileManager.defaultManager;
        if (![manager createDirectoryAtURL:fixture.directory
                withIntermediateDirectories:NO
                                 attributes:nil
                                      error:&error]) {
            fprintf(stderr, "Owned fixture creation failed: %s\n", error.description.UTF8String);
            return 1;
        }

        fixture.created = true;
        if (![[@"Elytra owned trash acceptance Grüße" dataUsingEncoding:NSUTF8StringEncoding]
                writeToURL:owned_source
                   options:NSDataWritingAtomic
                     error:&error]) {
            fprintf(stderr, "Owned fixture creation failed: %s\n", error.description.UTF8String);
            return 1;
        }

        auto method = class_getInstanceMethod(
            NSFileManager.class, @selector(trashItemAtURL:resultingItemURL:error:));
        original_trash = reinterpret_cast<TrashMethod>(method_getImplementation(method));
        method_setImplementation(method, reinterpret_cast<IMP>(observe_trash));
        auto provider = Firefly::Shell::Provider::create();
        provider->trash(owned_source.path.UTF8String, [](Firefly::Shell::Result value) {
            std::lock_guard lock(mutex);
            result = std::move(value);
            done = true;
            completed.notify_all();
        });
        {
            std::unique_lock lock(mutex);
            // Never unwind across an unfinished OS trash operation or remove its fixture.
            if (!completed.wait_for(lock, std::chrono::seconds(15), [] { return done; })) {
                fprintf(stderr, "Native trash timed out; fixture retained at %s\n",
                    owned_source.path.UTF8String);
                std::_Exit(1);
            }
        }

        provider->stop();
        method_setImplementation(method, reinterpret_cast<IMP>(original_trash));
        if (!result.code.empty() || !owned_trashed || [manager fileExistsAtPath:owned_source.path]
            || ![manager fileExistsAtPath:owned_trashed.path]) {
            fprintf(stderr, "Native trash failed: %s %s\n", result.code.c_str(),
                result.message.c_str());
            return 1;
        }

        if (![manager moveItemAtURL:owned_trashed toURL:owned_source error:&error]) {
            fprintf(stderr, "Restore failed: %s\n", error.description.UTF8String);
            return 1;
        }

        owned_trashed = nil;
        auto bytes = [NSData dataWithContentsOfURL:owned_source];
        if (![bytes isEqual:[@"Elytra owned trash acceptance Grüße"
                                dataUsingEncoding:NSUTF8StringEncoding]])
            return 1;
        puts("PASS actual macOS native trash of one owned temporary Unicode file and "
             "exact-destination restoration; no application launch");
        return 0;
    }
}
