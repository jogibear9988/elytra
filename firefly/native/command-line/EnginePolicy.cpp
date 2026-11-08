/** Apply validated language/CPU painting before readiness/helper creation. */
#include "EnginePolicy.h"
#include <LibUnicode/Locale.h>
#include <LibWebView/Application.h>
#include <LibWebView/Settings.h>

namespace Firefly::CommandLine {
EnginePolicy::EnginePolicy(bool startup_cpu)
    : m_startup_cpu(startup_cpu)
{
}

void EnginePolicy::activate()
{
    m_languages = WebView::Application::settings().languages();
    m_active = true;
    if (m_requested_language.has_value())
        WebView::Application::settings().set_languages({ *m_requested_language });
}

std::optional<Failure> EnginePolicy::apply(
    std::string_view name, std::optional<std::string> const& value, bool initial)
{
    if (name == "force-cpu-painting") {
        if (value.has_value() && !value->empty())
            return Failure { "ERR_INVALID_ARG_VALUE", "force-cpu-painting does not take a value" };
        // CPU mode remains a launch-only policy; deferred profile initialization
        // consumes this validated setting before starting the compositor.
        if (!initial)
            return Failure { "ERR_FIREFLY_COMMAND_LINE_TOO_LATE",
                "CPU painting must be selected in launch arguments" };

        if (!m_startup_cpu)
            return Failure { "ERR_FIREFLY_UNSUPPORTED_API",
                "CPU painting was not applied at native startup" };

        return {};
    }

    if (name == "lang") {
        if (!value.has_value()) {
            m_requested_language.clear();
            if (m_active)
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

        m_requested_language = language;
        if (m_active)
            WebView::Application::settings().set_languages({ std::move(language) });
        return {};
    }

    return Failure { "ERR_FIREFLY_UNSUPPORTED_API", "No native engine policy for this switch" };
}
}
