/** Adapt only authenticated LibWebView helper spawns to the native AppContainer owner. */
#include "SDK.h"
#include "Resources.h"
#include <AK/ByteString.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <algorithm>
#include <filesystem>
#include <map>

namespace Firefly::WindowsHelpers {
namespace {
    thread_local Scope* current;

    struct Lease {
        std::shared_ptr<WindowsSandbox::Profile> profile;
        std::unique_ptr<Resources> resources;
        std::unique_ptr<WindowsSandbox::Child> child;
    };

    struct Registry {
        std::mutex lock;
        std::map<void*, std::unique_ptr<Lease>> leases;
    };

    Registry& registry()
    {
        // Real Core::Process owners release leases before their HANDLE closes.
        // Never run profile/ACL teardown under the Windows DLL loader lock.
        // Abrupt broker death closes Job handles in the kernel; its supervisor
        // owns recovery of abandoned unique profiles, as in the base launcher.
        static auto* value = new Registry;
        return *value;
    }

    Error win_error(WindowsSandbox::Failure const& error)
    {
        SetLastError(error.code);
        return Error::from_windows_error();
    }

    ErrorOr<std::wstring> wide(StringView value)
    {
        if (value.contains('\0') || value.length() > 65536)
            return Error::from_string_literal("Invalid helper text");
        if (value.is_empty())
            return std::wstring {};
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.characters_without_null_termination(), static_cast<int>(value.length()), nullptr,
            0);
        if (!count)
            return Error::from_windows_error();
        std::wstring result(count, L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                value.characters_without_null_termination(), static_cast<int>(value.length()),
                result.data(), count)
            != count)
            return Error::from_windows_error();
        return result;
    }

    wchar_t const* executable_name(Role role)
    {
        switch (role) {
        case Role::WebContent:
            return L"WebContent.exe";
        case Role::ImageDecoder:
            return L"ImageDecoder.exe";
        case Role::Unsupported:
            return nullptr;
        }

        return nullptr;
    }

    bool allowed_environment(StringView name)
    {
        // These helper roles need exactly their private pipes.
        // No loader/runtime/debug prefix is copied from the ambient main process.
        return name == "SOCKET_TAKEOVER"sv;
    }
}

Scope::Scope(Role value, Core::ProcessSpawnOptions const& spawn_options)
    : role(value)
    , options(&spawn_options)
    , previous(current)
{
    current = this;
}

Scope::~Scope()
{
    current = previous;
}

bool has_launch_scope()
{
    return current != nullptr;
}

ErrorOr<void*> spawn(Core::ProcessSpawnOptions const& options)
{
    auto const* image_name = current ? executable_name(current->role) : nullptr;
    if (!image_name || current->previous || current->options != &options)
        return Error::from_string_literal("Windows helper sandbox role is not implemented");
    if (options.search_for_executable_in_path || !options.die_with_parent
        || !options.environment.has_value() || options.environment->size() > 512
        || options.arguments.size() > 1024)
        return Error::from_string_literal("Invalid sandbox helper spawn contract");
    WindowsSandbox::Launch launch;
    launch.executable = TRY(wide(options.executable));
    auto executable = std::filesystem::path(launch.executable).lexically_normal();
    executable.make_preferred();
    launch.executable = executable.wstring();
    auto root = executable.parent_path().parent_path();
    if (!executable.is_absolute() || executable.filename() != image_name
        || executable.parent_path().filename() != L"bin"
        || (root.filename() != L"firefly-helper" && root.filename() != L"firefly-runtime"))
        return Error::from_string_literal("Helper must be in its physical owned runtime");
    launch.directory = executable.parent_path().wstring();
    size_t bytes = options.executable.length();
    for (auto const& argument : options.arguments) {
        if (argument == "--disable-sandbox"sv)
            return Error::from_string_literal("Confined helper cannot disable its sandbox check");
        if (argument.length() > 65536 || bytes > 1024 * 1024 - argument.length())
            return Error::from_string_literal("Helper argument budget exceeded");
        bytes += argument.length();
        launch.arguments.push_back(TRY(wide(argument)));
    }

    bool takeover = false;
    ByteString takeover_entry;
    for (auto const& entry : *options.environment) {
        auto separator = entry.find('=');
        if (!separator.has_value() || !allowed_environment(entry.substring_view(0, *separator)))
            continue;
        if (entry.length() > 65536 || bytes > 1024 * 1024 - entry.length())
            return Error::from_string_literal("Helper environment budget exceeded");
        bytes += entry.length();
        if (entry.starts_with("SOCKET_TAKEOVER="sv)) {
            if (takeover)
                return Error::from_string_literal("Repeated helper takeover environment");
            takeover = true;
            takeover_entry = entry;
        }

        launch.environment.push_back(TRY(wide(entry)));
    }

    if (!takeover)
        return Error::from_string_literal("Missing private helper transport environment");

    // The SDK's two transport HANDLE values must survive unchanged. Unlike
    // stdio, duplicating them would invalidate SOCKET_TAKEOVER in the child.
    std::vector<WindowsSandbox::Handle> duplicates;
    std::vector<HANDLE> changed;

    struct Restore {
        std::vector<HANDLE>& handles;

        bool clear()
        {
            DWORD first_error = ERROR_SUCCESS;
            for (auto& handle : handles) {
                if (!handle)
                    continue;
                if (SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0))
                    handle = nullptr;
                else if (first_error == ERROR_SUCCESS)
                    first_error = GetLastError();
            }

            if (first_error != ERROR_SUCCESS)
                SetLastError(first_error);
            return first_error == ERROR_SUCCESS;
        }

        ~Restore()
        {
            (void)clear();
        }
    };

    auto& owner = registry();
    std::lock_guard lock(owner.lock);
    Restore restore { changed };
    if (owner.leases.size() >= 16)
        return Error::from_string_literal("Windows helper process budget exceeded");
    auto duplicate = [&](HANDLE source, bool inherit = true) -> ErrorOr<HANDLE> {
        HANDLE result = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &result, 0, inherit,
                DUPLICATE_SAME_ACCESS))
            return Error::from_windows_error();
        duplicates.emplace_back(result);
        return result;
    };

    SECURITY_ATTRIBUTES attributes { sizeof(attributes), nullptr, TRUE };
    WindowsSandbox::Handle input(CreateFileW(L"NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr));
    WindowsSandbox::Handle sink(CreateFileW(L"NUL", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr));
    if (!input || !sink)
        return Error::from_windows_error();
    launch.input = input.get();
    auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    auto error_stream = GetStdHandle(STD_ERROR_HANDLE);
    launch.output = TRY(duplicate(output && output != INVALID_HANDLE_VALUE ? output : sink.get()));
    launch.error = TRY(duplicate(
        error_stream && error_stream != INVALID_HANDLE_VALUE ? error_stream : sink.get()));
    std::vector<int> transport_fds;
    bool redirected_output = false;
    bool redirected_error = false;
    for (auto const& action : options.file_actions) {
        TRY(action.visit(
            [&](Core::FileAction::DupFd const& value) -> ErrorOr<void> {
                auto handle = to_handle(value.write_fd);
                if (value.fd == STDOUT_FILENO && !redirected_output) {
                    launch.output = TRY(duplicate(handle));
                    redirected_output = true;
                } else if (value.fd == STDERR_FILENO && !redirected_error) {
                    launch.error = TRY(duplicate(handle));
                    redirected_error = true;
                } else {
                    if (value.fd != value.write_fd || launch.inherited.size() >= 2)
                        return Error::from_string_literal("Unexpected helper file action");
                    if (GetFileType(handle) != FILE_TYPE_PIPE)
                        return Error::from_string_literal(
                            "Helper transport must be an actual pipe");
                    DWORD flags = 0;
                    if (!GetHandleInformation(handle, &flags))
                        return Error::from_windows_error();
                    if (!(flags & HANDLE_FLAG_INHERIT)) {
                        if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
                            return Error::from_windows_error();
                        changed.push_back(handle);
                    }

                    launch.inherited.push_back(handle);
                    transport_fds.push_back(value.write_fd);
                }

                return {};
            },
            [](auto const&) -> ErrorOr<void> {
                return Error::from_string_literal("Unsupported Windows helper file action");
            }));
    }

    if (launch.inherited.size() != 2 || launch.inherited[0] == launch.inherited[1])
        return Error::from_string_literal("Expected exactly two private helper transports");
    if (takeover_entry
        != ByteString::formatted(
            "SOCKET_TAKEOVER=ipc:{};handle-broker:{}", transport_fds[0], transport_fds[1]))
        return Error::from_string_literal("Private helper transport provenance mismatch");
    for (auto stream : { launch.input, launch.output, launch.error }) {
        auto type = GetFileType(stream);
        if (type != FILE_TYPE_PIPE && type != FILE_TYPE_DISK && type != FILE_TYPE_CHAR)
            return Error::from_string_literal(
                "Helper standard streams require actual data handles");
    }

    launch.inherited.insert(launch.inherited.end(), { launch.input, launch.output, launch.error });
    auto lease = std::make_unique<Lease>();
    WindowsSandbox::Failure error;
    lease->profile = WindowsSandbox::Profile::create(error);
    if (!lease->profile)
        return win_error(error);
    lease->resources = Resources::acquire(root, lease->profile->sid(), error);
    if (!lease->resources)
        return win_error(error);
    lease->child = WindowsSandbox::Child::create(lease->profile, launch, error);
    if (!lease->child)
        return win_error(error);
    // The child is still suspended. Refuse admission if restoring the caller's
    // original inheritance bits fails, rather than report a successful launch.
    if (!restore.clear())
        return Error::from_windows_error();
    WindowsSandbox::Handle sdk_handle(TRY(duplicate(lease->child->process(), false)));
    // Remove the temporary duplicate owner before returning the SDK's own handle.
    duplicates.back().release();
    if (!lease->child->resume(error))
        return win_error(error);
    auto handle = sdk_handle.get();
    owner.leases.emplace(handle, std::move(lease));
    return sdk_handle.release();
}

ErrorOr<void> verify_current()
{
    WindowsSandbox::Failure error;
    if (!WindowsSandbox::verify_token(GetCurrentProcess(), nullptr, error))
        return win_error(error);
    // Additional child-side containment sanity only. The exact freshly created
    // Job identity was verified by the parent before ResumeThread; no inherited
    // Job HANDLE may keep that Job alive after broker death.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION policy {};
    if (!QueryInformationJobObject(
            nullptr, JobObjectExtendedLimitInformation, &policy, sizeof(policy), nullptr))
        return Error::from_windows_error();
    constexpr DWORD required = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    if ((policy.BasicLimitInformation.LimitFlags & required) != required
        || policy.BasicLimitInformation.ActiveProcessLimit != 1)
        return Error::from_string_literal("Renderer lacks its restrictive native helper job");
    return {};
}

bool release(void* handle)
{
    std::unique_ptr<Lease> lease;
    {
        auto& owner = registry();
        std::lock_guard lock(owner.lock);
        auto entry = owner.leases.find(handle);
        if (entry == owner.leases.end())
            return false;
        lease = std::move(entry->second);
        owner.leases.erase(entry);
    }

    return true;
}
}
