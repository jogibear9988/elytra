/** Native Cocoa caption visibility on real full-size titled windows, not frameless emulation. */
#pragma once

class QWidget;

namespace Firefly {
bool title_bar_available();

// Hidden requires the expanded-client and transparent-titlebar Qt flags set
// before native creation. The owned controller reapplies caption policy on rebind.
bool configure_title_bar(QWidget& window, bool hidden);
}
