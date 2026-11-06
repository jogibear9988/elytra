/**
 * Composes the TreeWalker binding seam for approved callback worlds. Retain native traversal
 * lifetime and reject cross-world callback capability leaks.
 */

#include "callbacks/CallbackGate.h"
#include <TreeWalkerBinding.inc>
