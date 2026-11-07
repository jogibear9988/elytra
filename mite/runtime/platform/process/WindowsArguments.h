/** Own UTF-8 argument storage at native Windows entry points, without changing host policy. */
#pragma once

#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <AK/Windows.h>

namespace Mite {
class WindowsArguments {
public:
    WindowsArguments() = default;
    WindowsArguments(WindowsArguments const&) = delete;
    WindowsArguments& operator=(WindowsArguments const&) = delete;

    bool initialize(int count, wchar_t const* const* wide)
    {
        static_assert(sizeof(wchar_t) == 2);

        m_error = 0;

        m_arguments.clear();

        m_storage.clear();

        m_storage.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i) {
            auto size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, nullptr, 0, nullptr, nullptr);

            if (!size) {
                m_error = GetLastError();

                return false;
            }

            std::string bytes(static_cast<size_t>(size), '\0');

            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, bytes.data(), size,
                    nullptr, nullptr)
                != size) {
                m_error = GetLastError();

                return false;
            }

            bytes.pop_back(); // The conversion count includes the terminating NUL.

            m_storage.push_back(std::move(bytes));
        }

        // Publish pointers only after all string/vector storage has stabilized.
        m_arguments.reserve(static_cast<size_t>(count) + 1);

        for (auto& bytes : m_storage)
            m_arguments.push_back(bytes.data());

        m_arguments.push_back(nullptr);

        return true;
    }

    char** data()
    {
        return m_arguments.data();
    }

    DWORD error() const
    {
        return m_error;
    }

private:
    std::vector<std::string> m_storage;
    std::vector<char*> m_arguments;
    DWORD m_error { 0 };
};
}
#endif
