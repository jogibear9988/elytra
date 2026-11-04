#include "EventLoop.h"
#include "ModuleLoader.h"
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
    return JS::js_undefined();
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--version") {
        puts("mite 0.1.0-dev (LibJS)");
        return 0;
    }
    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
        puts("Usage: mite <script.js> [arguments...]\n       mite -e <source>\n       mite --version");
        return argc == 1 ? 1 : 0;
    }

    std::string filename;
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
        filename = std::filesystem::absolute(argv[1], error).string();
        if (error) {
            fprintf(stderr, "mite: cannot resolve script: %s\n", error.message().c_str());
            return 1;
        }
        bool is_file = std::filesystem::is_regular_file(filename, error);
        std::ifstream input;
        if (is_file && !error)
            input.open(filename, std::ios::binary);
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

    auto process = JS::Object::create(realm, realm.intrinsics().object_prototype());
    global.define_direct_property("process"_utf16, process, JS::default_attributes);
    GC::RootVector<JS::Value> args;
    args.append(JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(argv[0], strlen(argv[0])))));
    if (filename != "[eval]")
        args.append(JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(filename.data(), filename.size()))));
    for (int i = script_arg + 1; i < argc; ++i)
        args.append(JS::PrimitiveString::create(*vm, Utf16String::from_utf8_with_replacement_character(StringView(argv[i], strlen(argv[i])))));
    process->define_direct_property("argv"_utf16, JS::Array::create_from(realm, args), JS::default_attributes);

    Mite::EventLoop event_loop(*vm, report_exception);
    auto installed = event_loop.install(realm);
    if (installed.is_error()) {
        report_exception(installed.error_value());
        vm->pop_execution_context();
        return 1;
    }

    auto entry_path = filename == "[eval]" ? std::filesystem::current_path() / "[eval]" : std::filesystem::path(filename);
    Mite::ModuleLoader modules(*vm, realm, entry_path);
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
            status = event_loop.run_until_idle();
            if (!status && completion->state() == JS::Promise::State::Pending) {
                fputs("Unsettled top-level await in entry module\n", stderr);
                status = 13;
            } else if (!status && completion->state() == JS::Promise::State::Rejected) {
                report_exception(completion->result());
                status = 1;
            }
        }
    } else {
        auto text = Utf16String::from_utf8_with_replacement_character(StringView(source.data(), source.size()));
        auto parsed = JS::Script::parse(text, realm, StringView(filename.data(), filename.size()));
        if (parsed.is_error()) {
            for (auto const& error : parsed.error()) {
                fputs("SyntaxError: ", stderr);
                write_text(stderr, error.to_utf16_string());
                fputc('\n', stderr);
            }
            status = 1;
        } else {
            GC::Root script { parsed.release_value() };
            modules.register_script(*script);
            auto result = vm->run(*script);
            if (result.is_error()) {
                report_exception(result.error_value());
                status = 1;
            }
            if (!status)
                status = event_loop.run_until_idle();
        }
    }
    vm->pop_execution_context();
    return status;
}
