/** Strict synchronous native dispatch; no OS provider or assertion is created by import. */
#include "Host.h"
#include "Provider.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <cmath>

namespace Firefly::PowerSaveBlocker {
struct Controller::State {
    std::unique_ptr<Provider> provider;
    bool stopped { false };

    void stop()
    {
        if (stopped)
            return;
        stopped = true;
        if (provider)
            provider->shutdown();
    }
};

namespace {
    JS::Completion fail(JS::VM& vm, std::string const& code, std::string const& message)
    {
        GC::Root error { JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message.data(), message.size()))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(code.data(), code.size()))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<int> request_id(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number() || !std::isfinite(value.as_double())
            || std::floor(value.as_double()) != value.as_double()
            || (value.as_double() == 0 && std::signbit(value.as_double())))
            return fail(vm, "ERR_INVALID_ARG_TYPE", "Power save request ID must be an integer");
        if (value.as_double() < -2147483648.0 || value.as_double() > 2147483647.0)
            return fail(vm, "ERR_OUT_OF_RANGE", "Power save request ID must fit signed INT32");
        return static_cast<int>(value.as_double());
    }
}

Controller::Controller(std::shared_ptr<State> state)
    : m_state(std::move(state))
{
}

Controller::~Controller()
{
    stop();
}

void Controller::stop()
{
    m_state->stop();
}

std::shared_ptr<Controller> install_host(JS::Realm& realm, JS::Object& host)
{
    auto state = std::make_shared<Controller::State>();
    host.define_native_function(
        realm, "powerSaveBlockerStart"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (state->stopped)
                return fail(vm, "ERR_FIREFLY_APP_EXITED", "Power save service stopped");
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE", "Expected a power save blocker type");
            auto type = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            if (type != "prevent-app-suspension"sv && type != "prevent-display-sleep"sv)
                return fail(vm, "ERR_INVALID_ARG_VALUE", "Unknown power save blocker type");
            if (!state->provider)
                state->provider = Provider::create();
            if (!state->provider)
                return fail(vm, "ERR_FIREFLY_POWER_SAVE_UNAVAILABLE",
                    "Native sleep inhibition unavailable");
            auto result = state->provider->start(
                type == "prevent-display-sleep"sv ? Type::DisplaySleep : Type::AppSuspension);
            if (!result.code.empty())
                return fail(vm, result.code, result.message);
            if (result.id < 0) {
                // A corrupt native admission cannot leave requests running without
                // a usable JS handle. Revoke this service before releasing ownership.
                state->stop();
                return fail(
                    vm, "ERR_FIREFLY_POWER_SAVE_DATA", "Invalid native power save request ID");
            }

            return JS::Value(result.id);
        },
        1, JS::default_attributes);
    for (auto name : { "powerSaveBlockerStop"_utf16, "powerSaveBlockerIsStarted"_utf16 }) {
        bool remove = name == "powerSaveBlockerStop"_utf16;
        host.define_native_function(
            realm, name,
            [state, remove](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (state->stopped)
                    return fail(vm, "ERR_FIREFLY_APP_EXITED", "Power save service stopped");
                auto id = TRY(request_id(vm));
                // Unknown IDs do not construct a provider or touch the OS.
                if (!state->provider || id < 0)
                    return JS::Value(false);
                auto result = remove ? state->provider->stop(id) : state->provider->is_started(id);
                if (!result.code.empty())
                    return fail(vm, result.code, result.message);
                return JS::Value(result.flag);
            },
            1, JS::default_attributes);
    }

    host.define_native_function(
        realm, "powerSaveBlockerShutdown"_utf16,
        [state](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            state->stop();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return std::make_shared<Controller>(state);
}
}
