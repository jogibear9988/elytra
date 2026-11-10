/**
 * Resolves and evaluates application ESM and CommonJS modules. Cache identity, package
 * conditions and filesystem paths are shared across both loaders; native builtin registration
 * stays realm-local.
 */

#pragma once

#include "core/GCCaptures.h"
#include <AK/JsonValue.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Module.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/SourceTextModule.h>
#include <LibURL/URL.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Mite {

class EventLoop;

class ModuleLoader : public NativeCaptureOwner<ModuleLoader> {
public:
    void visit_capture_edges(GC::Cell::Visitor&) const;
    ModuleLoader(JS::VM&, JS::Realm&, EventLoop&, std::filesystem::path entry_path);

    ~ModuleLoader();

    JS::ThrowCompletionOr<void> install_globals(
        bool allow_process_signals = false, bool fork_child = false);

    JS::ThrowCompletionOr<GC::Ref<JS::Object>> builtin_exports(std::string const& name)
    {
        return builtin(name);
    }

    // Original transport primitives for trusted embeddings, captured before application
    // code can replace globals. This object is never a public JavaScript builtin.
    JS::ThrowCompletionOr<GC::Ref<JS::Object>> web_transport();

    // Trusted native hosts register exports before application entry execution.
    // Only this loader/realm gains the module; Node builtins cannot be replaced.
    JS::ThrowCompletionOr<void> register_host_module(std::string const& name,
        GC::Ref<JS::Object> exports, std::vector<std::string> named_exports);

    // Trusted hosts can expose a separate raw filesystem facade. It shares native
    // descriptor services, not the mutable public fs/promises JavaScript objects.
    JS::ThrowCompletionOr<void> register_host_filesystem(std::string const& name);

    JS::ThrowCompletionOr<bool> is_es_module(std::filesystem::path const&);

    JS::ThrowCompletionOr<GC::Ref<JS::Promise>> run_entry();

    JS::ThrowCompletionOr<JS::Value> run_eval(std::string const&);

    JS::ThrowCompletionOr<JS::Value> run_commonjs_entry();

private:
    JS::ThrowCompletionOr<void> install_policy_watcher();

    struct RequireResolution {
        std::filesystem::path path;
        std::string builtin;
        std::string identifier;
        std::string host;
    };

    JS::ThrowCompletionOr<GC::Ref<JS::Object>> builtin(std::string const&);

    JS::ThrowCompletionOr<GC::Ref<JS::Object>> construct_builtin(std::string const&);

    JS::ThrowCompletionOr<GC::Ref<JS::Module>> builtin_module(std::string const&);

    struct HostModule {
        GC::Root<JS::Object> exports;
        std::vector<std::string> named_exports;
    };

    JS::ThrowCompletionOr<GC::Ref<JS::Module>> host_module(std::string const&);

    struct PackageTarget {
        enum class State { Unmatched, Blocked, Resolved };
        State state { State::Unmatched };
        Optional<URL::URL> url;
    };

    JS::ThrowCompletionOr<Optional<std::filesystem::path>> package_scope(
        std::filesystem::path const&);

    JS::ThrowCompletionOr<URL::URL> resolve_package(std::string const&,
        std::filesystem::path const&, bool require, size_t depth = 0,
        std::vector<std::string> const* conditions = nullptr);

    JS::ThrowCompletionOr<URL::URL> resolve_exports(std::filesystem::path const&,
        std::string const&, AK::JsonValue const&, bool require, size_t depth,
        std::vector<std::string> const* conditions);

    JS::ThrowCompletionOr<PackageTarget> resolve_map(std::filesystem::path const&,
        AK::JsonObject const&, std::string const&, bool imports, bool require, size_t depth,
        std::vector<std::string> const* conditions);

    JS::ThrowCompletionOr<PackageTarget> resolve_target(std::filesystem::path const&,
        AK::JsonValue const&, Optional<std::string> const& pattern, bool imports, bool require,
        size_t depth, std::vector<std::string> const* conditions);

    JS::ThrowCompletionOr<Optional<AK::JsonValue>> read_package(std::filesystem::path const&);

    JS::ThrowCompletionOr<Optional<std::filesystem::path>> legacy_path(
        std::filesystem::path const&);

    JS::ThrowCompletionOr<RequireResolution> resolve_require(std::string const&,
        std::filesystem::path const&, std::vector<std::string> const* conditions = nullptr);

    JS::ThrowCompletionOr<RequireResolution> resolve_require_hooked(
        std::string const&, std::filesystem::path const&);

    // Only trusted main-process module resolution enters these realm-owned hooks.
    JS::ThrowCompletionOr<JS::Value> run_resolve_hooks(
        std::string const&, std::string const&, bool require, JS::Value attributes);

    JS::ThrowCompletionOr<URL::URL> hook_result_url(JS::Object&, std::string const&);

    void install_resolve_hook_host(JS::Object&);

    JS::ThrowCompletionOr<JS::Value> load_commonjs(
        std::filesystem::path const&, GC::Ptr<JS::Object> parent = nullptr, bool main = false);

    JS::ThrowCompletionOr<std::vector<Utf16String>> commonjs_export_names(
        std::filesystem::path const&, size_t depth = 0);

    JS::ThrowCompletionOr<JS::Value> commonjs_namespace_snapshot(
        std::filesystem::path const&, JS::Value names);

    static std::string quote_export_name(Utf16String const&);

    GC::Ref<JS::NativeFunction> make_require(std::filesystem::path const&, GC::Ref<JS::Object>);

    JS::ThrowCompletionOr<GC::Ref<JS::Object>> make_commonjs_module(
        std::filesystem::path const&, GC::Ptr<JS::Object>, bool main);

    void install_module_utilities(JS::Object&);

    JS::ThrowCompletionOr<void> add_child(JS::Object&, JS::Object&);

    JS::ThrowCompletionOr<void> remove_child(JS::Object&, JS::Object&);

    JS::Completion require_error(JS::Completion, JS::Object&);

    JS::Completion error(char const* code, std::string const& message, bool type_error = false);

    JS::ThrowCompletionOr<URL::URL> resolve(std::string const& specifier,
        std::string const& referrer, std::vector<std::string> const* conditions = nullptr);

    JS::ThrowCompletionOr<GC::Ref<JS::Module>> load(URL::URL const&);

    std::string referrer_url(JS::ImportedModuleReferrer const&) const;

    static URL::URL path_to_url(std::filesystem::path const&);

    bool m_fork_child { false };
    JS::VM& m_vm;
    JS::Realm& m_realm;
    EventLoop& m_event_loop;
    std::filesystem::path m_entry_path;
    std::string m_entry_url;
    GC::Root<JS::Object> m_promise_constructor;
    GC::Root<JS::Object> m_process;
    GC::Root<JS::Object> m_commonjs_cache;
    GC::Root<JS::Object> m_filesystem_native;
    GC::Root<JS::Object> m_main_module;
    GC::Root<JS::Module> m_commonjs_bridge;
    GC::Root<JS::Object> m_commonjs_lexer;
    GC::Root<JS::Object> m_commonjs_esm_exports;
    std::unordered_map<std::string, std::vector<Utf16String>> m_commonjs_export_names;
    GC::Root<JS::Module> m_builtin_bridge;
    GC::Root<JS::Module> m_host_bridge;
    std::unordered_map<std::string, HostModule> m_host_modules;
    std::unordered_set<void const*> m_host_shims;
    std::unordered_set<void const*> m_builtin_shims;
    GC::Root<JS::Object> m_abort_hooks;
    GC::Root<JS::Object> m_web_streams;
    GC::Root<JS::Object> m_web_stream_state;
    GC::Root<JS::Object> m_web_transport;
    GC::Root<JS::Object> m_web_crypto;
    std::unordered_map<std::string, GC::Root<JS::Object>> m_transport_dependencies;
    bool m_transport_captured {};
    GC::Root<JS::Object> m_http_client_hooks;
    GC::Root<JS::Object> m_tls_client_hooks;
    GC::Root<JS::Object> m_resolve_hook_dispatcher;
    GC::Root<JS::Object> m_promisify_state;
    std::unordered_map<std::string, GC::Root<JS::Object>> m_builtins;
    std::unordered_set<void const*> m_commonjs_shims;
    std::unordered_map<std::string, GC::Root<JS::Module>> m_modules;
};

}
