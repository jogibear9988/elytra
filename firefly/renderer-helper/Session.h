#pragma once
#include <LibWebCommon/WebDriver/BiDi.h>
namespace Firefly::RendererSession {
void initialize();
void apply(Web::WebDriver::BiDiSessionState);
void shutdown();
}
