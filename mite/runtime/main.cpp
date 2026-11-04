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

    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> jobs;
    GC::RootVector<GC::Ref<JS::Promise>> rejections;
    vm->host_enqueue_promise_job = [&](JS::PromiseJob job, GC::Ptr<JS::Realm> job_realm) {
        jobs.append(GC::create_function(vm->heap(), [job, job_realm] {
            (void)job_realm; // Keep the job's realm alive until dispatch.
            return job.run();
        }));
    };
    vm->host_promise_job_queue_is_empty = [&] { return jobs.is_empty(); };
    vm->host_promise_rejection_tracker = [&](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
        if (operation == JS::Promise::RejectionOperation::Reject)
            rejections.append(promise);
        else
            rejections.remove_all_matching([&](auto const& entry) { return entry.ptr() == &promise; });
    };

    auto text = Utf16String::from_utf8_with_replacement_character(StringView(source.data(), source.size()));
    auto parsed = JS::Script::parse(text, realm, StringView(filename.data(), filename.size()));
    int status = 0;
    if (parsed.is_error()) {
        for (auto const& error : parsed.error()) {
            fputs("SyntaxError: ", stderr);
            write_text(stderr, error.to_utf16_string());
            fputc('\n', stderr);
        }
        status = 1;
    } else {
        GC::Root script { parsed.release_value() };
        auto result = vm->run(*script);
        if (result.is_error()) {
            report_exception(result.error_value());
            status = 1;
        }
        while (status == 0 && !jobs.is_empty()) {
            GC::Root job { jobs.take_first() };
            auto result = job->function()();
            if (result.is_error()) {
                report_exception(result.error_value());
                status = 1;
            }
        }
        if (status == 0 && !rejections.is_empty()) {
            report_exception(rejections.first()->result());
            status = 1;
        }
    }
    vm->pop_execution_context();
    return status;
}
