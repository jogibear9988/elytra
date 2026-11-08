/** Exact NSWorkspace/NSFileManager provider with intercepted peers. Never launches or trashes OS
 * items. */
#include "native/shell/Provider.h"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#import <objc/runtime.h>
#include <vector>

using Firefly::Shell::Provider;
using Firefly::Shell::Result;
using LaunchCallback = void (^)(NSRunningApplication*, NSError*);

@interface ControlledWorkspace : NSObject
@property (nonatomic) BOOL hasHandler;
@property (nonatomic, strong) NSMutableArray<NSURL*>* urls;
@property (nonatomic, strong) NSMutableArray<NSNumber*>* activations;
@property (nonatomic, strong) NSMutableArray* callbacks;
@property (nonatomic, copy) NSArray<NSURL*>* revealed;
@end
@implementation ControlledWorkspace

- (instancetype)init
{
    self = [super init];
    if (self) {
        _hasHandler = YES;
        _urls = [NSMutableArray array];
        _activations = [NSMutableArray array];
        _callbacks = [NSMutableArray array];
    }

    return self;
}

- (NSURL*)URLForApplicationToOpenURL:(NSURL*)url
{
    (void)url;
    return self.hasHandler ? [NSURL fileURLWithPath:@"/controlled/Handler.app"] : nil;
}

- (void)openURL:(NSURL*)url
        configuration:(NSWorkspaceOpenConfiguration*)configuration
    completionHandler:(LaunchCallback)callback
{
    [self.urls addObject:url];
    [self.activations addObject:@(configuration.activates)];
    [self.callbacks addObject:[callback copy]];
}

- (void)activateFileViewerSelectingURLs:(NSArray<NSURL*>*)urls
{
    self.revealed = urls;
}

@end

namespace {
std::mutex result_mutex;
std::condition_variable result_ready;
std::vector<Result> results;
bool trash_success = true;
size_t trash_calls = 0;
size_t delete_calls = 0;
NSString* trashed_path;
bool passed = true;
}

@interface ControlledFileManager : NSObject
@end
@implementation ControlledFileManager

- (BOOL)trashItemAtURL:(NSURL*)url resultingItemURL:(NSURL**)result error:(NSError**)error
{
    (void)result;
    std::lock_guard lock(result_mutex);
    ++trash_calls;
    trashed_path = url.path;
    if (!trash_success && error)
        *error =
            [NSError errorWithDomain:@"controlled.trash"
                                code:1
                            userInfo:@{
                                NSLocalizedDescriptionKey : @"Controlled native trash failure Grüße"
                            }];
    return trash_success;
}

- (BOOL)removeItemAtURL:(NSURL*)url error:(NSError**)error
{
    (void)url;
    (void)error;
    std::lock_guard lock(result_mutex);
    ++delete_calls;
    return NO;
}

@end

namespace {
ControlledWorkspace* workspace;
ControlledFileManager* manager;

id workspace_peer(id, SEL)
{
    return workspace;
}

id manager_peer(id, SEL)
{
    return manager;
}

void check(bool condition, char const* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL native shell provider: %s\n", message);
        passed = false;
    }
}

class Interception {
public:
    Interception()
    {
        m_workspace = class_getClassMethod(NSWorkspace.class, @selector(sharedWorkspace));
        m_manager = class_getClassMethod(NSFileManager.class, @selector(defaultManager));
        if (!m_workspace || !m_manager)
            std::abort();
        m_original_workspace
            = method_setImplementation(m_workspace, reinterpret_cast<IMP>(workspace_peer));
        m_original_manager
            = method_setImplementation(m_manager, reinterpret_cast<IMP>(manager_peer));
    }

    ~Interception()
    {
        method_setImplementation(m_manager, m_original_manager);
        method_setImplementation(m_workspace, m_original_workspace);
    }

private:
    Method m_workspace {}, m_manager {};
    IMP m_original_workspace {}, m_original_manager {};
};

void collect(Result result)
{
    std::lock_guard lock(result_mutex);
    results.push_back(std::move(result));
    result_ready.notify_all();
}

bool await_results(size_t count)
{
    std::unique_lock lock(result_mutex);
    return result_ready.wait_for(
        lock, std::chrono::seconds(5), [&] { return results.size() >= count; });
}

void complete_launch(NSError* error = nil)
{
    check(workspace.callbacks.count > 0, "native launch callback exists");
    if (!workspace.callbacks.count)
        return;
    LaunchCallback callback = workspace.callbacks.firstObject;
    [workspace.callbacks removeObjectAtIndex:0];
    callback(nil, error);
}

void run()
{
    auto provider = Provider::create();
    check(workspace.urls.count == 0 && workspace.revealed == nil,
        "provider construction cannot launch a GUI application");
    // Foundation file URLs canonically decompose Unicode paths on macOS;
    // compare both the real file URL and composed full path, retaining %/#/spaces.
    auto reveal = provider->reveal("/controlled/Grüße %#.txt");
    check(reveal.code.empty() && workspace.revealed.count == 1 &&
            [workspace.revealed[0] isEqual:[NSURL fileURLWithPath:@"/controlled/Grüße %#.txt"]] &&
            [workspace.revealed[0].path.precomposedStringWithCanonicalMapping
                isEqual:@"/controlled/Grüße %#.txt"],
        "real reveal boundary receives exact Unicode/space/percent/hash file URL");
    check(provider->reveal("relative").code == "ERR_INVALID_ARG_VALUE",
        "native reveal refuses relative path");
    provider->open_external("https://example.invalid/Gr%C3%BC%C3%9Fe?q=a%20b#hash", false, collect);
    check(workspace.urls.count == 1 && !workspace.activations[0].boolValue && results.empty(),
        "launch uses actual configuration and awaits callback");
    complete_launch();
    check(results.size() == 1 && results.back().code.empty(),
        "native launch callback acknowledges success");
    provider->open_path("/controlled/Grüße %#.txt", collect);
    check(workspace.urls.count == 2 && workspace.urls[1].isFileURL &&
            [workspace.urls[1] isEqual:[NSURL fileURLWithPath:@"/controlled/Grüße %#.txt"]] &&
            [workspace.urls[1].path.precomposedStringWithCanonicalMapping
                isEqual:@"/controlled/Grüße %#.txt"]
            && workspace.activations[1].boolValue,
        "path opening is a file URL with default activation");
    auto failure =
        [NSError errorWithDomain:@"controlled.launch"
                            code:1
                        userInfo:@ {
                            NSLocalizedDescriptionKey : @"Controlled native launch failure 日本語"
                        }];
    complete_launch(failure);
    check(results.back().code == "ERR_FIREFLY_SHELL_NATIVE"
            && results.back().message.find("日本語") != std::string::npos,
        "native launch failure preserves Unicode NSError description");
    workspace.hasHandler = NO;
    auto launches = workspace.urls.count;
    provider->open_external("missing-handler:test", true, collect);
    check(workspace.urls.count == launches && results.back().code == "ERR_FIREFLY_SHELL_NATIVE",
        "unhandled URL scheme does not invoke system chooser or launch");
    workspace.hasHandler = YES;
    provider->open_external("not absolute", true, collect);
    check(results.back().code == "ERR_INVALID_URL", "relative URL rejected before native launch");
    provider->open_external(std::string("https://example.invalid/\0", 25), true, collect);
    check(results.back().code == "ERR_INVALID_URL", "NUL URL rejected");
    provider->open_path("relative", collect);
    check(results.back().code == "ERR_INVALID_ARG_VALUE", "relative file launch rejected");
    auto before = results.size();
    provider->trash("/controlled/Grüße %#.txt", collect);
    check(await_results(before + 1), "background native trash operation completes");
    {
        std::lock_guard lock(result_mutex);
        check(results.back().code.empty() && trash_calls == 1 &&
                [trashed_path.precomposedStringWithCanonicalMapping
                    isEqual:@"/controlled/Grüße %#.txt"],
            "real trash boundary receives exact file URL on a native worker");
        trash_success = false;
    }

    before = results.size();
    provider->trash("/controlled/failure", collect);
    check(await_results(before + 1), "background native trash failure completes");
    {
        std::lock_guard lock(result_mutex);
        check(results.back().code == "ERR_FIREFLY_SHELL_NATIVE"
                && results.back().message.find("Grüße") != std::string::npos && delete_calls == 0,
            "trash failure never falls back to permanent deletion");
    }

    provider->open_external("https://example.invalid/late", true, collect);
    before = results.size();
    provider->stop();
    complete_launch();
    check(results.size() == before, "stop revokes late OS launch completions");
    check(provider->reveal("/controlled/late").code == "ERR_FIREFLY_APP_EXITED",
        "stopped reveal refuses dispatch");
    provider->open_path("/controlled/late", collect);
    check(results.back().code == "ERR_FIREFLY_APP_EXITED",
        "stopped launcher cannot submit another OS job");
    provider.reset();
    puts("PASS exact macOS shell provider launch/reveal URL metadata, activation, errors and "
         "worker trash without delete fallback");

    results.clear();
    [workspace.urls removeAllObjects];
    [workspace.activations removeAllObjects];
    std::vector<std::unique_ptr<Provider>> generations;
    for (size_t index = 0; index < 64; ++index) {
        auto generation = Provider::create();
        generation->open_external("https://example.invalid/pending", true, collect);
        generation->stop();
        generations.push_back(std::move(generation));
    }

    check(workspace.callbacks.count == 64 && results.empty(),
        "stopped generations retain actual OS callback tickets");
    auto excess = Provider::create();
    excess->open_external("https://example.invalid/refused", true, collect);
    check(workspace.callbacks.count == 64 && results.size() == 1
            && results.back().code == "ERR_FIREFLY_SHELL_LIMIT",
        "global native quota64 bounds cancel/restart churn");
    complete_launch();
    excess->open_external("https://example.invalid/recovered", true, collect);
    check(workspace.callbacks.count == 64 && results.size() == 1,
        "only actual native completion frees a ticket for a new launch");
    excess->stop();
    excess.reset();
    generations.clear();
    while (workspace.callbacks.count)
        complete_launch();
    check(results.size() == 1, "callbacks after all native provider destructors cannot deliver");
    puts("PASS exact macOS shell provider global native ticket bound, late completions and "
         "terminal ownership");
}
}

int main()
{
    @autoreleasepool {
        workspace = [ControlledWorkspace new];
        manager = [ControlledFileManager new];
        // Originals are retained for restoration only, never invoked. Unknown
        // selectors fail loudly rather than falling back to a real desktop peer.
        Interception interception;
        @try {
            run();
        } @catch (NSException* exception) {
            fprintf(stderr, "Controlled desktop peer rejected selector: %s\n",
                exception.description.UTF8String);
            return 1;
        }

        if (!passed)
            return 1;
        puts("Controlled Objective-C peers only: no browser, Finder, application launch, trash "
             "mutation or sound");
        return 0;
    }
}
