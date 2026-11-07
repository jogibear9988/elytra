/** Apply validated language/CPU painting before readiness/helper creation. */
#include "EnginePolicy.h"
#include <LibUnicode/Locale.h>
#include <LibWebView/Application.h>
#include <LibWebView/Settings.h>

namespace Firefly::CommandLine {
EnginePolicy::EnginePolicy()
    : m_languages(WebView::Application::settings().languages())
{
}

std::optional<Failure> EnginePolicy::apply(
    std::string_view name, std::optional<std::string> const& value, bool initial)
{
    if (name == "force-cpu-painting") {
        if (value.has_value() && !value->empty())
            return Failure { "ERR_INVALID_ARG_VALUE", "force-cpu-painting does not take a value" };
        // The compositor exists before the script starts. Its CPU mode must
        // be selected in native launch arguments, not an ineffective late option.
        if (!initial)
            return Failure { "ERR_FIREFLY_COMMAND_LINE_TOO_LATE",
                "CPU painting must be selected in launch arguments" };

        if (WebView::Application::web_content_options().force_cpu_painting
            != WebView::ForceCPUPainting::Yes)
            return Failure { "ERR_FIREFLY_UNSUPPORTED_API",
                "CPU painting was not applied at native startup" };

        return {};
    }

    if (name == "lang") {
        if (!value.has_value()) {
            WebView::Application::settings().set_languages(m_languages);
            return {};
        }

        if (value->empty() || value->size() > 128)
            return Failure { "ERR_INVALID_ARG_VALUE",
                "lang requires a supported BCP 47 language tag" };

        auto canonical
            = Unicode::canonicalize_unicode_locale_id(StringView(value->data(), value->size()));
        if (!canonical.has_value())
            return Failure { "ERR_INVALID_ARG_VALUE",
                "lang requires a supported BCP 47 language tag" };

        auto language = canonical->to_utf8();
        if (!Unicode::is_locale_available(language))
            return Failure { "ERR_INVALID_ARG_VALUE",
                "Requested language is unavailable in this build" };

        WebView::Application::settings().set_languages({ std::move(language) });
        return {};
    }

    return Failure { "ERR_FIREFLY_UNSUPPORTED_API", "No native engine policy for this switch" };
}
}
