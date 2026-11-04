# Mite and Firefly implementation

Read `mite-firefly-agent-instructions.md` before making changes; it is the product specification.
Read `docs/PROGRESS.md` to resume work. Keep it current in each logical commit.

- Mite embeds LibJS, never Node or V8. Mite must not depend on Firefly/LibWeb.
- Pin upstream code and preserve license notices.
- Build and test working vertical slices; distinguish implemented, tested, and planned features.
- Unsupported APIs must fail explicitly. Never advertise unverified platform support.
- Commit each coherent step with its validation and remaining work recorded.
- Do not edit the upstream submodule without documenting the patch and reason.
