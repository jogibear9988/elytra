/**
 * Owns the LibJS VM, realm and application lifetime. Embedding hooks run on the VM owner
 * thread; teardown disconnects native callbacks before releasing JavaScript state.
 */

#include "core/Runtime.h"
#include "core/EventLoop.h"
#include "core/console/Formatting.h"
#include "core/errors/StackTrace.h"
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
#include "platform/UV.h"
#include "platform/network/tls/ExtraCertificates.h"
#include "platform/process/InheritedPipe.h"
#include "platform/process/ProcessHost.h"
#include "platform/terminal/pty/Posix.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16String.h>
#include <LibGC/Function.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ExecutionContext.h>
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
#include <string_view>
#include <vector>

static void write_text(FILE* stream, Utf16View text)
{
    auto bytes = MUST(text.to_byte_string());
    fwrite(bytes.characters(), 1, bytes.length(), stream);
}

// Observe loop ownership without changing references or running application JS.
// Explicit process.exit has its own marker because it need not return here.
static void diagnose_runtime_return(Mite::EventLoop& loop, int status, bool worker)
{
    if (!std::getenv("MITE_MODULE_DIAGNOSTIC"))
        return;

    struct Counts {
        unsigned handles {}, active {}, referenced {}, active_referenced {}, closing {};
        unsigned pipes {}, active_referenced_pipes {};
    } counts;

    uv_walk(
        &loop.native_loop(),
        [](uv_handle_t* handle, void* opaque) {
            auto& counts = *static_cast<Counts*>(opaque);
            bool active = uv_is_active(handle);
            bool referenced = uv_has_ref(handle);
            ++counts.handles;
            counts.active += active;
            counts.referenced += referenced;
            counts.active_referenced += active && referenced;
            counts.closing += uv_is_closing(handle) != 0;
            if (handle->type == UV_NAMED_PIPE) {
                ++counts.pipes;
                counts.active_referenced_pipes += active && referenced;
            }
        },
        &counts);
    fprintf(stderr,
        "MITE_RUNTIME_PHASE pid=%d stage=drive-return status=%d worker=%d uv_alive=%d "
        "pending=%d handles=%u active=%u referenced=%u active_referenced=%u closing=%u "
        "pipes=%u active_referenced_pipes=%u\n",
        static_cast<int>(uv_os_getpid()), status, worker, uv_loop_alive(&loop.native_loop()),
        loop.has_pending_work(), counts.handles, counts.active, counts.referenced,
        counts.active_referenced, counts.closing, counts.pipes, counts.active_referenced_pipes);
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
    auto formatted = TRY(Mite::format_console_arguments(vm));
    write_text(stream, formatted);
    fputc('\n', stream);
    // Servers must publish readiness/log lines while the process remains alive,
    // including when stdout is a pipe rather than a terminal.
    fflush(stream);
    return JS::js_undefined();
}

int Mite::run(int argc, char** argv, HostHooks const& hooks)
{
    if (auto status = Pty::maybe_run_pty_child(argc, argv))
        return *status;
    argv = uv_setup_args(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "--version") {
        puts("mite 0.1.0-dev (LibJS)");
        return 0;
    }

    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        puts("Usage: mite [--expose-gc] <script.js> [arguments...]\n       mite [--expose-gc] -e "
             "<source>\n       mite [--expose-gc] -p "
             "<source>\n       mite "
             "--version");
        return argc == 1 ? 1 : 0;
    }

    // Only the explicit internal role may attach inherited descriptor 3. The
    // environment marker alone never turns an ordinary application into a child.
    bool fork_child = argc > 1 && std::string_view(argv[1]) == "--mite-fork-child";
    std::vector<char*> fork_arguments;
    if (fork_child) {
        auto* descriptor = std::getenv("MITE_FORK_CHANNEL");
        if (argc < 3 || !descriptor || std::string_view(descriptor) != "3") {
            fputs("mite: fork-child role requires a script and inherited channel 3\n", stderr);
            return 9;
        }
        // Validate before reading the entry or creating a loop: otherwise a
        // missing inherited fd can be reused by our own libuv wakeup pipe.
        if (!Mite::is_inherited_ipc_pipe(3)) {
            fputs("mite: fork-child inherited channel is not a pipe\n", stderr);
            return 9;
        }
        // Hidden bootstrap state is not application argv/environment or execArgv.
        if (uv_os_unsetenv("MITE_FORK_CHANNEL")) {
            fputs("mite: cannot retire fork bootstrap environment\n", stderr);
            return 9;
        }

        fork_arguments.push_back(argv[0]);
        for (int index = 2; index < argc; ++index)
            fork_arguments.push_back(argv[index]);
        fork_arguments.push_back(nullptr);
        --argc;
        argv = fork_arguments.data();
    }

    bool expose_gc = false;
    bool eval_mode = false;
    bool eval_source_provided = false;
    bool print_completion = false;
    std::vector<std::string> exec_options;
    std::string filename;
    std::filesystem::path native_filename;
    std::string source;
    int argument = 1;
    // Eval/print consume their source, not the remaining option prefix. Node
    // continues scanning options until -- or the first application argument.
    // A script filename itself ends option parsing; later flags are ordinary argv.
    while (argument < argc) {
        std::string_view option { argv[argument] };
        if (option == "--") {
            ++argument;
            break;
        }

        if (option == "--expose-gc") {
            expose_gc = true;
            exec_options.emplace_back(argv[argument++]);
            continue;
        }

        bool print_option = option == "-p" || option == "--print";
        if (print_option || option == "-e" || option == "--eval") {
            exec_options.emplace_back(argv[argument++]);
            eval_mode = true;
            print_completion |= print_option;
            if (argument == argc || argv[argument][0] == '-') {
                if (print_option)
                    continue; // Node's print source is optional; options still follow.
                fputs("mite: --eval requires source text\n", stderr);
                return 9;
            }

            source = argv[argument];
            eval_source_provided = true;
            exec_options.emplace_back(argv[argument++]);
            continue;
        }

        if (option == "--version") {
            puts("mite 0.1.0-dev (LibJS)");
            return 0;
        }

        if (option == "--help") {
            puts("Usage: mite [--expose-gc] <script.js> [arguments...]\n"
                 "       mite [--expose-gc] -e <source>\n"
                 "       mite [--expose-gc] -p <source>\n"
                 "       mite --version");
            return 0;
        }

        if (!option.empty() && option.front() == '-') {
            fprintf(stderr, "mite: unsupported option: %s\n", argv[argument]);
            return 9;
        }

        break;
    }

    int application_argument = argument;
    if (eval_mode && !eval_source_provided && argument < argc) {
        // A bare print flag followed later by a filename (for example after a
        // native option or --) still selects that script, not piped evaluation.
        eval_mode = false;
        print_completion = false;
    }

    if (eval_mode) {
        if (!eval_source_provided) {
            // Optional --print source means evaluate real piped stdin, not an
            // invented empty expression. Interactive evaluation needs a REPL.
            if (uv_guess_handle(0) == UV_TTY) {
                fputs("mite: interactive stdin/REPL is not implemented\n", stderr);
                return 9;
            }

            char bytes[8192];
            for (;;) {
                auto count = fread(bytes, 1, sizeof(bytes), stdin);
                source.append(bytes, count);
                if (count != sizeof(bytes)) {
                    if (ferror(stdin)) {
                        fputs("mite: cannot read print source from stdin\n", stderr);
                        return 1;
                    }

                    break;
                }
            }
        }

        filename = "[eval]";
    } else {
        if (argument == argc) {
            fputs("mite: expected a script or --eval/--print source after options\n", stderr);
            return 9;
        }

        application_argument = argument + 1;
        std::error_code error;
        native_filename
            = std::filesystem::absolute(Mite::ModulePaths::path_from_utf8(argv[argument]), error);
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

    OwnerSession session;
    session.entry_path
        = filename == "[eval]" ? std::filesystem::current_path() / "[eval]" : native_filename;
    session.source = std::move(source);
    session.eval = filename == "[eval]";
    session.print_completion = print_completion;
    session.expose_gc = expose_gc;
    session.exec_argv = std::move(exec_options);
    session.fork_child = fork_child;
    session.argv.emplace_back(argv[0]);
    if (!session.eval)
        session.argv.push_back(filename);
    for (int index = application_argument; index < argc; ++index)
        session.argv.emplace_back(argv[index]);

    capture_tls_extra_certificates();
    return run_owner_session(session, hooks);
}

int Mite::run_owner_session(OwnerSession const& session, HostHooks const& hooks)
{
    auto filename
        = session.eval ? std::string("[eval]") : ModulePaths::path_string(session.entry_path);
    auto const& source = session.source;
    auto const& entry_path = session.entry_path;
    bool print_completion = session.print_completion;
    bool fork_child = session.fork_child;
    auto report = [&](JS::Value value) {
        if (hooks.exception)
            hooks.exception(value);
        else
            report_exception(value);
    };

    auto vm = JS::VM::create();
    auto context_result = JS::Realm::initialize_host_defined_realm(*vm, {}, {});
    if (context_result.is_error()) {
        report(context_result.error_value());
        return 1;
    }

    auto context = context_result.release_value();
    auto& realm = *context->realm;
    GC::Root realm_root { realm };
    auto& global = realm.global_object();

    auto console = JS::Object::create(realm, realm.intrinsics().object_prototype());
    global.define_direct_property("console"_utf16, console, JS::default_attributes);
    console->define_native_function(
        realm, "log"_utf16, +[](JS::VM& vm) { return print(vm, stdout); }, 0,
        JS::default_attributes);
    console->define_native_function(
        realm, "error"_utf16, +[](JS::VM& vm) { return print(vm, stderr); }, 0,
        JS::default_attributes);
    console->define_native_function(
        realm, "warn"_utf16, +[](JS::VM& vm) { return print(vm, stderr); }, 0,
        JS::default_attributes);
    global.define_direct_property("global"_utf16, &global, JS::default_attributes);
    if (session.expose_gc) {
        global.define_native_function(
            realm, "gc"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                // This is a real ordinary LibJS collection, preserving live
                // roots. CollectEverything is teardown-only, never public GC.
                // V8's generation/flavor/async option bags have no equivalent.
                if (vm.argument_count() > 1 || !vm.argument(0).is_undefined()) {
                    auto error = JS::Error::create(
                        vm.realm(), "Only synchronous gc() or gc(undefined) is supported"_utf16);
                    error->define_direct_property("code"_utf16,
                        JS::PrimitiveString::create(vm, "ERR_MITE_UNSUPPORTED_API"_utf16),
                        JS::default_attributes);
                    return JS::throw_completion(error);
                }

                vm.heap().collect_garbage(GC::Heap::CollectionType::CollectGarbage);
                return JS::js_undefined();
            },
            0, JS::Attribute::Writable | JS::Attribute::Enumerable);
    }

    // Initialize the owned libuv loop before process queries: Windows libuv's
    // uv_os_getppid uses WinAPI pointers initialized by uv_loop_init.
    Mite::EventLoop event_loop(*vm, report_exception);
    auto exit_state = std::make_shared<Mite::ProcessExitState>();
    exit_state->worker = session.worker;
    if (session.worker || hooks.exception)
        event_loop.set_exception_reporter([&](JS::Value value) {
            if (exit_state->requested_code.has_value() || vm->interrupt_requested())
                return;
            if (hooks.exception)
                hooks.exception(value);
            else
                report(value);
        });
    auto process_result = Mite::create_process(
        *vm, realm, exit_state, session.worker ? &session.environment : nullptr);
    if (process_result.is_error()) {
        report(process_result.error_value());
        vm->pop_execution_context();
        return 1;
    }

    GC::Root process { process_result.release_value() };
    global.define_direct_property("process"_utf16, process.ptr(), JS::default_attributes);
    GC::RootVector<JS::Value> args;
    args.append(process->get_without_side_effects("execPath"_utf16));
    auto append_text = [&](std::string const& argument) {
        return JS::PrimitiveString::create(*vm,
            Utf16String::from_utf8_with_replacement_character(
                StringView(argument.data(), argument.size())));
    };

    process->define_direct_property("argv0"_utf16,
        append_text(session.argv.empty() ? "mite" : session.argv[0]), JS::Attribute::Enumerable);
    for (size_t index = 1; index < session.argv.size(); ++index)
        args.append(append_text(session.argv[index]));
    process->define_direct_property(
        "argv"_utf16, JS::Array::create_from(realm, args), JS::default_attributes);
    GC::RootVector<JS::Value> exec_args;
    for (auto const& argument : session.exec_argv)
        exec_args.append(append_text(argument));
    process->define_direct_property(
        "execArgv"_utf16, JS::Array::create_from(realm, exec_args), JS::default_attributes);

    auto stack_support = install_error_stacks(realm);
    if (stack_support.is_error()) {
        report(stack_support.error_value());
        vm->pop_execution_context();
        return 1;
    }

    auto installed = event_loop.install(realm);
    if (installed.is_error()) {
        report(installed.error_value());
        vm->pop_execution_context();
        return 1;
    }

    // CLI captures immutable process trust before entering this reusable owner session.
    Mite::ModuleLoader modules(*vm, realm, event_loop, entry_path);
    bool host_active = true;
    auto stop_host = [&] {
        if (!host_active)
            return;
        host_active = false;
        if (hooks.shutdown)
            hooks.shutdown();
    };

    ScopeGuard stop_guard(stop_host);
    auto globals = modules.install_globals(hooks.allow_process_signals, fork_child);
    if (globals.is_error()) {
        report(globals.error_value());
        vm->pop_execution_context();
        return 1;
    }

    if (hooks.initialize) {
        auto result = hooks.initialize(*vm, realm, modules, event_loop);
        if (result.is_error()) {
            report(result.error_value());
            stop_host();
            if (event_loop.has_pending_work()) {
                // A partially initialized host can already own blocking work.
                fflush(nullptr);
                if (!session.worker)
                    std::_Exit(1);
            }

            vm->pop_execution_context();
            return 1;
        }
    }

    auto drive
        = [&] { return hooks.drive ? hooks.drive(event_loop) : event_loop.run_until_idle(); };
    auto module_format = filename == "[eval]" ? JS::ThrowCompletionOr<bool>(false)
                                              : modules.is_es_module(entry_path);
    int status = 0;
    if (module_format.is_error()) {
        report(module_format.error_value());
        status = 1;
    } else if (module_format.value()) {
        if (std::getenv("MITE_MODULE_DIAGNOSTIC"))
            fprintf(stderr, "MITE_ENTRY_PHASE pid=%d phase=esm-entry-begin\n",
                static_cast<int>(uv_os_getpid()));
        auto result = modules.run_entry();
        if (std::getenv("MITE_MODULE_DIAGNOSTIC"))
            fprintf(stderr, "MITE_ENTRY_PHASE pid=%d phase=esm-entry-submitted error=%d\n",
                static_cast<int>(uv_os_getpid()), result.is_error());
        if (result.is_error()) {
            event_loop.uncaught_exception(result.error_value(), true);
            status = event_loop.status() ? event_loop.status() : drive();
        } else {
            GC::Root completion { result.release_value() };
            event_loop.observe_entry_promise(*completion);
            status = drive();
            if (!hooks.exit_code.has_value() && !status
                && completion->state() == JS::Promise::State::Pending) {
                auto requested = process->get("exitCode"_utf16);
                if (requested.is_error()) {
                    report(requested.error_value());
                    status = 1;
                } else if (requested.value().is_int32()) {
                    status = requested.value().as_i32();
                } else {
                    fputs("Unsettled top-level await in entry module\n", stderr);
                    status = 13;
                }
            }
            // EventLoop owns entry rejection promotion and recovery. Reporting
            // the settled promise here would emit a second fatal error after a
            // real uncaughtException handler already accepted it.
        }
    } else if (filename != "[eval]") {
        auto result = modules.run_commonjs_entry();
        if (result.is_error()) {
            event_loop.uncaught_exception(result.error_value());
            status = event_loop.status() ? event_loop.status() : drive();
        } else {
            status = drive();
        }
    } else {
        auto result = modules.run_eval(source);
        if (result.is_error()) {
            event_loop.uncaught_exception(result.error_value());
            status = event_loop.status() ? event_loop.status() : drive();
        } else {
            // Print the actual script completion before driving asynchronous work.
            // This uses normal string conversion, including errors from user hooks.
            auto completion = result.release_value();
            if (print_completion) {
                auto text = completion.to_utf16_string(*vm);
                if (text.is_error()) {
                    event_loop.uncaught_exception(text.error_value());
                    status = event_loop.status();
                } else {
                    write_text(stdout, text.value());
                    fputc('\n', stdout);
                    fflush(stdout);
                }
            }

            if (!status)
                status = drive();
        }
    }

    diagnose_runtime_return(event_loop, status, session.worker);
    if (session.worker && exit_state->requested_code.has_value())
        status = *exit_state->requested_code;
    if (!hooks.exit_code.has_value() && (status == 0 || status == 13)) {
        auto code = process->get("exitCode"_utf16);
        if (code.is_error()) {
            report(code.error_value());
            status = 1;
        } else if (code.value().is_int32()) {
            status = code.value().as_i32();
        }
    }

    if (hooks.exit_code.has_value())
        status = *hooks.exit_code;
    bool pending_before_exit = event_loop.has_pending_work();
    auto exit_notice = (session.worker && vm->interrupt_requested())
        ? JS::ThrowCompletionOr<int>(exit_state->requested_code.value_or(status))
        : emit_process_exit(
              *vm, *process, *exit_state, status, status != 0 || hooks.exit_code.has_value());
    if (exit_notice.is_error()) {
        report(exit_notice.error_value());
        status = 1;
    } else {
        status = hooks.exit_code.has_value() ? *hooks.exit_code : exit_notice.value();
    }

    stop_host();
    if ((status || hooks.terminate_pending_work_on_stop || !pending_before_exit)
        && event_loop.has_pending_work()) {
        // Terminal callbacks cannot extend lifetime with new blocking work. Hosts
        // already stopping with pending work retain their explicit stop policy.
        // Fatal termination must not wait for a blocking filesystem worker (e.g.
        // a FIFO with no writer). The OS reclaims requests and handles at exit.
        fflush(nullptr);
        if (!session.worker)
            std::_Exit(status);
    }

    vm->pop_execution_context();
    return status;
}
