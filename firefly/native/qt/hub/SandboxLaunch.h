/** Deferred trusted launch options, derived from the actual upstream defaults and verified after
 * initialization. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Vector.h>
#include <LibWebView/Options.h>

namespace Firefly {
class SandboxLaunch {
public:
    SandboxLaunch() = default;

    explicit SandboxLaunch(WebView::DisableSandbox requested)
    {
        m_requested.disable_sandbox = requested;
    }

    bool enabled() const
    {
        return m_requested.disable_sandbox == WebView::DisableSandbox::No;
    }

    void append_arguments(Vector<ByteString>& arguments) const
    {
        // There is no upstream force-enable flag. No flag selects its genuine
        // default; the trusted disabled state must emit the actual disable flag.
        if (!enabled())
            arguments.append("--disable-sandbox"sv);
    }

    bool matches(WebView::BrowserOptions const& actual) const
    {
        return m_requested.disable_sandbox == actual.disable_sandbox;
    }

private:
    WebView::BrowserOptions m_requested;
};
}
