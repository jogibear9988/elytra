/** Decode trusted metadata before any preload source reaches a script executor. */
#pragma once
#include <AK/Error.h>
#include <LibWebCommon/WebDriver/BiDi.h>

namespace Firefly::HelperAutoplay {
ErrorOr<void> extract(Web::WebDriver::BiDiSessionState&);
}
