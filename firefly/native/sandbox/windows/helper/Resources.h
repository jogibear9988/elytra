/** SID-specific access to the physical, owned helper runtime; never grant a parent SDK tree. */
#pragma once
#include "../Sandbox.h"
#include <filesystem>

namespace Firefly::WindowsHelpers {
class Resources {
public:
    static std::unique_ptr<Resources> acquire(
        std::filesystem::path const& root, PSID, WindowsSandbox::Failure&);

    ~Resources();

private:
    struct Entry {
        WindowsSandbox::Handle handle;
        std::wstring relative;
        bool directory { false };
        bool granted { false };
    };

    std::vector<unsigned char> m_sid;
    std::vector<Entry> m_entries;
};
}
