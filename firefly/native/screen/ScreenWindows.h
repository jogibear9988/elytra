/** Pure translation of Win32 display orientation into public clockwise degrees. */
#pragma once

namespace Firefly {
int windows_display_rotation(unsigned long native_orientation);
}
