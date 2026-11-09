/** Strip trusted metadata-only records before any upstream/preload script processing. */
#pragma once
#include <AK/Error.h>
#include <LibWebCommon/WebDriver/BiDi.h>

namespace Firefly::HelperCanvas {
void initialize();

ErrorOr<void> extract(Web::WebDriver::BiDiSessionState&);

void shutdown();
}
