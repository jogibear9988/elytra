/** Exact physical files admitted by the stage seal; no directory-name-based resource authority. */
#pragma once
#include "../Sandbox.h"
#include <map>
#include <set>

namespace Firefly::WindowsHelpers {
struct Inventory {
    struct Record {
        std::string hash;
        unsigned long long size;
    };

    struct Less {
        bool operator()(std::wstring const& a, std::wstring const& b) const
        {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        }
    };

    static constexpr wchar_t name[] = L".firefly-windows-runtime-v1";
    static std::unique_ptr<Inventory> read(HANDLE, WindowsSandbox::Failure&);

    static bool verify(HANDLE, Record const&, WindowsSandbox::Failure&);

    std::map<std::wstring, Record, Less> files;
    std::set<std::wstring, Less> directories { L"", L"bin", L"share", L"share/Lagom" };
};
}
