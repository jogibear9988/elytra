#!/usr/bin/env python3
"""Compose pinned file-script and native protocol fetch conditions without editing Ladybird's fetch implementation."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "d819c6ecf14e5ef6daedae9a1f0629cc01a3573f34b3253f7bf0a9df4b04c5e7"
SEAM = "            || is_file_to_file_font_load\n"
HOOK = "            || Firefly::FileModules::allows(*request)\n"
PREFIX = '#include "loading/FileModules.h"\n#include "loading/protocol/Dispatch.h"\n#include "native/protocol/OriginPolicy.h"\n#include "loading/web-request/Gate.h"\n'
PROTOCOL_SEAM = "    // 3. Switch on request’s current URL’s scheme and run the associated steps:\n"
PROTOCOL_HOOK = "    if (auto response = Firefly::ProtocolFetch::dispatch(fetch_params))\n        return *response;\n\n"

CORS_SEAM = (
    "        if (!Infrastructure::is_http_or_https_scheme(request->current_url().scheme())\n"
)
CORS_HOOK = "            && !Firefly::Protocols::is_cors_scheme(request->current_url().scheme())\n"
NETWORK_SEAM = "GC::Ref<PendingResponse> http_network_or_cache_fetch(JS::Realm& realm, Infrastructure::FetchParams const& fetch_params, IsAuthenticationFetch is_authentication_fetch, IsNewConnectionFetch is_new_connection_fetch)\n{\n"
NETWORK_HOOK = "    if (auto response = Firefly::ProtocolFetch::dispatch(fetch_params))\n        return *response;\n\n"

FILE_INTERCEPT_SEAM = '        // -> request’s current URL’s origin is same origin with request’s origin, and request’s response tainting is "basic"\n'
FILE_INTERCEPT_HOOK = "        if (auto intercepted = Firefly::ProtocolFetch::dispatch_file_interception(*fetch_params))\n            return *intercepted;\n\n"


BEFORE_SEAM = "    if (recursive == Recursive::Yes) {\n        // 12. If response is null, then set response to the result of running the steps corresponding to the first\n"
BEFORE_HOOK = """    auto firefly_upstream_get_response = get_response;
    get_response = GC::create_function<GC::Ref<PendingResponse>()>(GC::Heap::the(), GC::captures(realm, NonnullRefPtr { fetch_params }, firefly_upstream_get_response), [](auto& realm, auto& fetch_params, auto& firefly_upstream_get_response) -> GC::Ref<PendingResponse> {
        return Firefly::WebRequestGate::before(realm, *fetch_params, firefly_upstream_get_response);
    });

"""
HEAD_START = "        response->set_url_list(http_request->url_list());\n\n"
HEAD_OPEN = """        auto firefly_continue_response = GC::create_function<void(NonnullRefPtr<Infrastructure::Response>)>(GC::Heap::the(), GC::captures(realm, fetch_params, http_fetch_params, http_request, returned_pending_response, is_authentication_fetch, is_new_connection_fetch, include_credentials), [](auto& realm, auto& fetch_params, auto& http_fetch_params, auto& http_request, auto& returned_pending_response, auto& is_authentication_fetch, auto& is_new_connection_fetch, auto& include_credentials, NonnullRefPtr<Infrastructure::Response> response) {
"""
HEAD_END = "        process_http_network_or_cache_response(realm.global_object(), *fetch_params, *http_request, *response, returned_pending_response, is_authentication_fetch, is_new_connection_fetch, include_credentials);\n    }));\n\n    // 18. Return response."
HEAD_CLOSE = """        });
        NonnullRefPtr firefly_head_response { *response };
        if (!Firefly::WebRequestGate::headers(*fetch_params, firefly_head_response, firefly_continue_response))
            firefly_continue_response->function()(move(firefly_head_response));
"""
FINISH_SEAM = "    auto process_response_end_of_body = GC::create_function(GC::Heap::the(), GC::captures(NonnullRefPtr { fetch_params }, controller, response_view, setup_report_timing_steps, report_timing_if_appropriate), [](auto& fetch_params, auto& controller, auto& response_view, auto& setup_report_timing_steps, auto& report_timing_if_appropriate) {\n"
FINISH_HOOK = "        Firefly::WebRequestGate::finish(*fetch_params);\n"
WEB_SEAMS = (BEFORE_SEAM, HEAD_START, HEAD_END, FINISH_SEAM)


def adapt(source):
    if any(
        source.count(seam) != 1
        for seam in (SEAM, PROTOCOL_SEAM, CORS_SEAM, NETWORK_SEAM, FILE_INTERCEPT_SEAM, *WEB_SEAMS)
    ):
        raise ValueError("Review the file module fetch condition")
    source = (
        source.replace(BEFORE_SEAM, BEFORE_HOOK + BEFORE_SEAM)
        .replace(HEAD_START, HEAD_START + HEAD_OPEN)
        .replace(HEAD_END, HEAD_END.replace("\n    }));", "\n" + HEAD_CLOSE + "    }));", 1))
        .replace(FINISH_SEAM, FINISH_SEAM + FINISH_HOOK)
    )
    return PREFIX + source.replace(SEAM, SEAM + HOOK).replace(
        PROTOCOL_SEAM, PROTOCOL_HOOK + PROTOCOL_SEAM
    ).replace(CORS_SEAM, CORS_SEAM + CORS_HOOK).replace(
        NETWORK_SEAM, NETWORK_SEAM + NETWORK_HOOK
    ).replace(FILE_INTERCEPT_SEAM, FILE_INTERCEPT_HOOK + FILE_INTERCEPT_SEAM)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Libraries/LibWeb/Fetch/Fetching/Fetching.cpp"
    if hashlib.sha256(source.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned file module adapter before updating Ladybird")
    original = source.read_text()
    output = adapt(original)
    assert (
        output.removeprefix(PREFIX)
        .replace(SEAM + HOOK, SEAM)
        .replace(PROTOCOL_HOOK + PROTOCOL_SEAM, PROTOCOL_SEAM)
        .replace(CORS_SEAM + CORS_HOOK, CORS_SEAM)
        .replace(NETWORK_SEAM + NETWORK_HOOK, NETWORK_SEAM)
        .replace(FILE_INTERCEPT_HOOK + FILE_INTERCEPT_SEAM, FILE_INTERCEPT_SEAM)
        .replace(BEFORE_HOOK + BEFORE_SEAM, BEFORE_SEAM)
        .replace(HEAD_START + HEAD_OPEN, HEAD_START)
        .replace(HEAD_END.replace("\n    }));", "\n" + HEAD_CLOSE + "    }));", 1), HEAD_END)
        .replace(FINISH_SEAM + FINISH_HOOK, FINISH_SEAM)
        == original
    )
    for invalid in [
        "",
        original + SEAM,
        original.replace(SEAM, ""),
        original + PROTOCOL_SEAM,
        original.replace(PROTOCOL_SEAM, ""),
        original + CORS_SEAM,
        original.replace(CORS_SEAM, ""),
        original + NETWORK_SEAM,
        original.replace(NETWORK_SEAM, ""),
        original + FILE_INTERCEPT_SEAM,
        original.replace(FILE_INTERCEPT_SEAM, ""),
        *(original + seam for seam in WEB_SEAMS),
        *(original.replace(seam, "") for seam in WEB_SEAMS),
    ]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/ambiguous fetch seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print(
        "PASS file module/protocol fetch seams, source fingerprint, exact byte preservation and drift rejection"
    )


if __name__ == "__main__":
    main()
