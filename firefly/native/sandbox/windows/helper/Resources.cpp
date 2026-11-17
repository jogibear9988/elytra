/** Hold no-delete-share file handles while applying and retiring only our fresh package SID ACEs.
 */
#include "Resources.h"
#include "Inventory.h"
#include <aclapi.h>
#include <algorithm>
#include <cstdio>

namespace Firefly::WindowsHelpers {
namespace {
    bool failure(WindowsSandbox::Failure& error, char const* operation, DWORD code)
    {
        error = { operation, code };
        return false;
    }

    class Transaction {
    public:
        Transaction()
            : m_mutex(CreateMutexW(nullptr, FALSE, L"Local\\elytra.firefly.helper-resource-acl.v1"))
        {
            if (!m_mutex) {
                m_error = GetLastError();
                return;
            }

            auto waited = WaitForSingleObject(m_mutex.get(), 5000);
            m_locked = waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED;
            if (!m_locked)
                m_error = waited == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
        }

        ~Transaction()
        {
            if (m_locked)
                ReleaseMutex(m_mutex.get());
        }

        bool locked() const
        {
            return m_locked;
        }

        DWORD error() const
        {
            return m_error;
        }

    private:
        WindowsSandbox::Handle m_mutex;
        bool m_locked { false };
        DWORD m_error { ERROR_SUCCESS };
    };

    // A single bounded transaction owns the named mutex for the whole grant or
    // cleanup. Never multiply the five-second wait by the resource file count.
    DWORD update(HANDLE handle, PSID sid, bool grant)
    {
        PACL previous = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        auto status = GetSecurityInfo(handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
            nullptr, &previous, nullptr, &descriptor);
        if (status != ERROR_SUCCESS)
            return status;
        if (!previous) {
            LocalFree(descriptor);
            return ERROR_INVALID_ACL;
        }

        EXPLICIT_ACCESSW entry {};
        entry.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
        entry.grfAccessMode = grant ? GRANT_ACCESS : REVOKE_ACCESS;
        entry.grfInheritance = NO_INHERITANCE;
        entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        entry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
        entry.Trustee.ptstrName = static_cast<LPWSTR>(sid);
        PACL next = nullptr;
        status = SetEntriesInAclW(1, &entry, previous, &next);
        if (status == ERROR_SUCCESS)
            status = SetSecurityInfo(
                handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, next, nullptr);
        LocalFree(next);
        LocalFree(descriptor);
        return status;
    }
}

std::unique_ptr<Resources> Resources::acquire(
    std::filesystem::path const& root, PSID sid, WindowsSandbox::Failure& error)
{
    auto result = std::unique_ptr<Resources>(new Resources);
    if (!root.is_absolute() || root.root_name().wstring().size() != 2
        || root.root_name().wstring()[1] != L':' || !IsValidSid(sid)) {
        failure(error, "runtime resource identity", ERROR_INVALID_PARAMETER);
        return {};
    }

    result->m_sid.resize(GetLengthSid(sid));
    if (!CopySid(static_cast<DWORD>(result->m_sid.size()), result->m_sid.data(), sid)) {
        failure(error, "copy runtime SID", GetLastError());
        return {};
    }

    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
        failure(error, "resource owner token", GetLastError());
        return {};
    }

    WindowsSandbox::Handle token(raw_token);
    DWORD token_size = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &token_size);
    std::vector<unsigned char> user(token_size);
    if (!token_size
        || !GetTokenInformation(token.get(), TokenUser, user.data(), token_size, &token_size)) {
        failure(error, "resource owner SID", GetLastError());
        return {};
    }

    auto user_sid = reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;

    // Do not follow a reparse point in any prefix, including above the owned root.
    auto prefix = root.root_path();
    for (auto const& component : root.relative_path()) {
        prefix /= component;
        auto attributes = GetFileAttributesW(prefix.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            failure(error, "runtime path reparse point", ERROR_ACCESS_DENIED);
            return {};
        }
    }

    std::vector<std::filesystem::path> pending { root };
    while (!pending.empty()) {
        auto path = std::move(pending.back());
        pending.pop_back();
        if (result->m_entries.size() >= 16384) {
            failure(error, "runtime resource count", ERROR_TOO_MANY_OPEN_FILES);
            return {};
        }
        // Keeping these handles denies deletion/replacement until the child is gone.
        WindowsSandbox::Handle handle(CreateFileW(path.c_str(),
            READ_CONTROL | WRITE_DAC | FILE_READ_ATTRIBUTES | FILE_READ_DATA, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
            nullptr));
        BY_HANDLE_FILE_INFORMATION information {};
        if (!handle || !GetFileInformationByHandle(handle.get(), &information)
            || information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT
            || (!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                && information.nNumberOfLinks != 1)) {
            failure(error, "open physical runtime resource", ERROR_ACCESS_DENIED);
            return {};
        }

        std::vector<wchar_t> final_name(32768);
        auto final_size = GetFinalPathNameByHandleW(handle.get(), final_name.data(),
            static_cast<DWORD>(final_name.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        auto expected_name = std::wstring(L"\\\\?\\") + path.lexically_normal().wstring();
        if (!final_size || final_size >= final_name.size()
            || _wcsicmp(final_name.data(), expected_name.c_str()) != 0) {
            failure(error, "runtime resource path identity", ERROR_ACCESS_DENIED);
            return {};
        }

        PSID owner = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        auto status = GetSecurityInfo(handle.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
            &owner, nullptr, nullptr, nullptr, &descriptor);
        bool owned = status == ERROR_SUCCESS && owner && EqualSid(owner, user_sid);
        LocalFree(descriptor);
        if (!owned) {
            failure(error, "runtime resource not owned", ERROR_ACCESS_DENIED);
            return {};
        }

        auto relative
            = path == root ? std::wstring {} : path.lexically_relative(root).generic_wstring();
        result->m_entries.push_back({ std::move(handle), std::move(relative),
            static_cast<bool>(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), false });
        if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            WIN32_FIND_DATAW data {};
            auto search = FindFirstFileW((path / L"*").c_str(), &data);
            if (search == INVALID_HANDLE_VALUE) {
                failure(error, "enumerate runtime resources", GetLastError());
                return {};
            }

            do {
                if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
                    continue;
                if (pending.size() + result->m_entries.size() >= 16384
                    || data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    FindClose(search);
                    failure(error, "runtime resource limit/reparse", ERROR_ACCESS_DENIED);
                    return {};
                }

                pending.push_back(path / data.cFileName);
            } while (FindNextFileW(search, &data));
            status = GetLastError();
            FindClose(search);
            if (status != ERROR_NO_MORE_FILES) {
                failure(error, "enumerate runtime resources", status);
                return {};
            }
        }
    }

    // Seal and every file identity/hash are checked before the first ACL write.
    Inventory* inventory = nullptr;
    std::unique_ptr<Inventory> owned_inventory;
    for (auto& entry : result->m_entries) {
        if (entry.relative == Inventory::name && !entry.directory) {
            owned_inventory = Inventory::read(entry.handle.get(), error);
            inventory = owned_inventory.get();
            break;
        }
    }

    if (!inventory) {
        failure(error, "missing/invalid sealed helper inventory", ERROR_INVALID_DATA);
        return {};
    }

    size_t matched = 0;
    for (auto& entry : result->m_entries) {
        if (entry.relative == Inventory::name
            || std::filesystem::path(entry.relative).filename() == L".DS_Store")
            continue;
        if (entry.directory) {
            if (!inventory->directories.contains(entry.relative)) {
                failure(error, "unowned helper directory", ERROR_ACCESS_DENIED);
                return {};
            }
        } else {
            auto expected = inventory->files.find(entry.relative);
            if (expected == inventory->files.end()
                || !Inventory::verify(entry.handle.get(), expected->second, error)) {
                failure(error, "unowned/changed helper file", ERROR_ACCESS_DENIED);
                return {};
            }

            ++matched;
        }
    }

    if (matched != inventory->files.size()) {
        failure(error, "missing sealed helper file", ERROR_INVALID_DATA);
        return {};
    }

    Transaction transaction;
    if (!transaction.locked()) {
        failure(error, "runtime ACL transaction", transaction.error());
        return {};
    }

    for (auto& entry : result->m_entries) {
        if (entry.relative == Inventory::name
            || std::filesystem::path(entry.relative).filename() == L".DS_Store")
            continue;
        auto status = update(entry.handle.get(), result->m_sid.data(), true);
        if (status != ERROR_SUCCESS) {
            failure(error, "grant runtime SID access", status);
            return {};
        }

        entry.granted = true;
    }

    return result;
}

Resources::~Resources()
{
    // Fresh profiles give each lease a distinct SID. Remove only its ACE, merging
    // the current ACL so concurrent leases/host changes are not overwritten.
    if (std::none_of(
            m_entries.begin(), m_entries.end(), [](auto const& entry) { return entry.granted; }))
        return;
    Transaction transaction;
    if (!transaction.locked()) {
        std::fprintf(stderr, "Firefly runtime SID cleanup lock failed: %lu\n", transaction.error());
        return;
    }

    DWORD first_error = ERROR_SUCCESS;
    for (auto& entry : m_entries) {
        if (entry.granted) {
            auto status = update(entry.handle.get(), m_sid.data(), false);
            if (first_error == ERROR_SUCCESS)
                first_error = status;
        }
    }

    if (first_error != ERROR_SUCCESS)
        std::fprintf(stderr, "Firefly runtime SID cleanup failed: %lu\n", first_error);
}
}
