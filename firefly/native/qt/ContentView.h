/** Opaque application canvas defaults for the real, final upstream WebContentView widget. */
#pragma once
#include <AK/Types.h>

namespace Ladybird {
class WebContentView;
}

namespace Firefly {
void initialize_content_background(Ladybird::WebContentView&, u32 rgb = 0xffffff);

void set_content_background(Ladybird::WebContentView&, u32 rgb);
}
