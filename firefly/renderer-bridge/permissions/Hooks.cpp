/** Check every permission read before upstream caches; requests have a separate native decision
 * path. */
#include "Hooks.h"
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWebCommon/PermissionsPolicy/PermissionsPolicy.h>
#include <atomic>

namespace Firefly::RendererPermissions {
namespace {
    std::atomic<bool> enabled {};
    std::atomic<u64> snapshot {};
    std::atomic<Query> receiver {};

    bool guarded(Web::PermissionsAPI::PermissionDescriptor const& descriptor,
        Web::HTML::EnvironmentSettingsObject& settings)
    {
        if (!Web::HTML::is_secure_context(settings))
            return false;
        auto* window = Web::HTML::window_from_global_object(settings.global_object());
        if (!window || !window->associated_document().is_fully_active())
            return false;
        Optional<Web::PermissionsPolicy::PolicyControlledFeature> feature;
        if (descriptor.name == "geolocation"_utf16)
            feature = Web::PermissionsPolicy::PolicyControlledFeature::Geolocation;
        else if (descriptor.name == "microphone"_utf16)
            feature = Web::PermissionsPolicy::PolicyControlledFeature::Microphone;
        else if (descriptor.name != "notifications"_utf16)
            return false;
        return !feature.has_value()
            || window->associated_document().is_allowed_to_use_feature(*feature);
    }
}

void enable()
{
    enabled.store(true, std::memory_order_release);
}

void set_query(Query value)
{
    receiver.store(value, std::memory_order_release);
}

bool active()
{
    return enabled.load(std::memory_order_acquire);
}

Policy policy()
{
    auto value = snapshot.load(std::memory_order_acquire);
    return { value & 0x7fffffff, static_cast<bool>(value & (1ull << 32)),
        static_cast<bool>(value & (1ull << 33)) };
}

bool apply_policy(u64 epoch, bool check, bool request)
{
    if (!epoch || epoch > 2147483647)
        return false;
    u64 next = epoch | (check ? 1ull << 32 : 0) | (request ? 1ull << 33 : 0);
    auto previous = snapshot.load(std::memory_order_acquire);
    for (;;) {
        auto old_epoch = previous & 0x7fffffff;
        if (epoch < old_epoch || (epoch == old_epoch && previous != next))
            return false;
        if (snapshot.compare_exchange_weak(previous, next, std::memory_order_acq_rel))
            return true;
    }
}

Optional<Web::PermissionsAPI::PermissionState> query(
    Web::PermissionsAPI::PermissionDescriptor const& descriptor,
    Web::HTML::EnvironmentSettingsObject& settings)
{
    using State = Web::PermissionsAPI::PermissionState;
    if (!active())
        return {};
    auto current = policy();
    auto callback = receiver.load(std::memory_order_acquire);
    if (!guarded(descriptor, settings) || !current.epoch || !current.check_enabled || !callback)
        return State::Denied;
    auto* window = Web::HTML::window_from_global_object(settings.global_object());
    auto name = descriptor.name.utf16_view().to_utf8(AllowLonelySurrogates::No);
    if (name.is_error() || name.value().byte_count() > 64)
        return State::Denied;
    bool granted = callback(
        window->associated_document(), current.epoch, name.value().bytes_as_string_view());
    // A policy update processed during synchronous transport invalidates its old answer.
    if (policy().epoch != current.epoch)
        return State::Denied;
    return granted ? State::Granted : State::Denied;
}

Web::PermissionsAPI::PermissionState request_state(
    Web::PermissionsAPI::PermissionDescriptor const& descriptor,
    Web::HTML::EnvironmentSettingsObject& settings)
{
    if (active() && policy().request_enabled && guarded(descriptor, settings))
        return Web::PermissionsAPI::PermissionState::Prompt;
    return Web::PermissionsAPI::permission_state(descriptor, settings);
}
}
