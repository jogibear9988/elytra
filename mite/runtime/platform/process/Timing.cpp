/** Monotonic nanoseconds and process CPU accounting; wall-clock adjustments cannot change them. */
#include "Timing.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <chrono>
#include <cstring>
#include <uv.h>

namespace Mite {
namespace {
    // Initialized once when the runtime is loaded, not once per VM/realm. Avoid
    // invoking libuv before its platform initialization during static startup.
    auto const process_start = std::chrono::steady_clock::now();
    // Pair the runtime's initial monotonic sample with one native epoch sample.
    // Only the origin uses wall time; later clock reads never consult Date or OS wall time.
    auto const process_epoch = std::chrono::system_clock::now();

    JS::Completion cpu_error(JS::VM& vm, int status)
    {
        GC::Root error { JS::Error::create(vm.realm(),
            Utf16String::from_utf8(
                StringView(uv_strerror(status), std::strlen(uv_strerror(status))))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm,
                Utf16String::from_utf8(
                    StringView(uv_err_name(status), std::strlen(uv_err_name(status))))),
            JS::default_attributes);
        error->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);
        error->define_direct_property("syscall"_utf16,
            JS::PrimitiveString::create(vm, "uv_getrusage"_utf16), JS::default_attributes);
        return JS::throw_completion(error);
    }
}

void install_performance_timing(JS::Realm& realm, JS::Object& host)
{
    host.define_direct_property("timeOrigin"_utf16,
        JS::Value(
            std::chrono::duration<double, std::milli>(process_epoch.time_since_epoch()).count()),
        JS::default_attributes);
    host.define_native_function(
        realm, "now"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - process_start)
                    .count());
        },
        0, JS::default_attributes);
}

void install_process_timing(JS::Realm& realm, JS::Object& host)
{
    host.define_native_function(
        realm, "monotonicTime"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto nanos = uv_hrtime();
            // Quotient/remainder stay exact in Number; never round the u64 total
            // through a double before JS reconstructs the BigInt result.
            JS::Value pair[] { JS::Value(static_cast<double>(nanos / 1000000000)),
                JS::Value(static_cast<double>(nanos % 1000000000)) };
            return JS::Array::create_from(vm.realm(), pair);
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "processUptime"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(
                std::chrono::duration<double>(std::chrono::steady_clock::now() - process_start)
                    .count());
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "cpuTime"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            uv_rusage_t usage {};
            if (auto status = uv_getrusage(&usage))
                return cpu_error(vm, status);
            auto micros = [](uv_timeval_t const& value) {
                return static_cast<double>(value.tv_sec) * 1000000.0 + value.tv_usec;
            };

            JS::Value pair[] { JS::Value(micros(usage.ru_utime)),
                JS::Value(micros(usage.ru_stime)) };
            return JS::Array::create_from(vm.realm(), pair);
        },
        0, JS::default_attributes);
}
}
