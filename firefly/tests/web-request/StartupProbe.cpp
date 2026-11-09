/** Invoke the exact production startup parser and policy state without VM, Qt or OS changes. */
#include "loading/web-request/Gate.h"
#include "renderer-helper/web-request/Session.h"
#include <AK/Array.h>
#include <cstdio>

int main()
{
    using Firefly::HelperWebRequest::prepare;
    auto check = [](bool value, char const* message) {
        if (!value)
            std::fprintf(stderr, "FAIL webRequest startup: %s\n", message);
        return value;
    };

    auto before = Firefly::WebRequestGate::policy();
    if (!check(!before.epoch && !before.before && !before.headers, "fresh process defaults"))
        return 1;
    Array absent { "WebContent"sv, "--other"sv };
    auto normal = prepare(absent);
    if (!check(!normal.is_error() && normal.value().strings.size() == 2
                && !Firefly::WebRequestGate::policy().epoch,
            "unconfigured launch preserves arguments"))
        return 1;
    Array valid { "WebContent"sv, "--firefly-web-request-policy=1:1:0"sv, "tail"sv };
    auto accepted = prepare(valid);
    if (!check(!accepted.is_error() && accepted.value().strings.size() == 2
                && accepted.value().strings[1] == "tail"sv,
            "valid policy removed from argv"))
        return 1;
    auto current = Firefly::WebRequestGate::policy();
    if (!check(current.epoch == 1 && current.before && !current.headers,
            "startup policy enforced before VM"))
        return 1;
    for (auto invalid :
        { "--firefly-web-request-policy=0:1:1"sv, "--firefly-web-request-policy=01:1:1"sv,
            "--firefly-web-request-policy=2147483648:1:1"sv,
            "--firefly-web-request-policy=2:true:0"sv, "--firefly-web-request-policy=2:1:2"sv,
            "--firefly-web-request-policy=2:1"sv, "--firefly-web-request-policy"sv,
            "--firefly-web-request-policy=1:0:0"sv }) {
        Array arguments { "WebContent"sv, invalid };
        if (!check(prepare(arguments).is_error() && Firefly::WebRequestGate::policy().epoch == 1
                    && Firefly::WebRequestGate::policy().before,
                "malformed/conflicting policy cannot alter authority"))
            return 1;
    }

    Array duplicate { "WebContent"sv, "--firefly-web-request-policy=2:0:1"sv,
        "--firefly-web-request-policy=3:1:1"sv };
    if (!check(prepare(duplicate).is_error() && Firefly::WebRequestGate::policy().epoch == 1,
            "duplicate rejected before policy mutation"))
        return 1;
    Array positional { "WebContent"sv, "--"sv, "--firefly-web-request-policy=9:1:1"sv };
    auto tail = prepare(positional);
    if (!check(!tail.is_error() && tail.value().strings.size() == 3
                && Firefly::WebRequestGate::policy().epoch == 1,
            "literal positional argument preserved"))
        return 1;
    Array next { "WebContent"sv, "--firefly-web-request-policy=2:0:1"sv };
    if (!check(!prepare(next).is_error() && Firefly::WebRequestGate::policy().epoch == 2
                && Firefly::WebRequestGate::policy().headers,
            "newer snapshot accepted"))
        return 1;
    if (!check(prepare(valid).is_error() && Firefly::WebRequestGate::policy().epoch == 2,
            "rollback refused"))
        return 1;
    std::puts("PASS native webRequest startup parser and pre-VM policy");
    return 0;
}
