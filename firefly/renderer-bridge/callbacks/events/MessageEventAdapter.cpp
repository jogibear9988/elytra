/**
 * Owns copied MessageEvent data and cache/lifetime behavior. Source detach must not
 * invalidate approved destination snapshots or expose raw functions.
 */

#include "serialization/EventValues.h"
#include <MessageEventNative.inc>
