/** Native build-only rustc driver: pass arguments through Python without invoking a shell. */
#include "DriverConfig.h"
#include <cstdio>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

static std::wstring wide(char const* text)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (!length)
        return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), length))
        return {};
    result.pop_back();
    return result;
}

// Quote exactly one CRT argv value, including empty/quoted/trailing-backslash values.
static std::wstring quote(std::wstring const& text)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : text) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }

        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += ch;
    }

    result.append(slashes * 2, L'\\');
    return result + L'"';
}

int wmain(int argc, wchar_t** argv)
{
    auto python = wide(FIREFLY_DRIVER_PYTHON);
    auto script = wide(FIREFLY_DRIVER_SCRIPT);
    auto config = wide(FIREFLY_DRIVER_CONFIG);
    if (python.empty() || script.empty() || config.empty())
        return 127;
    auto command = quote(python) + L" " + quote(script) + L" " + quote(config);
    for (int i = 1; i < argc; ++i)
        command += L" " + quote(argv[i]);
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if (!CreateProcessW(python.c_str(), command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
            &startup, &process))
        return 127;
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 127;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);
    return static_cast<int>(status);
}
#else
#include <unistd.h>

int main(int argc, char** argv)
{
    std::vector<char*> arguments { const_cast<char*>(FIREFLY_DRIVER_PYTHON),
        const_cast<char*>(FIREFLY_DRIVER_SCRIPT), const_cast<char*>(FIREFLY_DRIVER_CONFIG) };
    for (int i = 1; i < argc; ++i)
        arguments.push_back(argv[i]);
    arguments.push_back(nullptr);
    execv(FIREFLY_DRIVER_PYTHON, arguments.data());
    std::perror("Cannot run the owned build compiler driver");
    return 127;
}
#endif
