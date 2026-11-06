/**
 * Copies approved CustomEvent detail into destination-owned snapshots. Source values and raw
 * callable capabilities cannot leak through event accessors.
 */

#include "serialization/EventValues.h"
#include <CustomEventNative.inc>
