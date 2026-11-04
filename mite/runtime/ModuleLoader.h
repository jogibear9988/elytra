#pragma once

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

namespace Mite {

class ModuleLoader {
public:
    ModuleLoader(JS::VM&, JS::Realm&, std::filesystem::path entry_path);
    JS::ThrowCompletionOr<bool> is_es_module(std::filesystem::path const&);
    JS::ThrowCompletionOr<GC::Ref<JS::Promise>> run_entry();
    void register_script(JS::Script&);

private:
    JS::Completion error(char const* code, std::string const& message, bool type_error = false);
    JS::ThrowCompletionOr<URL::URL> resolve(std::string const& specifier, std::string const& referrer);
    JS::ThrowCompletionOr<GC::Ref<JS::Module>> load(URL::URL const&);
    std::string referrer_url(JS::ImportedModuleReferrer const&) const;
    static URL::URL path_to_url(std::filesystem::path const&);

    JS::VM& m_vm;
    JS::Realm& m_realm;
    std::filesystem::path m_entry_path;
    std::string m_entry_url;
    GC::Root<JS::Object> m_promise_constructor;
    std::unordered_map<std::string, GC::Root<JS::Module>> m_modules;
    std::unordered_map<void const*, std::string> m_urls;
    GC::RootVector<GC::Ref<JS::Script>> m_scripts;
};

}
