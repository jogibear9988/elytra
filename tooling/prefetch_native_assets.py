#!/usr/bin/env python3
"""Preseed one hash-pinned vcpkg PCRE2 gitlink response without editing dependencies.

The credential is read only on a cache miss, only from the dedicated environment
variable, and sent only to the exact HTTPS API endpoint. Redirects are refused.
This is a build dependency fetch, never an application/runtime credential path.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import urllib.request

BASELINE = "8f363bdc11359ede4bc51e5eb32a2617538049c8"
PORT_TREE = "53bb7f4499d752b9e4d725b8b419ff225fef76e6"
PORT_SHA512 = "2fcc11ebc97497e5c769019abd3a1cf195c84b6438854f570e7ed7f8a91d99d9b4d7976ac8798069d9912a0fdf29bde95e6cf48d522953cbe89381d91000a91e"
MANIFEST_SHA512 = "ab6a5b4754b09cbe1b5c559ac3659e7239815dcb2e659e072312e7b1ed5e72116c20114b353b639a85a93095c8b78b8f0a913799960879df40a52e191b13fc5a"
ASSET_SHA512 = "719fa1d8d8e1e749ce6f7370db32818268f84b1376ed9367fd5290ab440d8bea7a6a25a68e55045a45da68b0ce7a55daa0d7acd2f1b11774319b50268c02441c"
URL = "https://api.github.com/repos/PCRE2Project/pcre2/contents/deps/sljit?ref=pcre2-10.49"
FILENAME = "pcre2-pcre2-10.49-sljit-gitlink.json"
TOKEN_ENV = "ELYTRA_DEPENDENCY_GITHUB_TOKEN"
MAX_BYTES = 65536
TIMEOUT = 30


class PrefetchError(ValueError):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise PrefetchError("Native asset redirect refused")


def git_object(vcpkg, name):
    result = subprocess.run(
        ["git", "-C", str(vcpkg), "show", name],
        capture_output=True,
        env={key: value for key, value in os.environ.items() if key != TOKEN_ENV},
        check=False,
        timeout=15,
    )
    if result.returncode or len(result.stdout) > 1024 * 1024:
        raise PrefetchError("Pinned vcpkg object missing; run the dependency bootstrap")
    return result.stdout


def verify_pins(root, vcpkg):
    try:
        for manifest in (root / "vcpkg.json", root / "vendor/ladybird/vcpkg.json"):
            if json.loads(manifest.read_bytes())["builtin-baseline"] != BASELINE:
                raise PrefetchError("Native dependency baseline changed; review asset pins")
        versions = json.loads(git_object(vcpkg, BASELINE + ":versions/p-/pcre2.json"))["versions"]
        if not isinstance(versions, list) or any(not isinstance(v, dict) for v in versions):
            raise PrefetchError("Malformed PCRE2 version inventory")
        selected = [
            v for v in versions if v.get("version") == "10.49" and v.get("port-version", 0) == 2
        ]
        if len(selected) != 1 or selected[0]["git-tree"] != PORT_TREE:
            raise PrefetchError("Pinned PCRE2 versioned tree changed")
        for name, digest in (("portfile.cmake", PORT_SHA512), ("vcpkg.json", MANIFEST_SHA512)):
            if hashlib.sha512(git_object(vcpkg, PORT_TREE + ":" + name)).hexdigest() != digest:
                raise PrefetchError("Pinned PCRE2 port bytes changed")
    except (KeyError, TypeError, json.JSONDecodeError, OSError) as error:
        raise PrefetchError("Malformed native dependency pin inputs") from error


def verify_bytes(data):
    if len(data) > MAX_BYTES or hashlib.sha512(data).hexdigest() != ASSET_SHA512:
        raise PrefetchError("Native asset SHA512 mismatch; cache was not published")


def prefetch(root, vcpkg, downloads, *, opener=None, environment=None):
    verify_pins(root, vcpkg)
    destination = downloads / FILENAME
    if destination.is_symlink():
        raise PrefetchError("Native asset cache entry must not be a symlink")
    if destination.exists():
        if not destination.is_file() or destination.stat().st_size > MAX_BYTES:
            raise PrefetchError("Invalid native asset cache entry")
        verify_bytes(destination.read_bytes())
        return "verified cache"
    environment = os.environ if environment is None else environment
    token = environment.get(TOKEN_ENV, "")
    if not token or len(token) > 4096 or any(ord(c) < 33 or ord(c) > 126 for c in token):
        raise PrefetchError("Cache miss requires a valid " + TOKEN_ENV + " environment credential")
    request = urllib.request.Request(
        URL,
        headers={
            "Authorization": "Bearer " + token,
            "User-Agent": "Elytra-native-asset-prefetch",
            # Match vcpkg/curl content negotiation: an absent Accept returns a
            # compact JSON representation with a different (correctly refused) hash.
            "Accept": "*/*",
        },
    )
    opener = opener or urllib.request.build_opener(NoRedirect())
    deadline = time.monotonic() + TIMEOUT
    try:
        with opener.open(request, timeout=TIMEOUT) as response:
            if response.geturl() != URL or response.status != 200:
                raise PrefetchError("Native asset endpoint or response status refused")
            data = bytearray()
            while True:
                if time.monotonic() >= deadline:
                    raise PrefetchError("Native asset fetch deadline exceeded")
                chunk = response.read(min(8192, MAX_BYTES + 1 - len(data)))
                if not chunk:
                    break
                data.extend(chunk)
                if time.monotonic() >= deadline:
                    raise PrefetchError("Native asset fetch deadline exceeded")
                if len(data) > MAX_BYTES:
                    raise PrefetchError("Native asset response exceeded byte limit")
    except Exception:
        # Network exceptions may contain request headers or hostile response text.
        # Never propagate them or their chained exception into build logs.
        raise PrefetchError("Native asset fetch failed; no cache entry published") from None
    # Fixed scalar diagnostics only. Do not echo arbitrary response headers,
    # response bodies, request headers, credentials, or exception text.
    content_type = response.headers.get("Content-Type", "")
    api_version = response.headers.get("X-GitHub-Api-Version-Selected", "")
    print(
        "Native asset response:",
        json.dumps(
            {
                "status": 200,
                "bytes": len(data),
                "sha512": hashlib.sha512(data).hexdigest(),
                "contentType": content_type
                if content_type == "application/json; charset=utf-8"
                else "other",
                "apiVersion": api_version if api_version == "2022-11-28" else "other",
            },
            sort_keys=True,
        ),
    )
    verify_bytes(data)
    downloads.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            prefix=".elytra-native-", dir=downloads, delete=False
        ) as file:
            temporary = Path(file.name)
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        # Another prefetcher may already have published the same verified bytes.
        if destination.exists() or destination.is_symlink():
            if (
                destination.is_symlink()
                or not destination.is_file()
                or destination.stat().st_size > MAX_BYTES
            ):
                raise PrefetchError("Native asset cache collision")
            verify_bytes(destination.read_bytes())
        else:
            os.replace(temporary, destination)
        return "prefetched verified asset"
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg-root", type=Path, default=root / "build/vcpkg")
    parser.add_argument("--downloads", type=Path)
    args = parser.parse_args()
    downloads = args.downloads or args.vcpkg_root / "downloads"
    try:
        print("PCRE2 SLJIT:", prefetch(root, args.vcpkg_root, downloads))
    except PrefetchError as error:
        parser.exit(1, str(error) + "\n")
    except (OSError, subprocess.SubprocessError):
        # CLI output deliberately excludes arbitrary native/network exception text.
        parser.exit(
            1,
            "Native asset prefetch failed (pins/cache/credential/network); no unverified asset accepted\n",
        )


if __name__ == "__main__":
    main()
