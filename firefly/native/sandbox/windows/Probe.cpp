/** Native security fixture. Mutate only a fresh owned test tree/profile and exercise real denied
 * I/O. */
#include "Sandbox.h"
#include <aclapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <iostream>
#include <iterator>
#include <sddl.h>
#include <sstream>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace Firefly::WindowsSandbox;
namespace fs = std::filesystem;

namespace {
void require(bool value, char const* message)
{
    if (!value) {
        throw std::runtime_error(std::string(message) + " win32=" + std::to_string(GetLastError()));
    }
}

std::wstring sid_text(PSID sid)
{
    wchar_t* text = nullptr;
    require(ConvertSidToStringSidW(sid, &text), "SID text");
    std::wstring result(text);
    LocalFree(text);
    return result;
}

std::wstring user_sid()
{
    HANDLE raw = nullptr;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw), "own token");
    Handle token(raw);
    DWORD bytes = 0;
    GetTokenInformation(raw, TokenUser, nullptr, 0, &bytes);
    std::vector<unsigned char> data(bytes);
    require(GetTokenInformation(raw, TokenUser, data.data(), bytes, &bytes), "own SID");
    return sid_text(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid);
}

void seal(fs::path const& path, std::wstring const& user, std::wstring const& package,
    std::wstring const& access)
{
    // Explicit protected DACLs on this fresh fixture only. No broad package,
    // user-directory, system ACL or network-exemption changes are allowed.
    auto sddl = L"D:P(A;OICI;FA;;;" + user + L")(A;OICI;FA;;;SY)";
    if (!access.empty())
        sddl += L"(A;OICI;" + access + L";;;" + package + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr),
        "fixture descriptor");
    PACL dacl = nullptr;
    BOOL present = FALSE, defaulted = FALSE;
    require(GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted) && present,
        "fixture DACL");
    auto result = SetNamedSecurityInfoW(const_cast<wchar_t*>(path.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl,
        nullptr);
    LocalFree(descriptor);
    require(result == ERROR_SUCCESS, "set fixture DACL");
}

fs::path executable()
{
    wchar_t path[32768];
    auto length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    require(length && length < std::size(path), "native image path");
    return fs::path(path);
}

std::string json(std::wstring const& value)
{
    std::ostringstream out;
    out << '"';
    for (auto ch : value) {
        char encoded[7];
        std::snprintf(encoded, sizeof(encoded), "\\u%04x", static_cast<unsigned>(ch));
        out << encoded;
    }

    out << '"';
    return out.str();
}

std::string read_file(HANDLE handle)
{
    char bytes[64];
    DWORD count = 0;
    if (!ReadFile(handle, bytes, sizeof(bytes), &count, nullptr))
        return {};
    return { bytes, count };
}

bool excluded_handle(HANDLE handle)
{
    // This negative test deliberately probes a handle value absent from our
    // allowlist. Strict-handle mitigation may raise instead of returning an
    // invalid-handle error. Catch only that expected exception, only here.
    __try {
        SetLastError(ERROR_SUCCESS);
        auto type = GetFileType(handle);
        if (type == FILE_TYPE_UNKNOWN)
            return GetLastError() == ERROR_INVALID_HANDLE;
        if (type != FILE_TYPE_DISK)
            return true; // Startup may reuse this number for an unrelated object.
        char bytes[64];
        DWORD count = 0;
        if (!ReadFile(handle, bytes, sizeof(bytes), &count, nullptr))
            return GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_ACCESS_DENIED;
        return count != 17 || std::memcmp(bytes, "PRIVATE_TEST_ONLY", 17) != 0;
    } __except (GetExceptionCode() == 0xc0000008UL ? EXCEPTION_EXECUTE_HANDLER
                                                   : EXCEPTION_CONTINUE_SEARCH) {
        return true;
    }
}

struct NetworkResult {
    bool denied;
    char const* operation;
    int error;
};

NetworkResult denied_network(unsigned short port)
{
    WSADATA data {};
    auto startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0)
        return { false, "WSAStartup", startup };
    auto socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (socket == INVALID_SOCKET) {
        auto error = WSAGetLastError();
        WSACleanup();
        return { error == WSAEACCES, "WSASocket", error };
    }

    u_long nonblocking = 1;
    if (ioctlsocket(socket, FIONBIO, &nonblocking) != 0) {
        auto error = WSAGetLastError();
        closesocket(socket);
        WSACleanup();
        return { false, "ioctlsocket", error };
    }

    sockaddr_in target {};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    auto result = connect(socket, reinterpret_cast<sockaddr*>(&target), sizeof(target));
    auto error = result == 0 ? 0 : WSAGetLastError();
    char const* operation = "connect";
    if (error == WSAEWOULDBLOCK) {
        WSAPOLLFD wait { socket, POLLWRNORM, 0 };
        auto polled = WSAPoll(&wait, 1, 2000);
        if (polled > 0) {
            int size = sizeof(error);
            operation = "SO_ERROR";
            if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size)
                != 0) {
                error = WSAGetLastError();
                operation = "getsockopt failed";
            }
        } else {
            operation = polled == 0 ? "WSAPoll timeout" : "WSAPoll failed";
            error = polled == 0 ? WSAETIMEDOUT : WSAGetLastError();
        }
    }

    closesocket(socket);
    WSACleanup();
    return { (std::strcmp(operation, "connect") == 0 || std::strcmp(operation, "SO_ERROR") == 0)
            && error == WSAEACCES,
        operation, error };
}

int worker(int argc, wchar_t** argv)
{
    if (argc == 3 && std::wstring(argv[2]) == L"hold") {
        std::cout << "WORKER_RUNNING\n" << std::flush;
        Sleep(INFINITE);
        return 1;
    }

    require(argc == 6, "worker arguments");
    auto resources = fs::path(argv[2]);
    auto secret = fs::path(argv[3]);
    auto inherited = reinterpret_cast<HANDLE>(std::stoull(argv[4]));
    auto port = static_cast<unsigned short>(std::stoul(argv[5]));
    Handle allowed(CreateFileW((resources / L"allowed.txt").c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr));
    bool can_read = allowed && read_file(allowed.get()) == "ALLOWED";
    auto forbidden = CreateFileW(
        secret.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    bool read_denied = forbidden == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED;
    Handle forbidden_owner(forbidden);
    auto write = CreateFileW(
        (resources / L"blocked.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    bool write_denied = write == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED;
    Handle write_owner(write);
    bool handle_isolated = excluded_handle(inherited);
    wchar_t value[64];
    bool clean_env = GetEnvironmentVariableW(L"FIREFLY_SANDBOX_SECRET", value, 64) == 0
        && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    std::wstring command = L"worker --nested";
    bool nested = CreateProcessW(executable().c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, resources.c_str(), &startup, &process);
    if (nested) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }

    auto network = denied_network(port);
    auto all = can_read && read_denied && write_denied && handle_isolated && clean_env && !nested
        && network.denied;
    std::cout << "{\"allowedRead\":" << can_read << ",\"privateReadDenied\":" << read_denied
              << ",\"writeDenied\":" << write_denied << ",\"handleIsolated\":" << handle_isolated
              << ",\"cleanEnvironment\":" << clean_env << ",\"childSpawnDenied\":" << !nested
              << ",\"networkDenied\":" << network.denied << ",\"networkOperation\":\""
              << network.operation << "\",\"networkError\":" << network.error << "}\n"
              << std::flush;
    return all ? 0 : 1;
}
}

int run_probe(int argc, wchar_t** argv)
{
    if (argc > 1 && std::wstring(argv[1]) == L"--worker")
        return worker(argc, argv);
    require(argc == 4, "probe requires fresh root, port and checks/hold mode");
    fs::path root(argv[1]);
    require(root.is_absolute() && root.filename().wstring().starts_with(L"firefly-sandbox-")
            && fs::is_directory(root) && fs::is_empty(root),
        "fresh owned fixture root");
    auto attributes = GetFileAttributesW(root.c_str());
    require(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
        "no fixture reparse point");
    Failure error;
    auto profile = Profile::create(error);
    if (!profile) {
        std::cerr << error.operation << ": " << error.code << '\n';
        return 1;
    }

    auto user = user_sid();
    auto package = sid_text(profile->sid());
    seal(root, user, package, L"0x20"); // Directory traversal only.
    auto resources = root / L"resources \u00fc";
    fs::create_directory(resources);
    fs::create_directory(root / L"private");
    auto image = resources / L"worker.exe";
    fs::copy_file(executable(), image);
    {
        std::ofstream(resources / L"allowed.txt") << "ALLOWED";
    }

    {
        std::ofstream(root / L"private/secret.txt") << "PRIVATE_TEST_ONLY";
    }

    seal(resources, user, package, L"GRGX");
    seal(image, user, package, L"GRGX");
    seal(resources / L"allowed.txt", user, package, L"GR");
    seal(root / L"private", user, package, L"");
    seal(root / L"private/secret.txt", user, package, L"");
    SECURITY_ATTRIBUTES inherited { sizeof(inherited), nullptr, TRUE };
    HANDLE read_raw = nullptr, write_raw = nullptr;
    require(CreatePipe(&read_raw, &write_raw, &inherited, 4096), "private report pipe");
    Handle read(read_raw), write(write_raw);
    require(SetHandleInformation(read.get(), HANDLE_FLAG_INHERIT, 0), "private pipe reader");
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited,
        OPEN_EXISTING, 0, nullptr));
    Handle private_handle(CreateFileW((root / L"private/secret.txt").c_str(), GENERIC_READ,
        FILE_SHARE_READ, &inherited, OPEN_EXISTING, 0, nullptr));
    require(input && private_handle, "fixture handles");
    auto hold = std::wstring(argv[3]) == L"hold";
    Launch launch;
    launch.executable = image.wstring();
    launch.directory = resources.wstring();
    launch.arguments = hold
        ? std::vector<std::wstring> { L"--worker", L"hold" }
        : std::vector<std::wstring> { L"--worker", resources.wstring(),
              (root / L"private/secret.txt").wstring(),
              std::to_wstring(reinterpret_cast<uintptr_t>(private_handle.get())), argv[2] };
    launch.inherited = { input.get(), write.get() };
    launch.input = input.get();
    launch.output = launch.error = write.get();

    // Reject malformed caller contracts before creating an executable child.
    auto rejected = [&](Launch const& invalid) {
        Failure failure;
        require(
            !Child::create(profile, invalid, failure) && failure.code == ERROR_INVALID_PARAMETER,
            "invalid launch contract rejected");
    };

    auto invalid = launch;
    invalid.executable = L"relative.exe";
    rejected(invalid);
    invalid = launch;
    invalid.arguments.emplace_back(L"bad\0argument", 12);
    rejected(invalid);
    invalid = launch;
    invalid.inherited.push_back(write.get());
    rejected(invalid);
    invalid = launch;
    invalid.error = private_handle.get();
    rejected(invalid);
    invalid = launch;
    invalid.inherited.push_back(read.get()); // Deliberately non-inheritable.
    rejected(invalid);

    auto child = Child::create(profile, launch, error);
    if (!child) {
        std::cerr << error.operation << ": " << error.code << '\n';
        return 1;
    }

    Failure busy;
    require(
        !profile->remove(busy) && busy.code == ERROR_BUSY, "profile lifetime while child exists");
    write.reset(); // Only the child retains its report writer now.
    std::cout << "{\"pid\":" << child->pid() << ",\"profile\":" << json(profile->name())
              << ",\"suspended\":true}\n"
              << std::flush;
    std::string signal;
    require(static_cast<bool>(std::getline(std::cin, signal)) && signal == "GO",
        "independent token observation before resume");
    require(child->resume(error), "resume isolated worker");
    if (hold) {
        auto running = read_file(read.get());
        require(running == "WORKER_RUNNING\n", "actual worker executed before broker loss");
        std::cout << "{\"running\":" << child->pid() << "}\n" << std::flush;
        Sleep(INFINITE);
    }

    DWORD status = 1;
    require(child->wait(10000, status, error), "bounded worker completion");
    std::string report;
    for (;;) {
        auto part = read_file(read.get());
        if (part.empty())
            break;
        report += part;
        require(report.size() < 4096, "bounded worker report");
    }

    std::cout << report << std::flush;
    child.reset();
    require(profile->remove(error), "remove owned profile");
    require(!Child::create(profile, launch, error) && error.code == ERROR_INVALID_PARAMETER,
        "removed profile cannot launch");
    return static_cast<int>(status);
}

int wmain(int argc, wchar_t** argv)
{
    try {
        require(_setmode(_fileno(stdout), _O_BINARY) != -1, "binary report stream");
        std::cout << std::boolalpha;
        return run_probe(argc, argv);
    } catch (std::exception const& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
