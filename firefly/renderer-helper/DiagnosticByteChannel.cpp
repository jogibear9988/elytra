/** Exercise trusted handle delivery after the real WebContent sandbox is installed. */
#include "DiagnosticByteChannel.h"
#include "native/ipc/bootstrap/Bootstrap.h"
#include "native/ipc/bytes/Channel.h"
#include <AK/Array.h>
#include <AK/NeverDestroyed.h>
#include <LibCore/System.h>

namespace Firefly::RendererSession {
static NeverDestroyed<RefPtr<Bytes::Channel>> channel;
static bool admitted = false;

void initialize_byte_diagnostic()
{
    admitted = false;
    Bytes::set_bootstrap_receiver([](IPC::TransportHandle handle, Bytes::BootstrapCookie cookie) {
        // One admission per diagnostic session, including after disconnect. Dropping a
        // duplicate closes its transferred endpoint without replacing the live channel.
        if (admitted)
            return;
        admitted = true;
        auto transport = handle.create_transport();
        if (transport.is_error()) {
            warnln("Byte diagnostic transport failed: {}", transport.error());
            return;
        }

        auto created = Bytes::Channel::create(transport.release_value(),
            {
                .on_data =
                    [](ReadonlyBytes bytes) {
                        if (auto result = channel.get()->send(bytes); result.is_error()) {
                            warnln("Byte diagnostic echo failed: {}", result.error());
                            channel.get()->close();
                        }
                    },
                .on_writable = {},
                .on_closed = [](auto) {},
            });
        if (created.is_error()) {
            warnln("Byte diagnostic channel failed: {}", created.error());
            return;
        }

        *channel = created.release_value();
        Array<u8, 24> hello {};
        Array<u64, 3> words { cookie.high, cookie.low, static_cast<u64>(Core::System::getpid()) };
        for (size_t word = 0; word < words.size(); ++word)
            for (size_t byte = 0; byte < 8; ++byte)
                hello[word * 8 + byte] = static_cast<u8>(words[word] >> (byte * 8));
        if (auto result = channel.get()->send(hello); result.is_error()) {
            warnln("Byte diagnostic handshake failed: {}", result.error());
            channel.get()->close();
        }
    });
}

void shutdown_byte_diagnostic()
{
    Bytes::set_bootstrap_receiver({});
    if (*channel)
        channel.get()->close();
    channel->clear();
}
}
