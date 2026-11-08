/** Actual NSWorkspace dispatch and NSFileManager trash; no shell command or delete fallback. */
#include "Provider.h"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace Firefly::Shell {
namespace {
    constexpr unsigned request_limit = 64;
    std::atomic<unsigned> inflight { 0 };

    struct Ticket {
        std::atomic<bool> finished { false };

        void release()
        {
            if (!finished.exchange(true))
                inflight.fetch_sub(1);
        }
    };

    std::shared_ptr<Ticket> ticket()
    {
        auto count = inflight.load();
        while (count < request_limit)
            if (inflight.compare_exchange_weak(count, count + 1))
                return std::make_shared<Ticket>();
        return {};
    }

    Result failure(char const* code, char const* message)
    {
        return { code, message };
    }

    NSString* text(std::string const& value, size_t limit)
    {
        if (value.empty() || value.size() > limit || value.find('\0') != std::string::npos)
            return nil;
        return [[NSString alloc] initWithBytes:value.data()
                                        length:value.size()
                                      encoding:NSUTF8StringEncoding];
    }

    Result native_error(NSError* error, char const* fallback)
    {
        auto description = error.localizedDescription;
        auto bytes = description.UTF8String;
        return { "ERR_FIREFLY_SHELL_NATIVE", bytes ? std::string(bytes) : std::string(fallback) };
    }

    struct State {
        std::mutex mutex;
        bool stopped { false };
        uint64_t next {};
        std::unordered_map<uint64_t, Completion> pending;
    };

    void finish(
        std::weak_ptr<State> weak, uint64_t id, std::shared_ptr<Ticket> const& lease, Result result)
    {
        lease->release();
        auto state = weak.lock();
        if (!state)
            return;
        Completion callback;
        {
            std::lock_guard lock(state->mutex);
            auto found = state->pending.find(id);
            if (state->stopped || found == state->pending.end())
                return;
            callback = std::move(found->second);
            state->pending.erase(found);
        }
        // Callbacks can run synchronously or on an arbitrary native worker. The
        // host, not this provider, owns VM-thread marshaling and result framing.
        callback(std::move(result));
    }

    class MacProvider final : public Provider {
    public:
        MacProvider()
            : m_state(std::make_shared<State>())
        {
        }

        ~MacProvider() override
        {
            stop();
        }

        Result reveal(std::string const& path) override
        {
            {
                std::lock_guard lock(m_state->mutex);
                if (m_state->stopped)
                    return failure("ERR_FIREFLY_APP_EXITED", "Shell provider stopped");
            }

            auto value = text(path, 32768);
            if (!value || !value.isAbsolutePath)
                return failure("ERR_INVALID_ARG_VALUE", "Expected an absolute UTF-8 path");
            if (!NSThread.isMainThread)
                return failure(
                    "ERR_FIREFLY_SHELL_NATIVE", "File reveal requires the native owner thread");
            @try {
                auto url = [NSURL fileURLWithPath:value];
                auto workspace = NSWorkspace.sharedWorkspace;
                if (!workspace || !url)
                    return failure("ERR_FIREFLY_SHELL_NATIVE", "Native file viewer unavailable");
                [workspace activateFileViewerSelectingURLs:@[ url ]];
                // This acknowledges native dispatch, not successful Finder selection.
                return {};
            } @catch (NSException*) {
                return failure("ERR_FIREFLY_SHELL_NATIVE", "Native file reveal failed");
            }
        }

        void open_external(std::string value, bool activate, Completion callback) override
        {
            auto string = text(value, 65536);
            auto url = string ? [NSURL URLWithString:string] : nil;
            if (!url || !url.scheme.length) {
                callback(failure("ERR_INVALID_URL", "Expected an absolute UTF-8 URL"));
                return;
            }

            open(url, activate, std::move(callback));
        }

        void open_path(std::string path, Completion callback) override
        {
            auto value = text(path, 32768);
            if (!value || !value.isAbsolutePath) {
                callback(failure("ERR_INVALID_ARG_VALUE", "Expected an absolute UTF-8 path"));
                return;
            }

            open([NSURL fileURLWithPath:value], true, std::move(callback));
        }

        void trash(std::string path, Completion callback) override
        {
            auto value = text(path, 32768);
            if (!value || !value.isAbsolutePath) {
                callback(failure("ERR_INVALID_ARG_VALUE", "Expected an absolute UTF-8 path"));
                return;
            }

            auto url = [NSURL fileURLWithPath:value];
            submit(std::move(callback),
                [url](std::weak_ptr<State> weak, uint64_t id, std::shared_ptr<Ticket> lease) {
                    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                        @autoreleasepool {
                            NSError* error = nil;
                            BOOL success = NO;
                            @try {
                                success = [NSFileManager.defaultManager trashItemAtURL:url
                                                                      resultingItemURL:nil
                                                                                 error:&error];
                            } @catch (NSException*) {
                                finish(weak, id, lease,
                                    failure("ERR_FIREFLY_SHELL_NATIVE",
                                        "Native trash operation failed"));
                                return;
                            }

                            finish(weak, id, lease,
                                success ? Result {}
                                        : native_error(error, "Native trash operation failed"));
                        }
                    });
                });
        }

        void stop() override
        {
            std::lock_guard lock(m_state->mutex);
            m_state->stopped = true;
            m_state->pending.clear();
        }

    private:
        template <typename Submit> void submit(Completion callback, Submit operation)
        {
            auto lease = ticket();
            if (!lease) {
                callback(failure("ERR_FIREFLY_SHELL_LIMIT", "Too many native shell requests"));
                return;
            }

            uint64_t id {};
            Result error;
            {
                std::lock_guard lock(m_state->mutex);
                if (m_state->stopped)
                    error = failure("ERR_FIREFLY_APP_EXITED", "Shell provider stopped");
                else if (m_state->next == std::numeric_limits<uint64_t>::max())
                    error = failure("ERR_FIREFLY_SHELL_LIMIT", "Shell request identity exhausted");
                else {
                    id = ++m_state->next;
                    m_state->pending.emplace(id, std::move(callback));
                }
            }

            if (!error.code.empty()) {
                lease->release();
                callback(std::move(error));
                return;
            }

            operation(std::weak_ptr(m_state), id, std::move(lease));
        }

        void open(NSURL* url, bool activate, Completion callback)
        {
            if (!NSThread.isMainThread) {
                callback(
                    failure("ERR_FIREFLY_SHELL_NATIVE", "Native launch requires the owner thread"));
                return;
            }

            submit(std::move(callback),
                [url, activate](
                    std::weak_ptr<State> weak, uint64_t id, std::shared_ptr<Ticket> lease) {
                    @try {
                        auto workspace = NSWorkspace.sharedWorkspace;
                        // Matching Electron: avoid a system chooser/dialog for an
                        // unhandled scheme rather than invoking a command-line launcher.
                        if (!workspace || ![workspace URLForApplicationToOpenURL:url]) {
                            finish(weak, id, lease,
                                failure("ERR_FIREFLY_SHELL_NATIVE",
                                    "No application found to open URL"));
                            return;
                        }

                        auto configuration = NSWorkspaceOpenConfiguration.configuration;
                        configuration.activates = activate;
                        [workspace openURL:url
                                configuration:configuration
                            completionHandler:^(NSRunningApplication*, NSError* error) {
                                finish(weak, id, lease,
                                    error ? native_error(error, "Native launch failed")
                                          : Result {});
                            }];
                    } @catch (NSException*) {
                        finish(weak, id, lease,
                            failure("ERR_FIREFLY_SHELL_NATIVE", "Native launch failed"));
                    }
                });
        }

        std::shared_ptr<State> m_state;
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<MacProvider>();
}
}
