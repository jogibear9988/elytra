/** Exercise actual composed LibURL origins and LibWebCommon trust decisions, without a GUI. */
#include "OriginPolicy.h"
#include <AK/Array.h>
#include <AK/Vector.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebCommon/SecureContexts/AbstractOperations.h>
#include <atomic>
#include <thread>

namespace {
using namespace Firefly::Protocols;

void check(bool condition, StringView label)
{
    if (!condition) {
        warnln("FAIL protocol origin: {}", label);
        exit(1);
    }
}

URL::URL parse(StringView text)
{
    auto result = URL::Parser::basic_parse(text);
    check(result.has_value(), "fixture URL parses"sv);
    return result.release_value();
}

bool trusted(URL::URL const& url)
{
    return Web::SecureContexts::is_url_potentially_trustworthy(url)
        == Web::SecureContexts::Trustworthiness::PotentiallyTrustworthy;
}

void baseline()
{
    check(parse("https://example.com/path"sv).origin().serialize() == "https://example.com"sv,
        "HTTPS origin"sv);
    check(trusted(parse("https://example.com"sv)), "HTTPS secure default"sv);
    check(!trusted(parse("http://example.com"sv)), "HTTP untrusted default"sv);
    check(trusted(parse("http://localhost"sv)), "loopback secure default"sv);
    check(
        parse("unregistered://host/path"sv).origin().is_opaque(), "unknown schemes stay opaque"sv);
    check(!trusted(parse("unregistered://host/path"sv)), "unknown schemes stay untrusted"sv);
    check(trusted(parse("file:///tmp/test.html"sv)), "upstream file trust preserved"sv);
}

void configured()
{
    for (auto name : { ""sv, "1app"sv, "App"sv, "app:"sv, "app_name"sv, "https"sv, "file"sv,
             "resource"sv, "blob"sv, "mite:internal"sv, "mite-host"sv }) {
        Array schemes { OriginScheme { MUST(String::from_utf8(name)), true, true } };
        check(install_origin_schemes(schemes).is_error(), "invalid/reserved names refused"sv);
    }

    Array duplicate { OriginScheme { "app"_string, true, true },
        OriginScheme { "app"_string, true, false } };
    check(install_origin_schemes(duplicate).is_error(), "duplicate refuses whole registration"sv);
    check(parse("app://bundle/a"sv).origin().is_opaque(), "refused batches preserve policy"sv);
    Vector<OriginScheme> too_many;
    for (int i = 0; i < 33; ++i)
        too_many.append({ MUST(String::formatted("scheme{}", i)), true, false });
    check(install_origin_schemes(too_many).is_error(), "bounded policy refuses excess entries"sv);
    Array schemes { OriginScheme { "app"_string, true, true },
        OriginScheme { "insecure-app"_string, true, false },
        OriginScheme { "opaque-app"_string, false, true } };
    MUST(install_origin_schemes(schemes));
    check(install_origin_schemes(schemes).is_error(), "registration immutable"sv);
    auto app = parse("app://BUNDLE/a?query#hash"sv);
    check(app.origin().serialize() == "app://bundle"sv, "normalized custom tuple origin"sv);
    check(app.origin().is_same_origin(parse("app://bundle/b"sv).origin()),
        "same host ignores path/query/fragment"sv);
    check(!app.origin().is_same_origin(parse("app://other/a"sv).origin()),
        "host origin separation"sv);
    check(!app.origin().is_same_origin(parse("insecure-app://bundle/a"sv).origin()),
        "scheme origin separation"sv);
    check(!app.origin().is_same_origin(parse("app://bundle:443/a"sv).origin()),
        "explicit custom port retained"sv);
    check(parse("app://bundle:0/a"sv).origin().serialize() == "app://bundle:0"sv,
        "zero port retained"sv);
    check(parse("app://user:pass@bundle/a"sv).origin().is_same_origin(app.origin()),
        "userinfo is not origin identity"sv);
    auto relative = app.complete_url("../script.mjs"sv);
    check(relative.has_value() && relative->origin().is_same_origin(app.origin()),
        "actual parser relative URL origin"sv);
    check(parse("app://bücher.example/a"sv).origin().serialize() == "app://xn--bcher-kva.example"sv,
        "real IDNA host normalization"sv);
    check(parse("app://[::1]:9000/a"sv).origin().serialize() == "app://[::1]:9000"sv,
        "IPv6 tuple origin"sv);
    check(trusted(app), "registered secure origin trust"sv);
    check(!trusted(parse("insecure-app://bundle/a"sv)), "standard is not secure"sv);
    check(parse("opaque-app://bundle/a"sv).origin().is_opaque(), "nonstandard remains opaque"sv);
    check(!trusted(parse("opaque-app://bundle/a"sv)), "secure flag cannot bless opaque origin"sv);
    check(parse("app:opaque"sv).origin().is_opaque() && !trusted(parse("app:opaque"sv)),
        "opaque-path form denied tuple/trust"sv);
    check(parse("app:///empty"sv).origin().is_opaque(), "empty host not granted tuple origin"sv);
    check(parse("app://host%2fother/a"sv).origin().is_opaque(),
        "invalid standard host not granted tuple origin"sv);
    baseline();
    std::atomic<bool> failed { false };
    auto reader = [&] {
        for (int i = 0; i < 500; ++i) {
            auto value = parse("app://bundle/a"sv);
            if (value.origin().serialize() != "app://bundle"sv || !trusted(value))
                failed = true;
        }
    };

    std::thread one(reader);
    std::thread two(reader);
    reader();
    one.join();
    two.join();
    check(!failed, "immutable concurrent origin/trust reads"sv);
    outln("PASS native custom tuple/IDNA/IPv6 origins, isolation, secure trust, atomic refusal and "
          "concurrent reads");
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (arguments.strings.size() != 2)
        return 2;
    baseline();
    if (arguments.strings[1] == "configured"sv) {
        configured();
    } else if (arguments.strings[1] == "default"sv) {
        check(parse("app://bundle/a"sv).origin().is_opaque(), "default private policy disabled"sv);
        outln("PASS default upstream origins/trust with no custom scheme grant");
    } else {
        return 2;
    }

    return 0;
}
