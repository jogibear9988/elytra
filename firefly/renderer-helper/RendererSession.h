/**
 * Declares the helper service lifecycle shared by product and diagnostic executables.
 * Initialization precedes configuration; shutdown releases native and realm ownership.
 */

#pragma once
#include <LibWebCommon/WebDriver/BiDi.h>

namespace Firefly::RendererSession {
void initialize();

void apply(Web::WebDriver::BiDiSessionState);

void shutdown();
}
