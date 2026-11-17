# Pinned native dependency fixture

pcre2-port.json records exact Microsoft/vcpkg objects at baseline
8f363bdc11359ede4bc51e5eb32a2617538049c8 and PCRE2 tree
53bb7f4499d752b9e4d725b8b419ff225fef76e6. It supplies independent source-pin
negative controls without a native dependency build or network request.
The original vcpkg MIT notice is retained in LICENSE.vcpkg.txt.

`pcre2-sljit-gitlink.response` preserves the 848 public response bytes from
https://api.github.com/repos/PCRE2Project/pcre2/contents/deps/sljit?ref=pcre2-10.49
with `Accept: */*`, matching the unmodified versioned vcpkg port's SHA512
`719fa1d8d8e1e749ce6f7370db32818268f84b1376ed9367fd5290ab440d8bea7a6a25a68e55045a45da68b0ce7a55daa0d7acd2f1b11774319b50268c02441c`.
The public response was checked on 2026-11-17. Omitting Accept produces 781
compact bytes with a different digest; the prefetcher refuses that representation
and never rewrites JSON. The `.response` suffix keeps formatters from changing
the pinned payload. This is GitHub's public gitlink metadata, not package source
or authenticated/private response data.
