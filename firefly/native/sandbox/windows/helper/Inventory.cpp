/** Bound and hash the exact staged bytes before changing any resource ACL. */
#include "Inventory.h"
#include <algorithm>
#include <bcrypt.h>
#include <charconv>
#include <filesystem>

namespace Firefly::WindowsHelpers {
namespace {
    bool invalid(WindowsSandbox::Failure& error, char const* operation)
    {
        error = { operation, ERROR_INVALID_DATA };
        return false;
    }

    bool hash_text(std::string const& text)
    {
        return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char value) {
            return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
        });
    }
}

std::unique_ptr<Inventory> Inventory::read(HANDLE handle, WindowsSandbox::Failure& error)
{
    LARGE_INTEGER length {};
    if (!GetFileSizeEx(handle, &length) || length.QuadPart <= 0
        || length.QuadPart > 4 * 1024 * 1024) {
        invalid(error, "helper inventory size");
        return {};
    }

    std::string data(static_cast<size_t>(length.QuadPart), '\0');
    DWORD count = 0;
    if (!ReadFile(handle, data.data(), static_cast<DWORD>(data.size()), &count, nullptr)
        || count != data.size() || data.find('\0') != std::string::npos) {
        invalid(error, "read helper inventory");
        return {};
    }

    std::string header = "FWR1 " FIREFLY_WINDOWS_HELPER_REVISION "\n";
    if (!data.starts_with(header) || data.back() != '\n') {
        invalid(error, "helper inventory revision");
        return {};
    }

    auto result = std::make_unique<Inventory>();
    size_t offset = header.size();
    unsigned long long total = 0;
    bool resources = false;
    while (offset < data.size()) {
        auto end = data.find('\n', offset);
        auto line = data.substr(offset, end - offset);
        offset = end + 1;
        auto separator = line.find(' ', 65);
        if (line.size() < 68 || line[64] != ' ' || separator == std::string::npos
            || !hash_text(line.substr(0, 64)) || result->files.size() >= 16384) {
            invalid(error, "helper inventory record");
            return {};
        }

        unsigned long long size = 0;
        auto parsed = std::from_chars(line.data() + 65, line.data() + separator, size);
        if (parsed.ec != std::errc {} || parsed.ptr != line.data() + separator
            || size > 512ull * 1024 * 1024 || total + size > 2ull * 1024 * 1024 * 1024) {
            invalid(error, "helper inventory byte budget");
            return {};
        }

        total += size;
        auto text = line.substr(separator + 1);
        if (text.empty() || text.size() > 32768
            || text.find_first_of("\\:*?\"<>|\r") != std::string::npos
            || std::any_of(
                text.begin(), text.end(), [](unsigned char value) { return value < 32; })) {
            invalid(error, "helper inventory path");
            return {};
        }

        int units = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!units) {
            invalid(error, "helper inventory UTF-8");
            return {};
        }

        std::wstring relative(units, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), relative.data(), units);
        auto path = std::filesystem::path(relative);
        if (path.is_absolute() || path.generic_wstring() != relative
            || std::any_of(path.begin(), path.end(),
                [](auto const& component) { return component == L"." || component == L".."; })
            || path.filename() == L".DS_Store"
            || !(relative.starts_with(L"bin/") || relative.starts_with(L"share/Lagom/"))) {
            invalid(error, "helper inventory subtree");
            return {};
        }

        if (!result->files.emplace(relative, Record { line.substr(0, 64), size }).second) {
            invalid(error, "duplicate helper inventory file");
            return {};
        }

        resources |= relative.starts_with(L"share/Lagom/");
        for (auto parent = path.parent_path(); !parent.empty(); parent = parent.parent_path())
            result->directories.insert(parent.generic_wstring());
    }

    if (!resources || !result->files.contains(L"bin/WebContent.exe")) {
        invalid(error, "missing helper inventory resources");
        return {};
    }

    return result;
}

bool Inventory::verify(HANDLE handle, Record const& record, WindowsSandbox::Failure& error)
{
    LARGE_INTEGER size {};
    if (!GetFileSizeEx(handle, &size)
        || static_cast<unsigned long long>(size.QuadPart) != record.size)
        return invalid(error, "helper resource byte count");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    struct Cleanup {
        BCRYPT_ALG_HANDLE& algorithm;
        BCRYPT_HASH_HANDLE& hash;

        ~Cleanup()
        {
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    } cleanup { algorithm, hash };

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0
        || BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
        return invalid(error, "create helper resource SHA256");
    unsigned char buffer[65536];
    unsigned long long total = 0;
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(handle, buffer, sizeof(buffer), &count, nullptr))
            return invalid(error, "read immutable helper resource");
        if (!count)
            break;
        total += count;
        if (total > record.size || BCryptHashData(hash, buffer, count, 0) < 0)
            return invalid(error, "hash helper resource");
    }

    unsigned char bytes[32];
    if (total != record.size || BCryptFinishHash(hash, bytes, sizeof(bytes), 0) < 0)
        return invalid(error, "finish helper resource SHA256");
    std::string actual;
    for (auto byte : bytes) {
        actual.push_back("0123456789abcdef"[byte >> 4]);
        actual.push_back("0123456789abcdef"[byte & 15]);
    }

    return actual == record.hash || invalid(error, "helper resource seal mismatch");
}
}
