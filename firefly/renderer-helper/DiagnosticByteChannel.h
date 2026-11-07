/** Native-only transport acceptance fixture; never installed in the product helper. */
#pragma once

namespace Firefly::RendererSession {
void initialize_byte_diagnostic();

void shutdown_byte_diagnostic();
}
