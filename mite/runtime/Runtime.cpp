#include "Runtime.h"
#include "EventLoop.h"
#include <AK/ScopeGuard.h>
#include "ModuleLoader.h"
#include "ModulePaths.h"
#include "ProcessHost.h"
#include <uv.h>
#include <AK/Utf16String.h>
#include <LibGC/Function.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/ExecutionContext.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void write_text(FILE* stream, Utf16View text)
{
    auto bytes = MUST(text.to_byte_string());
    fwrite(bytes.characters(), 1, bytes.length(), stream);
}

static void report_exception(JS::Value value)
{
    fputs("Uncaught ", stderr);
    if (value.is_object() && JS::Error::is_engine_class_of(value.as_object())) {
        auto& error = static_cast<JS::Error&>(value.as_object());
        auto name = error.get_without_side_effects("name"_utf16);
        auto message = error.get_without_side_effects("message"_utf16);
        write_text(stderr, name.is_string() ? name.as_string().utf16_string_view() : "Error"_utf16);
        if (message.is_string() && message.as_string().length_in_utf16_code_units()) {
            fputs(": ", stderr);
            write_text(stderr, message.as_string().utf16_string_view());
        }
        fputc('\n', stderr);
        write_text(stderr, error.stack_string());
    } else {
        write_text(stderr, value.to_utf16_string_without_side_effects());
    }
    fputc('\n', stderr);
}

static JS::ThrowCompletionOr<JS::Value> print(JS::VM& vm, FILE* stream)
{
    for (size_t i = 0; i < vm.argument_count(); ++i) {
        auto text = TRY(vm.argument(i).to_utf16_string(vm));
        if (i)
            fputc(' ', stream);
        write_text(stream, text);
    }
    fputc('\n', stream);
    // Servers must publish readiness/log lines while the process remains alive,
    // including when stdout is a pipe rather than a terminal.
    fflush(stream);
    return JS::js_undefined();
}

int Mite::run(int argc, char** argv, HostHooks const& hooks)
{
    argv = uv_setup_args(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "--version") {
        puts("mite 0.1.0-dev (LibJS)");
        return 0;
    }
    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        puts("Usage: mite <script.js> [arguments...]\n       mite -e <source>\n       mite --version");
        return argc == 1 ? 1 : 0;
    }

    std::string filename;
    std::filesystem::path native_filename;
    std::string source;
    int script_arg = 1;
    if (std::string(argv[1]) == "-e" || std::string(argv[1]) == "--eval") {
        if (argc < 3) {
            fputs("mite: --eval requires source text\n", stderr);
            return 9;
        }
        filename = "[eval]";
        source = argv[2];
        script_arg = 2;
    } else {
        if (argv[1][0] == '-') {
            fprintf(stderr, "mite: unsupported option: %s\n", argv[1]);
            return 9;
        }
        std::error_code error;
        native_filename = std::filesystem::absolute(Mite::ModulePaths::path_from_utf8(argv[1]), error);
        if (error) {
            fprintf(stderr, "mite: cannot resolve script: %s\n", error.message().c_str());
            return 1;
        }
        filename = Mite::ModulePaths::path_string(native_filename);
        bool is_file = std::filesystem::is_regular_file(native_filename, error);
        std::ifstream input;
        if (is_file && !error)
            input.open(native_filename, std::ios::binary);
        if (!input.is_open()) {
            fprintf(stderr, "mite: cannot read script: %s\n", filename.c_str());
            return 1;
        }
        source.assign(std::istreambuf_iterator<char>(input), {});
        if (input.bad()) {
            fprintf(stderr, "mite: error reading script: %s\n", filename.c_str());
            return 1;
        }
        if (source.starts_with("#!")) {
            auto end = source.find('\n');
            source.replace(0, end == std::string::npos ? source.size() : end, "//");
        }
    }

    auto vm = JS::VM::create();
    auto context_result = JS::Realm::initialize_host_defined_realm(*vm, {}, {});
    if (context_result.is_error()) {
        report_exception(context_result.error_value());
        return 1;
    }
    auto context = context_result.release_value();
    auto& realm = *context->realm;
    GC::Root realm_root { realm };
    auto& global = realm.global_object();

    auto console = JS::Object::create(realm, realm.intrinsics().object_prototype());
    global.define_direct_property("console"_utf16, console, JS::default_attributes);
    console->define_native_function(realm, "log"_utf16,
        +[](JS::VM& vm) { return print(vm, stdout); }, 0, JS::default_attributes);
    console->define_native_function(realm, "error"_utf16,
        +[](JS::VM& vm) { return print(vm, stderr); }, 0, JS::default_attributes);
    global.define_direct_property("global"_utf16, &global, JS::default_attributes);

    // Initialize the owned libuv loop before process queries: Windows libuv's
    // uv_os_getppid uses WinAPI pointers initialized by uv_loop_init.
    Mite::EventLoop event_loop(*vm, report_exception);
    auto process_result = Mite::create_process(*vm, realm);
    if (process_result.is_error()) {
        report_exception(process_result.error_value());
        vm->pop_execution_context();
        return 1;
    }
    GC::Root process { process_result.release_value() };
    global.define_direct_property("process"_utf16, process.ptr(), JS::default_attributes);
    GC::RootVector<JS::Value> args;
    args.append(process->get_without_side_effects("execPath"_utf16));
    process->define_direct_property("argv0"_utf16,
        JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(argv[0], strlen(argv[0])))), JS::Attribute::Enumerable);
    if (filename != "[eval]")
        args.append(JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(filename.data(), filename.size()))));
    for (int i = script_arg + 1; i < argc; ++i)
        args.append(JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(argv[i], strlen(argv[i])))));
    process->define_direct_property("argv"_utf16, JS::Array::create_from(realm, args), JS::default_attributes);

    auto installed = event_loop.install(realm);
    if (installed.is_error()) {
        report_exception(installed.error_value());
        vm->pop_execution_context();
        return 1;
    }

    auto entry_path = filename == "[eval]" ? std::filesystem::current_path() / "[eval]" : native_filename;
    Mite::ModuleLoader modules(*vm, realm, event_loop, entry_path);
    auto globals = modules.install_globals();
    if (globals.is_error()) {
        report_exception(globals.error_value());
        vm->pop_execution_context();
        return 1;
    }
    bool host_active = true;
    auto stop_host = [&] {
        if (!host_active) return;
        host_active = false;
        if (hooks.shutdown) hooks.shutdown();
    };
    ScopeGuard stop_guard(stop_host);
    if (hooks.initialize) {
        auto result = hooks.initialize(*vm, realm, modules, event_loop);
        if (result.is_error()) {
            report_exception(result.error_value());
            stop_host();
            if (event_loop.has_pending_work()) {
                // A partially initialized host can already own blocking work.
                fflush(nullptr);
                std::_Exit(1);
            }
            vm->pop_execution_context();
            return 1;
        }
    }
    auto drive = [&] { return hooks.drive ? hooks.drive(event_loop) : event_loop.run_until_idle(); };
    auto module_format = filename == "[eval]" ? JS::ThrowCompletionOr<bool>(false) : modules.is_es_module(entry_path);
    int status = 0;
    if (module_format.is_error()) {
        report_exception(module_format.error_value());
        status = 1;
    } else if (module_format.value()) {
        auto result = modules.run_entry();
        if (result.is_error()) {
            report_exception(result.error_value());
            status = 1;
        } else {
            GC::Root completion { result.release_value() };
            status = drive();
            if (!hooks.exit_code.has_value() && !status && completion->state() == JS::Promise::State::Pending) {
                auto requested = process->get("exitCode"_utf16);
                if (requested.is_error()) {
                    report_exception(requested.error_value());
                    status = 1;
                } else if (requested.value().is_int32()) {
                    status = requested.value().as_i32();
                } else {
                    fputs("Unsettled top-level await in entry module\n", stderr);
                    status = 13;
                }
            } else if (!hooks.exit_code.has_value() && !status && completion->state() == JS::Promise::State::Rejected) {
                report_exception(completion->result());
                status = 1;
            }
        }
    } else if (filename != "[eval]") {
        auto result = modules.run_commonjs_entry();
        if (result.is_error()) {
            report_exception(result.error_value());
            status = 1;
        } else {
            status = drive();
        }
    } else {
        auto result = modules.run_eval(source);
        if (result.is_error()) {
            report_exception(result.error_value());
            status = 1;
        } else {
            status = drive();
        }
    }
    if (!hooks.exit_code.has_value() && (status == 0 || status == 13)) {
        auto code = process->get("exitCode"_utf16);
        if (code.is_error()) {
            report_exception(code.error_value());
            status = 1;
        } else if (code.value().is_int32()) {
            status = code.value().as_i32();
        }
    }
    if (hooks.exit_code.has_value()) status = *hooks.exit_code;
    stop_host();
    if ((status || hooks.terminate_pending_work_on_stop) && event_loop.has_pending_work()) {
        // Fatal termination must not wait for a blocking filesystem worker (e.g.
        // a FIFO with no writer). The OS reclaims requests and handles at exit.
        fflush(nullptr);
        std::_Exit(status);
    }
    vm->pop_execution_context();
    return status;
}
