#pragma once

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

class ModuleLoader {
public:
    ModuleLoader(JS::VM&, JS::Realm&, EventLoop&, std::filesystem::path entry_path);
    JS::ThrowCompletionOr<void> install_globals();
    JS::ThrowCompletionOr<GC::Ref<JS::Object>> builtin_exports(std::string const& name) { return builtin(name); }
    // Trusted native hosts register exports before application entry execution.
    // Only this loader/realm gains the module; Node builtins cannot be replaced.
    JS::ThrowCompletionOr<void> register_host_module(std::string const& name,
        GC::Ref<JS::Object> exports, std::vector<std::string> named_exports);
    JS::ThrowCompletionOr<bool> is_es_module(std::filesystem::path const&);
    JS::ThrowCompletionOr<GC::Ref<JS::Promise>> run_entry();
    JS::ThrowCompletionOr<JS::Value> run_eval(std::string const&);
    JS::ThrowCompletionOr<JS::Value> run_commonjs_entry();

private:
    struct RequireResolution {
        std::filesystem::path path;
        std::string builtin;
        std::string identifier;
        std::string host;
    };
    JS::ThrowCompletionOr<GC::Ref<JS::Object>> builtin(std::string const&);
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
    JS::ThrowCompletionOr<Optional<std::filesystem::path>> package_scope(std::filesystem::path const&);
    JS::ThrowCompletionOr<URL::URL> resolve_package(std::string const&, std::filesystem::path const&, bool require, size_t depth = 0);
    JS::ThrowCompletionOr<URL::URL> resolve_exports(std::filesystem::path const&, std::string const&, AK::JsonValue const&, bool require, size_t depth);
    JS::ThrowCompletionOr<PackageTarget> resolve_map(std::filesystem::path const&, AK::JsonObject const&, std::string const&, bool imports, bool require, size_t depth);
    JS::ThrowCompletionOr<PackageTarget> resolve_target(std::filesystem::path const&, AK::JsonValue const&, Optional<std::string> const& pattern, bool imports, bool require, size_t depth);
    JS::ThrowCompletionOr<Optional<AK::JsonValue>> read_package(std::filesystem::path const&);
    JS::ThrowCompletionOr<Optional<std::filesystem::path>> legacy_path(std::filesystem::path const&);
    JS::ThrowCompletionOr<RequireResolution> resolve_require(std::string const&, std::filesystem::path const&);
    JS::ThrowCompletionOr<JS::Value> load_commonjs(std::filesystem::path const&, GC::Ptr<JS::Object> parent = nullptr, bool main = false);
    GC::Ref<JS::NativeFunction> make_require(std::filesystem::path const&, GC::Ref<JS::Object>);
    JS::ThrowCompletionOr<void> add_child(JS::Object&, JS::Object&);
    JS::ThrowCompletionOr<void> remove_child(JS::Object&, JS::Object&);
    JS::Completion require_error(JS::Completion, JS::Object&);
    JS::Completion error(char const* code, std::string const& message, bool type_error = false);
    JS::ThrowCompletionOr<URL::URL> resolve(std::string const& specifier, std::string const& referrer);
    JS::ThrowCompletionOr<GC::Ref<JS::Module>> load(URL::URL const&);
    std::string referrer_url(JS::ImportedModuleReferrer const&) const;
    static URL::URL path_to_url(std::filesystem::path const&);

    JS::VM& m_vm;
    JS::Realm& m_realm;
    EventLoop& m_event_loop;
    std::filesystem::path m_entry_path;
    std::string m_entry_url;
    GC::Root<JS::Object> m_promise_constructor;
    GC::Root<JS::Object> m_process;
    GC::Root<JS::Object> m_commonjs_cache;
    GC::Root<JS::Object> m_main_module;
    GC::Root<JS::Module> m_commonjs_bridge;
    GC::Root<JS::Module> m_builtin_bridge;
    GC::Root<JS::Module> m_host_bridge;
    std::unordered_map<std::string, HostModule> m_host_modules;
    std::unordered_set<void const*> m_host_shims;
    std::unordered_set<void const*> m_builtin_shims;
    std::unordered_map<std::string, GC::Root<JS::Object>> m_builtins;
    std::unordered_set<void const*> m_commonjs_shims;
    std::unordered_map<std::string, GC::Root<JS::Module>> m_modules;
};

}
