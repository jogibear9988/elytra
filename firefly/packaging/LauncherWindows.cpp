/** Launch Firefly from a relocated Windows package with native UTF-16 arguments. */
#ifndef _WIN32
#error This launcher requires native Windows
#endif

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#ifndef FIREFLY_ELECTRON_COMPAT
#define FIREFLY_ELECTRON_COMPAT 0
#endif

namespace {
constexpr size_t command_line_limit = 32767;

void append_argument(std::wstring& line, std::wstring_view argument)
{
    if (!line.empty())
        line.push_back(L' ');
    line.push_back(L'"');

    // The Windows CRT consumes backslashes in pairs before a quote. Double
    // trailing backslashes so they cannot escape the enclosing quote.
    size_t backslashes = 0;
    for (auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }

        line.append(backslashes * (character == L'"' ? 2 : 1), L'\\');
        backslashes = 0;
        if (character == L'"')
            line.push_back(L'\\');
        line.push_back(character);
    }

    line.append(backslashes * 2, L'\\');
    line.push_back(L'"');
}

bool is_package_file(std::filesystem::path const& path)
{
    auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

struct ArgumentDeleter {
    void operator()(wchar_t** arguments) const
    {
        LocalFree(arguments);
    }
};

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    std::wstring executable(command_line_limit, L'\0');
    auto length
        = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size())
        return 1;
    executable.resize(length);

    auto root = std::filesystem::path(executable).parent_path();
    auto runtime = root / L"bin" / L"firefly.exe";
    auto entry = root / L"app" / L"main.mjs";
    if (!is_package_file(runtime) || !is_package_file(entry))
        return 1;

    int count = 0;
    std::unique_ptr<wchar_t*, ArgumentDeleter> arguments(
        CommandLineToArgvW(GetCommandLineW(), &count));
    if (!arguments || count < 1)
        return 1;

    std::wstring command;
    append_argument(command, runtime.native());
#if FIREFLY_ELECTRON_COMPAT
    append_argument(command, L"--electron-compat");
#endif
    append_argument(command, entry.native());
    for (int index = 1; index < count; ++index)
        append_argument(command, arguments.get()[index]);
    if (command.size() >= command_line_limit)
        return 1;

    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child {};
    // An explicit image eliminates PATH lookup. The child keeps the caller's
    // working directory but receives no inherited handles.
    if (!CreateProcessW(runtime.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
            nullptr, &startup, &child))
        return 1;
    CloseHandle(child.hThread);

    DWORD status = 1;
    if (WaitForSingleObject(child.hProcess, INFINITE) == WAIT_OBJECT_0) {
        if (!GetExitCodeProcess(child.hProcess, &status))
            status = 1;
    }

    CloseHandle(child.hProcess);
    ExitProcess(status);
}
