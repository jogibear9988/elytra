/** Retire actual Hub/UI callback ownership before late IPC self-references are destroyed. */
#pragma once

namespace Hub {
class ConnectionFromUI;
}

namespace Ladybird {
class HubConnection;
}

namespace Firefly::HubLifecycle {
// Owner thread, while the actual Hub Application and its callback stores live.
// Idempotent for this endpoint; leaves ordinary upstream endpoint cleanup intact.
void retire_ui_connection(Hub::ConnectionFromUI&);

void retire_ui_client(Ladybird::HubConnection&);
}
