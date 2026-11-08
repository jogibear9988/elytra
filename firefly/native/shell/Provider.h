/** Native desktop launches and reversible OS trash. No VM/GC or Qt objects cross callbacks. */
#pragma once
#include <functional>
#include <memory>
#include <string>

namespace Firefly::Shell {
struct Result {
    std::string code;
    std::string message;
};

using Completion = std::function<void(Result)>;

class Provider {
public:
    static std::unique_ptr<Provider> create();

    virtual ~Provider() = default;
    virtual Result reveal(std::string const& path) = 0;
    virtual void open_external(std::string url, bool activate, Completion) = 0;
    virtual void open_path(std::string path, Completion) = 0;
    virtual void trash(std::string path, Completion) = 0;
    // Revokes completion delivery, not an already submitted OS operation. Native
    // callback tickets remain charged until actual OS callbacks/workers complete.
    virtual void stop() = 0;
};
}
