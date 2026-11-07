/** Verify real engine/UI URL classification without opening a window or external application. */
#include "native/protocol/OriginPolicy.h"
#include <AK/Array.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebCommon/Fetch/Infrastructure/URL.h>
#include <LibWebView/URL.h>

namespace {
ErrorOr<void> check(StringView text, bool fetch, WebView::UserInputClassification kind)
{
    auto url = URL::Parser::basic_parse(text);
    if (!url.has_value())
        return Error::from_string_literal("Routing fixture did not parse");
    auto classified = WebView::classify_user_input(text);
    if (Web::Fetch::Infrastructure::is_fetch_scheme(url->scheme()) != fetch
        || WebView::is_url_handled_internally(*url) != fetch || classified.classification != kind)
        return Error::from_string_literal("Native UI and engine fetch classification disagree");
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (arguments.argc != 1)
        return Error::from_string_literal("Routing probe takes no arguments");
    using Kind = WebView::UserInputClassification;
    TRY(check("app://bundle/index.html"sv, false, Kind::ExternalURL));
    TRY(check("insecure-app://bundle/index.html"sv, false, Kind::ExternalURL));
    Array schemes { Firefly::Protocols::OriginScheme { "app"_string, true, true },
        Firefly::Protocols::OriginScheme { "insecure-app"_string, true, false },
        Firefly::Protocols::OriginScheme { "opaque-app"_string, false, true } };
    TRY(Firefly::Protocols::install_origin_schemes(schemes));
    TRY(check("app://bundle/index.html"sv, true, Kind::InternalURL));
    TRY(check("app://bundle/index.html?query#fragment"sv, true, Kind::InternalURL));
    TRY(check("insecure-app://bundle/index.html"sv, true, Kind::InternalURL));
    TRY(check("opaque-app://bundle/index.html"sv, false, Kind::ExternalURL));
    TRY(check("unregistered://bundle/index.html"sv, false, Kind::ExternalURL));
    TRY(check("https://example.com/index.html"sv, true, Kind::InternalURL));
    TRY(check("http://example.com/index.html"sv, true, Kind::InternalURL));
    TRY(check("file:///tmp/index.html"sv, true, Kind::InternalURL));
    TRY(check("data:text/plain,hello"sv, true, Kind::InternalURL));
    TRY(check("mailto:example@example.com"sv, false, Kind::ExternalURL));
    outln("PASS actual LibWebCommon/LibWebView registered-standard fetch and native UI routing; no "
          "navigation or handler grant");
    return 0;
}
