/** Real OS capture-source metadata. Completion never carries VM or QWidget objects. */
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Firefly::DesktopCapture {
struct Source {
    std::string id;
    std::string name;
    std::string display_id;
};

struct Result {
    std::vector<Source> sources;
    // Empty code means success, including a genuinely empty OS source list.
    std::string code;
    std::string message;
};

using Completion = std::function<void(Result)>;

// Checks current OS access without requesting permission, enumerating or capturing.
bool preflight();

// Enumerates actual screen/window metadata. ScreenCaptureKit may request OS
// permission. No screenshot or stream is created, and no image pixels are read.
// Completion may be synchronous on rejection or run on an arbitrary OS queue.
// Pass only native state/weak dispatcher tokens, never JS/GC roots. The bridge
// must marshal back to its owner thread before accessing the VM.
void enumerate(Completion);
}
