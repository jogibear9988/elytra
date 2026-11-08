/** Bounded native watchers; snapshots and rooted callbacks meet only on Mite's owner loop. */
#include "Host.h"
#include "core/EventLoop.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/Utf8View.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cmath>
#include <memory>
#include <unordered_map>

namespace Mite::Policy {
namespace {
    constexpr size_t watcher_limit = 16;
    constexpr u64 identity_limit = 2147483647;

    JS::Completion fail(JS::VM& vm, StringView code, StringView message)
    {
        auto error = JS::Error::create(*vm.current_realm(), Utf16String::from_utf8(message));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }

    Optional<Type> type(StringView value)
    {
        if (value == "string"sv)
            return Type::String;
        if (value == "number"sv)
            return Type::Number;
        if (value == "boolean"sv)
            return Type::Boolean;
        return {};
    }

    u8 bit(Type value)
    {
        return 1 << static_cast<u8>(value);
    }

    ErrorOr<Vector<Definition>> definitions(StringView text)
    {
        auto json = TRY(JsonValue::from_string(text));
        if (!json.is_object() || json.as_object().size() > definition_limit)
            return Error::from_string_literal("Invalid policy definitions object/count");
        Vector<Definition> result;
        ErrorOr<void> status;
        json.as_object().for_each_member([&](String const& name, JsonValue const& entry) {
            if (status.is_error())
                return;
            auto parse = [&]() -> ErrorOr<void> {
                if (name.is_empty() || name.byte_count() > name_limit || name.contains('\0')
                    || !Utf8View(name.bytes_as_string_view()).validate() || !entry.is_object()
                    || entry.as_object().size() != 1)
                    return Error::from_string_literal("Invalid policy name/definition");
                auto value = entry.as_object().get("type"sv);
                if (!value.has_value() || (!value->is_array() && !value->is_string()))
                    return Error::from_string_literal("Policy type must be a scalar or union");
                Definition definition { name.to_byte_string(), {}, value->is_array() };
                u8 seen = 0;
                auto append = [&](JsonValue const& item) -> ErrorOr<void> {
                    if (!item.is_string())
                        return Error::from_string_literal("Invalid policy type");
                    auto selected = type(item.as_string());
                    if (!selected.has_value() || (seen & bit(*selected)))
                        return Error::from_string_literal("Invalid/duplicate policy type");
                    seen |= bit(*selected);
                    return definition.types.try_append(*selected);
                };

                if (value->is_array()) {
                    if (value->as_array().size() < 1 || value->as_array().size() > 3)
                        return Error::from_string_literal("Invalid policy union size");
                    for (auto const& item : value->as_array().values())
                        TRY(append(item));
                } else {
                    TRY(append(*value));
                }

                return result.try_append(move(definition));
            };

            status = parse();
        });
        TRY(status);
        return result;
    }

    struct State;

    struct Watch {
        std::shared_ptr<State> owner;
        u64 id;
        OwnPtr<Provider> provider;
        HashMap<ByteString, u8> allowed;
        GC::Root<JS::FunctionObject> callback;
        uv_timer_t timer {};
    };

    ErrorOr<ByteString> encode(Watch const& watch, Snapshot const& snapshot)
    {
        if (snapshot.size() > definition_limit)
            return Error::from_string_literal("Policy provider exceeded snapshot count");
        JsonObject values;
        size_t budget = 0;
        for (auto const& entry : snapshot) {
            auto allowed = watch.allowed.get(entry.key);
            if (!allowed.has_value())
                return Error::from_string_literal("Policy provider returned undeclared key");
            budget += entry.key.length();
            if (entry.value.has<ByteString>()) {
                auto const& value = entry.value.get<ByteString>();
                if (!(*allowed & bit(Type::String)) || value.length() > value_limit)
                    return Error::from_string_literal("Policy provider returned invalid string");
                budget += value.length();
                values.set(entry.key, TRY(String::from_utf8(value.view())));
            } else if (entry.value.has<double>()) {
                auto value = entry.value.get<double>();
                if (!(*allowed & bit(Type::Number)) || !std::isfinite(value))
                    return Error::from_string_literal("Policy provider returned invalid number");
                values.set(entry.key, value);
                budget += sizeof(value);
            } else {
                if (!(*allowed & bit(Type::Boolean)))
                    return Error::from_string_literal("Policy provider returned invalid boolean");
                values.set(entry.key, entry.value.get<bool>());
                ++budget;
            }

            if (budget > snapshot_limit)
                return Error::from_string_literal("Policy provider exceeded snapshot bytes");
        }

        JsonObject result;
        result.set("values"sv, move(values));
        auto text = JsonValue(move(result)).serialized();
        if (text.byte_count() > snapshot_limit)
            return Error::from_string_literal("Encoded policy snapshot exceeds budget");
        return text.to_byte_string();
    }

    struct State {
        JS::Realm& realm;
        EventLoop& loop;
        ProviderFactory factory;
        std::unordered_map<u64, Watch*> active;
        u64 next_id { 1 };
        size_t handles {};
        bool stopped {};

        ~State()
        {
            VERIFY(active.empty() && !handles);
        }

        Watch* find(JS::Value value)
        {
            if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
                || value.as_double() > identity_limit
                || std::floor(value.as_double()) != value.as_double())
                return nullptr;
            auto found = active.find(static_cast<u64>(value.as_double()));
            return found == active.end() ? nullptr : found->second;
        }

        void close(Watch& watch)
        {
            active.erase(watch.id);
            // Revoke roots before stopping/draining the worker or entering any close callback.
            watch.callback = nullptr;
            watch.provider->stop();
            watch.provider = nullptr;
            uv_timer_stop(&watch.timer);
            uv_close(reinterpret_cast<uv_handle_t*>(&watch.timer), [](uv_handle_t* timer) {
                auto* watch = static_cast<Watch*>(timer->data);
                --watch->owner->handles;
                delete watch;
            });
        }

        void poll(Watch& watch)
        {
            if (stopped || !watch.callback)
                return;
            auto snapshot = watch.provider->take_snapshot();
            if (snapshot.is_error()) {
                close(watch);
                loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                    return fail(realm.vm(), "ERR_MITE_POLICY_PROVIDER"sv,
                        "Native policy snapshot failed; watcher retired"sv);
                });
                return;
            }

            if (!snapshot.value().has_value())
                return;
            auto text = encode(watch, *snapshot.value());
            if (text.is_error()) {
                close(watch);
                loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                    return fail(realm.vm(), "ERR_MITE_POLICY_DATA"sv,
                        "Native policy snapshot violated its contract; watcher retired"sv);
                });
                return;
            }

            GC::Root callback { watch.callback };
            auto payload = text.release_value();
            loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                return JS::call(realm.vm(), *callback, JS::js_undefined(),
                    JS::PrimitiveString::create(realm.vm(), Utf16String::from_utf8(payload)));
            });
        }
    };
}

GC::Ref<JS::Object> create_host(JS::Realm& realm, EventLoop& loop, ProviderFactory factory)
{
    if (!factory)
        factory = Provider::create;
    auto state = std::make_shared<State>(realm, loop, move(factory));
    loop.on_shutdown([state] {
        state->stopped = true;
        while (!state->active.empty())
            state->close(*state->active.begin()->second);
    });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "start"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string() || !vm.argument(1).is_string()
                || !vm.argument(2).is_function())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Invalid native policy arguments"sv);
            auto product_view = vm.argument(0).as_string().utf16_string_view();
            auto definition_view = vm.argument(1).as_string().utf16_string_view();
            if (product_view.is_empty() || product_view.length_in_code_units() > name_limit
                || definition_view.length_in_code_units() > snapshot_limit)
                return fail(vm, "ERR_MITE_POLICY_LIMIT"sv, "Policy arguments exceed budget"sv);
            auto product = product_view.to_utf8(AllowLonelySurrogates::No);
            auto text = definition_view.to_utf8(AllowLonelySurrogates::No);
            if (product.is_error() || text.is_error() || product.value().contains('\0')
                || product.value().byte_count() > name_limit
                || text.value().byte_count() > snapshot_limit)
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid native policy text"sv);
            auto parsed = definitions(text.value().bytes_as_string_view());
            if (parsed.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid native policy definitions"sv);
            if (state->stopped)
                return fail(vm, "ERR_MITE_POLICY_CLOSED"sv, "Policy host has stopped"sv);
            if (state->handles >= watcher_limit || state->next_id > identity_limit)
                return fail(vm, "ERR_MITE_POLICY_LIMIT"sv, "Policy watcher budget exhausted"sv);
            HashMap<ByteString, u8> allowed;
            for (auto const& entry : parsed.value()) {
                u8 bits = 0;
                for (auto type : entry.types)
                    bits |= bit(type);
                allowed.set(entry.name, bits);
            }

            auto provider
                = state->factory(product.release_value().to_byte_string(), parsed.release_value());
            if (provider.is_error()) {
                bool unsupported
                    = provider.error().is_errno() && provider.error().code() == ENOTSUP;
                return fail(vm,
                    unsupported ? "ERR_MITE_UNSUPPORTED_API"sv : "ERR_MITE_POLICY_PROVIDER"sv,
                    "Native policy provider could not start"sv);
            }

            auto* watch = new Watch { state, state->next_id++, provider.release_value(),
                move(allowed), GC::Root { vm.argument(2).as_function() } };
            int status = uv_timer_init(&state->loop.native_loop(), &watch->timer);
            if (status) {
                delete watch;
                return fail(
                    vm, "ERR_MITE_POLICY_PROVIDER"sv, "Cannot initialize policy owner timer"sv);
            }

            ++state->handles;
            watch->timer.data = watch;
            state->active.emplace(watch->id, watch);
            status = uv_timer_start(
                &watch->timer,
                [](uv_timer_t* timer) {
                    auto& watch = *static_cast<Watch*>(timer->data);
                    watch.owner->poll(watch);
                },
                0, 25);
            if (status) {
                state->close(*watch);
                return fail(
                    vm, "ERR_MITE_POLICY_PROVIDER"sv, "Cannot schedule policy snapshot delivery"sv);
            }

            return JS::Value(static_cast<double>(watch->id));
        },
        3, JS::default_attributes);
    host->define_native_function(
        realm, "stop"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* watch = state->find(vm.argument(0));
            if (!watch)
                return JS::Value(false);
            state->close(*watch);
            return JS::Value(true);
        },
        1, JS::default_attributes);
    return *host;
}
}
