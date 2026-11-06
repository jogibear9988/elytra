# Mite and Firefly implementation

Read `mite-firefly-agent-instructions.md` before making changes; it is the product specification.
Read `docs/PROGRESS.md` to resume work. Keep it current in each logical commit.

- Mite embeds LibJS, never Node or V8. Mite must not depend on Firefly/LibWeb.
- Pin upstream code and preserve license notices.
- Build and test working vertical slices; distinguish implemented, tested, and planned features.
- Unsupported APIs must fail explicitly. Never advertise unverified platform support.
- Commit each coherent step with its validation and remaining work recorded.
- Keep Ladybird unmodified: integration fixes belong in our adapters/build files, not a downstream patch stack. Preserve the ability to update upstream regularly. Follow `docs/development/updating-ladybird.md` and keep the manifest/gitlink synchronized.

- Keep local GUI tests infrequent and batched: they disrupt the user's desktop. Run targeted UI acceptance for substantial renderer/window behavior changes; prefer builds, non-GUI checks and hosted CI otherwise. Do not repeatedly launch desktop suites for documentation, formatting or unchanged behavior.
