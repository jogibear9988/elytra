/** Real deferred launch-plan/policy code, without creating Hub, Qt or any renderer process. */
#include "../../native/qt/hub/SandboxLaunch.h"
#include "../../native/sandbox/policy/Policy.h"
#include <cstdio>

int main()
{
    auto check = [](bool condition, char const* message) {
        if (!condition)
            std::fprintf(stderr, "Deferred sandbox policy: %s\n", message);
        return condition;
    };

    WebView::BrowserOptions actual;
    Firefly::SandboxLaunch selected;
    if (!check(selected.matches(actual), "Selected state must originate in actual SDK defaults")
        || !check(selected.enabled() == (actual.disable_sandbox == WebView::DisableSandbox::No),
            "Native enabled state differs from the SDK"))
        return 1;
    Vector<ByteString> arguments;
    selected.append_arguments(arguments);
    if (!check(arguments.size() == (selected.enabled() ? 0u : 1u),
            "Selected policy is not represented in actual native launch flags"))
        return 1;

    Firefly::SandboxLaunch enabled(WebView::DisableSandbox::No);
    Firefly::SandboxLaunch disabled(WebView::DisableSandbox::Yes);
    Firefly::SandboxPolicy::Policy admitted;
    if (!check(!admitted.enable(enabled.enabled()).has_value(),
            "Actual enabled plan must latch before any Hub exists")
        || !check(admitted.required(), "Enable cannot be accepted inertly"))
        return 1;
    Firefly::SandboxPolicy::Policy rejected;
    if (!check(rejected.enable(disabled.enabled()).has_value(),
            "Disabled native plan must reject the pre-ready request")
        || !check(!rejected.required(), "Failed admission must not latch"))
        return 1;
    Vector<ByteString> disabled_arguments;
    disabled.append_arguments(disabled_arguments);
    if (!check(disabled_arguments.size() == 1 && disabled_arguments[0] == "--disable-sandbox"sv,
            "Disabled plan must emit the upstream flag"))
        return 1;
    actual.disable_sandbox = WebView::DisableSandbox::No;
    if (!check(enabled.matches(actual) && !disabled.matches(actual),
            "Observed native configuration must match the selected state"))
        return 1;
    actual.disable_sandbox = WebView::DisableSandbox::Yes;
    if (!check(!enabled.matches(actual) && disabled.matches(actual),
            "Startup must detect a native disabled-policy mismatch"))
        return 1;
    admitted.ready();
    if (!check(!admitted.check_window(enabled.enabled()).has_value(),
            "Enabled observed launch must admit a ready window")
        || !check(admitted.check_window(disabled.enabled()).has_value(),
            "Actual native disabled state must deny window creation despite prior enable"))
        return 1;
    std::puts("PASS genuine deferred sandbox launch flags latch and actual-native mismatch guards "
              "without Hub");
    return 0;
}
