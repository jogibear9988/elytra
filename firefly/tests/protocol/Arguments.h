/** Own native fixture arguments as UTF-8; upstream LibMain uses narrow CRT argv on Windows. */
#pragma once

#include <AK/ByteBuffer.h>
#include <AK/ByteString.h>
#include <AK/ScopeGuard.h>
#include <AK/Vector.h>
#include <LibMain/Main.h>
#ifdef AK_OS_WINDOWS
#include <AK/Windows.h>
#include <shellapi.h>
#endif

namespace Firefly::Tests {
class ProtocolArguments {
    AK_MAKE_NONCOPYABLE(ProtocolArguments);

    AK_MAKE_NONMOVABLE(ProtocolArguments);

public:
    ProtocolArguments() = default;

    ErrorOr<void> initialize(Main::Arguments original)
    {
        m_arguments = original;
#ifdef AK_OS_WINDOWS
        // Recover the Unicode command line before any path is interpreted. Do
        // not reinterpret locale-dependent narrow bytes or change the fixture.
        int count = 0;
        auto wide = CommandLineToArgvW(GetCommandLineW(), &count);

        if (!wide)
            return Error::from_windows_error();

        ScopeGuard release = [&] { LocalFree(wide); };
        if (count != original.argc)
            return Error::from_string_literal("Native fixture argument count mismatch");

        for (int i = 0; i < count; ++i) {
            auto size = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, nullptr, 0, nullptr, nullptr);

            if (!size)
                return Error::from_windows_error();

            auto bytes = TRY(ByteBuffer::create_uninitialized(static_cast<size_t>(size)));

            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1,
                    reinterpret_cast<char*>(bytes.data()), size, nullptr, nullptr)
                != size)
                return Error::from_windows_error();

            TRY(m_storage.try_append(ByteString(
                reinterpret_cast<char const*>(bytes.data()), static_cast<size_t>(size - 1))));
        }

        // Publish views/pointers only after storage is stable. The owner remains
        // on the fixture's stack until all operations and event pumping finish.
        for (auto& text : m_storage) {
            TRY(m_views.try_append(text.view()));

            TRY(m_argv.try_append(const_cast<char*>(text.characters())));
        }

        TRY(m_argv.try_append(nullptr));

        m_arguments = { count, m_argv.data(), m_views.span() };
#endif
        return {};
    }

    Main::Arguments arguments() const
    {
        return m_arguments;
    }

private:
    Main::Arguments m_arguments;
#ifdef AK_OS_WINDOWS
    Vector<ByteString> m_storage;
    Vector<StringView> m_views;
    Vector<char*> m_argv;
#endif
};
}
