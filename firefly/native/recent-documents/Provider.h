/** App-owned OS recent documents; these operations neither open documents nor display UI. */
#pragma once
#include <string>

namespace Firefly::RecentDocuments {
struct Result {
    std::string code;
    std::string message;
};

bool available();

Result clear();

Result add(std::string const& absolute_path);
}
