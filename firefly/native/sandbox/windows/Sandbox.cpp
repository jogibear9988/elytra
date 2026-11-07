/** Own Win32 AppContainer/profile/process resources; all launch failures close the suspended child.
 */
#include "Sandbox.h"
#include <algorithm>
#include <bcrypt.h>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iterator>
#include <shlobj.h>
#include <userenv.h>

namespace Firefly::WindowsSandbox {
namespace {
    bool fail(Failure& error, char const* operation, DWORD code = GetLastError())
    {
        error = { operation, code };
        return false;
    }

    std::wstring quote(std::wstring const& value)
    {
        std::wstring result = L"\"";
        size_t slashes = 0;
        for (auto ch : value) {
            if (ch == L'\\') {
                ++slashes;
                continue;
            }

            result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            result.push_back(ch);
            slashes = 0;
        }

        result.append(slashes * 2, L'\\');
        result.push_back(L'"');
        return result;
    }
}

Handle& Handle::operator=(Handle&& other) noexcept
{
    if (this != &other)
        reset(other.release());
    return *this;
}

Handle::~Handle()
{
    reset();
}

HANDLE Handle::release()
{
    auto value = m_value;
    m_value = nullptr;
    return value;
}

void Handle::reset(HANDLE value)
{
    if (*this)
        CloseHandle(m_value);
    m_value = value;
}

std::shared_ptr<Profile> Profile::create(Failure& error)
{
    unsigned char random[16];
    auto random_status
        = BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (random_status != 0) {
        fail(error, "BCryptGenRandom", static_cast<DWORD>(random_status));
        return {};
    }

    auto profile = std::shared_ptr<Profile>(new Profile);
    profile->m_name = L"elytra.firefly.sandbox.";
    for (auto byte : random) {
        profile->m_name.push_back(L"0123456789abcdef"[byte >> 4]);
        profile->m_name.push_back(L"0123456789abcdef"[byte & 15]);
    }

    auto result = CreateAppContainerProfile(profile->m_name.c_str(), L"Firefly isolated child",
        L"Owned native launch profile", nullptr, 0, &profile->m_sid);
    if (FAILED(result)) {
        fail(error, "CreateAppContainerProfile", static_cast<DWORD>(result));
        return {};
    }

    profile->m_registered = true;
    return profile;
}

bool Profile::remove(Failure& error)
{
    std::lock_guard lock(m_lock);
    if (m_children)
        return fail(error, "profile has active children", ERROR_BUSY);
    if (!m_registered)
        return true;
    auto result = DeleteAppContainerProfile(m_name.c_str());
    if (FAILED(result))
        return fail(error, "DeleteAppContainerProfile", static_cast<DWORD>(result));
    m_registered = false;
    return true;
}

bool Profile::retain(Failure& error)
{
    std::lock_guard lock(m_lock);
    if (!m_registered)
        return fail(error, "profile is closed", ERROR_INVALID_PARAMETER);
    ++m_children;
    return true;
}

void Profile::release()
{
    std::lock_guard lock(m_lock);
    --m_children;
}

Profile::~Profile()
{
    Failure error;
    if (!remove(error))
        std::fprintf(stderr, "Profile cleanup failed: %s %lu\n", error.operation, error.code);
    if (m_sid)
        FreeSid(m_sid);
}

std::unique_ptr<Child> Child::create(
    std::shared_ptr<Profile> profile, Launch const& options, Failure& error)
{
    auto child = std::unique_ptr<Child>(new Child);
    if (!profile || !profile->sid() || !std::filesystem::path(options.executable).is_absolute()
        || !std::filesystem::path(options.directory).is_absolute()) {
        fail(error, "launch inputs", ERROR_INVALID_PARAMETER);
        return {};
    }

    auto invalid_string
        = [](std::wstring const& value) { return value.find(L'\0') != std::wstring::npos; };
    if (invalid_string(options.executable) || invalid_string(options.directory)
        || std::any_of(options.arguments.begin(), options.arguments.end(), invalid_string)) {
        fail(error, "NUL launch input", ERROR_INVALID_PARAMETER);
        return {};
    }

    if (!profile->retain(error))
        return {};
    child->m_profile = std::move(profile);
    auto handles = options.inherited;
    std::sort(handles.begin(), handles.end(), std::less<HANDLE> {});
    if (handles.empty() || std::adjacent_find(handles.begin(), handles.end()) != handles.end()) {
        fail(error, "explicit handle list", ERROR_INVALID_PARAMETER);
        return {};
    }

    for (auto handle : handles) {
        DWORD flags = 0;
        if (!handle || handle == INVALID_HANDLE_VALUE || !GetHandleInformation(handle, &flags)
            || !(flags & HANDLE_FLAG_INHERIT)) {
            fail(error, "inheritable handle list", ERROR_INVALID_PARAMETER);
            return {};
        }
    }

    for (auto stream : { options.input, options.output, options.error }) {
        if (std::find(handles.begin(), handles.end(), stream) == handles.end()) {
            fail(error, "standard handle allowlist", ERROR_INVALID_PARAMETER);
            return {};
        }
    }

    child->m_job.reset(CreateJobObjectW(nullptr, nullptr));
    if (!child->m_job) {
        fail(error, "CreateJobObject");
        return {};
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
    limits.BasicLimitInformation.LimitFlags
        = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
    if (!SetInformationJobObject(
            child->m_job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        fail(error, "SetInformationJobObject");
        return {};
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual {};
    if (!QueryInformationJobObject(
            child->m_job.get(), JobObjectExtendedLimitInformation, &actual, sizeof(actual), nullptr)
        || actual.BasicLimitInformation.LimitFlags != limits.BasicLimitInformation.LimitFlags
        || actual.BasicLimitInformation.ActiveProcessLimit != 1) {
        fail(error, "verify job policy", ERROR_INVALID_DATA);
        return {};
    }

    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 3, 0, &bytes);
    if (!bytes) {
        fail(error, "attribute list size");
        return {};
    }

    std::vector<unsigned char> storage(bytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 3, 0, &bytes)) {
        fail(error, "InitializeProcThreadAttributeList");
        return {};
    }

    struct AttributeOwner {
        LPPROC_THREAD_ATTRIBUTE_LIST value;

        ~AttributeOwner()
        {
            DeleteProcThreadAttributeList(value);
        }
    } owner { attributes };

    SECURITY_CAPABILITIES capabilities {};
    capabilities.AppContainerSid = child->m_profile->sid();
    // Assign atomically during creation. A later AssignProcessToJobObject call
    // would leave an orphaned suspended child if the broker died in between.
    HANDLE job = child->m_job.get();
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
            &capabilities, sizeof(capabilities), nullptr, nullptr)
        || !UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handles.data(), handles.size() * sizeof(HANDLE), nullptr, nullptr)
        || !UpdateProcThreadAttribute(
            attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job, sizeof(job), nullptr, nullptr)) {
        fail(error, "UpdateProcThreadAttribute");
        return {};
    }

    wchar_t windows[32768];
    auto length = GetWindowsDirectoryW(windows, static_cast<UINT>(std::size(windows)));
    if (!length || length >= std::size(windows)) {
        fail(error, "GetWindowsDirectory");
        return {};
    }
    // Windows needs LOCALAPPDATA while constructing the AppContainer environment.
    // Resolve its native known folder instead of copying ambient configuration.
    PWSTR local_app_data = nullptr;
    auto folder_status = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data);
    if (FAILED(folder_status)) {
        CoTaskMemFree(local_app_data);
        fail(error, "SHGetKnownFolderPath LocalAppData", static_cast<DWORD>(folder_status));
        return {};
    }

    std::wstring local_directory(local_app_data);
    CoTaskMemFree(local_app_data);
    // Ambient main-process secrets/configuration never enter the renderer environment.
    std::wstring environment;
    for (auto const& entry : { std::wstring(L"LOCALAPPDATA=") + local_directory,
             std::wstring(L"PATH=") + windows + L"\\System32",
             std::wstring(L"SystemRoot=") + windows, std::wstring(L"WINDIR=") + windows }) {
        environment += entry;
        environment.push_back(L'\0');
    }

    environment.push_back(L'\0');
    std::wstring command = quote(options.executable);
    for (auto const& argument : options.arguments)
        command += L" " + quote(argument);
    STARTUPINFOEXW startup {};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = options.input;
    startup.StartupInfo.hStdOutput = options.output;
    startup.StartupInfo.hStdError = options.error;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION info {};
    if (!CreateProcessW(options.executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW
                | EXTENDED_STARTUPINFO_PRESENT,
            environment.data(), options.directory.c_str(), &startup.StartupInfo, &info)) {
        fail(error, "CreateProcessW AppContainer");
        return {};
    }

    child->m_process.reset(info.hProcess);
    child->m_thread.reset(info.hThread);
    child->m_pid = info.dwProcessId;
    BOOL in_job = FALSE;
    if (!IsProcessInJob(child->m_process.get(), child->m_job.get(), &in_job) || !in_job) {
        fail(error, "verify child job", ERROR_INVALID_DATA);
        return {};
    }

    return child;
}

bool Child::resume(Failure& error)
{
    if (m_resumed)
        return fail(error, "resume once", ERROR_INVALID_PARAMETER);
    if (ResumeThread(m_thread.get()) != 1)
        return fail(error, "ResumeThread");
    m_resumed = true;
    m_thread.reset();
    return true;
}

bool Child::wait(DWORD timeout, DWORD& exit_code, Failure& error)
{
    auto result = WaitForSingleObject(m_process.get(), timeout);
    if (result != WAIT_OBJECT_0)
        return fail(
            error, "WaitForSingleObject", result == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError());
    if (!GetExitCodeProcess(m_process.get(), &exit_code))
        return fail(error, "GetExitCodeProcess");
    return true;
}

Child::~Child()
{
    // The kernel closes our job even when the broker is killed. Explicit
    // termination also covers a failed post-creation verification.
    m_job.reset();
    if (m_process && WaitForSingleObject(m_process.get(), 0) == WAIT_TIMEOUT) {
        TerminateProcess(m_process.get(), 1);
        WaitForSingleObject(m_process.get(), 5000);
    }

    m_thread.reset();
    m_process.reset();
    if (m_profile)
        m_profile->release();
}
}
