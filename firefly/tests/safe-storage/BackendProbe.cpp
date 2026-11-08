/** Exercise actual OS keys in a disposable explicit keychain; never use the login keychain. */
#include "native/safe-storage/Backend.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <openssl/crypto.h>
#include <unistd.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Firefly::SafeStorage;

namespace {
void check(bool value, char const* message);
#ifdef __APPLE__
void check_ui(Boolean expected)
{
    Boolean actual = false;
    check(SecKeychainGetUserInteractionAllowed(&actual) == errSecSuccess && actual == expected,
        "UI interaction policy restored after operation");
}

CFMutableDictionaryRef query_for(SecKeychainRef keychain)
{
    auto query = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    void const* refs[] = { keychain };
    auto list = CFArrayCreate(nullptr, refs, 1, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR("org.elytra.firefly.safe-storage.v1"));
    CFDictionarySetValue(query, kSecMatchSearchList, list);
    CFRelease(list);
    return query;
}

size_t key_count(SecKeychainRef keychain)
{
    auto query = query_for(keychain);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    CFTypeRef result = nullptr;
    auto status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status == errSecItemNotFound)
        return 0;
    check(status == errSecSuccess && result && CFGetTypeID(result) == CFArrayGetTypeID(),
        "read isolated key metadata");
    auto count = CFArrayGetCount(static_cast<CFArrayRef>(result));
    CFRelease(result);
    return static_cast<size_t>(count);
}

void corrupt_key(SecKeychainRef keychain)
{
    auto query = query_for(keychain);
    auto changes = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    uint8_t invalid[] = { 1, 2, 3 };
    auto data = CFDataCreate(nullptr, invalid, sizeof(invalid));
    CFDictionarySetValue(changes, kSecValueData, data);
    auto status = SecItemUpdate(query, changes);
    CFRelease(data);
    CFRelease(changes);
    CFRelease(query);
    check(status == errSecSuccess, "inject malformed key only in disposable keychain");
}

struct TemporaryKeychain {
    static inline TemporaryKeychain* current {};
    std::string directory;
    SecKeychainRef keychain {};

    TemporaryKeychain()
    {
        current = this;
        std::array<char, 64> pattern {};
        strcpy(pattern.data(), "/tmp/firefly-safe-storage-XXXXXX");
        auto* path = mkdtemp(pattern.data());
        check(path != nullptr, "mkdtemp");
        directory = path;
        constexpr char password[] = "disposable-probe-keychain-password";
        auto status = SecKeychainCreate((directory + "/probe.keychain-db").c_str(),
            sizeof(password) - 1, password, false, nullptr, &keychain);
        check(status == errSecSuccess, "create isolated keychain");
    }

    ~TemporaryKeychain()
    {
        cleanup();
        current = nullptr;
    }

    void cleanup()
    {
        if (keychain) {
            SecKeychainDelete(keychain);
            CFRelease(keychain);
            keychain = nullptr;
        }

        std::error_code ignored;
        if (!directory.empty())
            std::filesystem::remove_all(directory, ignored);
    }
};
#endif

void check(bool value, char const* message)
{
    if (value)
        return;
    fprintf(stderr, "FAIL safe storage: %s\n", message);
#ifdef __APPLE__
    if (TemporaryKeychain::current)
        TemporaryKeychain::current->cleanup();
#endif
    std::exit(1);
}
}

int main()
{
#ifdef __APPLE__
    CFArrayRef before = nullptr;
    check(SecKeychainCopySearchList(&before) == errSecSuccess, "snapshot search list");
    Boolean ui_before = false;
    check(SecKeychainGetUserInteractionAllowed(&ui_before) == errSecSuccess, "read UI policy");
    {
        TemporaryKeychain temporary;
        Backend first("/profiles/first", temporary.keychain);
        Backend same("/profiles/first", temporary.keychain);
        Backend other("/profiles/second", temporary.keychain);
        check(first.available(), "unlocked backend available");
        check(key_count(temporary.keychain) == 0, "availability never creates key");
        check_ui(ui_before);
        std::vector<uint8_t> input { 's', 'e', 'c', 'r', 'e', 't', 0, 0xf0, 0x9f, 0x98, 0x80 };
        auto encrypted = first.encrypt(input);
        check(encrypted.status == Status::Ok, "encrypt");
        check_ui(ui_before);
        auto second = first.encrypt(input);
        check(second.status == Status::Ok && second.bytes != encrypted.bytes, "random nonce");
        check(
            std::search(encrypted.bytes.begin(), encrypted.bytes.end(), input.begin(), input.end())
                == encrypted.bytes.end(),
            "plaintext absent");
        auto recovered = same.decrypt(encrypted.bytes);
        check(recovered.status == Status::Ok && recovered.bytes == input, "reopen profile decrypt");
        check(other.decrypt(encrypted.bytes).status != Status::Ok, "profile separation");
        check(key_count(temporary.keychain) == 1, "failed decrypt never creates new profile key");
        check_ui(ui_before);
        auto empty = first.encrypt({});
        check(empty.status == Status::Ok, "encrypt empty");
        auto empty_plain = first.decrypt(empty.bytes);
        check(empty_plain.status == Status::Ok && empty_plain.bytes.empty(), "decrypt empty");
        for (size_t index : { size_t { 0 }, size_t { 4 }, size_t { 8 }, size_t { 20 },
                 encrypted.bytes.size() - 1 }) {
            auto modified = encrypted.bytes;
            modified[index] ^= 1;
            auto result = first.decrypt(modified);
            check(result.status != Status::Ok && result.bytes.empty(),
                "tamper rejection with no plaintext");
        }

        check_ui(ui_before);
        auto truncated = encrypted.bytes;
        truncated.pop_back();
        check(first.decrypt(truncated).status == Status::InvalidData, "truncation authentication");
        std::vector<uint8_t> maximum(Backend::plaintext_limit, 0xa5);
        auto maximum_cipher = first.encrypt(maximum);
        check(maximum_cipher.status == Status::Ok, "exact maximum plaintext accepted");
        auto maximum_plain = first.decrypt(maximum_cipher.bytes);
        check(maximum_plain.status == Status::Ok && maximum_plain.bytes == maximum,
            "exact maximum plaintext authenticates and roundtrips");
        std::vector<uint8_t> oversized(Backend::plaintext_limit + 1);
        check(first.encrypt(oversized).status == Status::Limit, "plaintext limit");
        oversized.resize(Backend::plaintext_limit + 37);
        check(first.decrypt(oversized).status == Status::Limit, "ciphertext limit");
        check(SecKeychainLock(temporary.keychain) == errSecSuccess, "lock isolated keychain");
        check(!first.available(), "locked unavailable");
        check(
            first.encrypt(input).status == Status::Unavailable, "locked encrypt denied without UI");
        check(first.decrypt(encrypted.bytes).status == Status::Unavailable,
            "locked decrypt denied without UI");
        check_ui(ui_before);
        constexpr char password[] = "disposable-probe-keychain-password";
        check(SecKeychainUnlock(temporary.keychain, sizeof(password) - 1, password, true)
                == errSecSuccess,
            "explicit test unlock");
        check(first.decrypt(encrypted.bytes).status == Status::Ok, "same key after unlock");
        corrupt_key(temporary.keychain);
        check(!first.available(), "malformed key unavailable");
        auto corrupted = first.decrypt(encrypted.bytes);
        check(corrupted.status == Status::Keychain && corrupted.bytes.empty(),
            "malformed key denied without plaintext");
        check(first.encrypt(input).status == Status::Keychain,
            "corrupted key never replaced silently");
        check_ui(ui_before);
        // Exercise restoration of both possible process policies, rather than
        // assuming the process entered with the platform default enabled.
        Backend policy("/profiles/policy", temporary.keychain);
        for (Boolean expected : { Boolean(false), Boolean(true) }) {
            check(SecKeychainSetUserInteractionAllowed(expected) == errSecSuccess,
                "select test-only interaction policy");
            check(policy.available(), "policy test backend available");
            check_ui(expected);
            auto protected_value = policy.encrypt(input);
            check(protected_value.status == Status::Ok, "encrypt under either prior UI policy");
            check_ui(expected);
            protected_value.bytes.back() ^= 1;
            check(policy.decrypt(protected_value.bytes).status == Status::InvalidData,
                "authenticated failure under either prior UI policy");
            check_ui(expected);
        }

        check(SecKeychainSetUserInteractionAllowed(ui_before) == errSecSuccess,
            "restore probe entry policy");
        OPENSSL_cleanse(recovered.bytes.data(), recovered.bytes.size());
        OPENSSL_cleanse(input.data(), input.size());
    }

    CFArrayRef after = nullptr;
    check(SecKeychainCopySearchList(&after) == errSecSuccess, "read final search list");
    bool unchanged = CFEqual(before, after);
    CFRelease(before);
    CFRelease(after);
    check(unchanged, "keychain search list unchanged");
    Boolean ui_after = false;
    check(SecKeychainGetUserInteractionAllowed(&ui_after) == errSecSuccess && ui_before == ui_after,
        "UI policy unchanged");
    puts("PASS isolated native Keychain/AES-GCM persistence, profile separation, tamper, "
         "limits and locked denial; UI/search list unchanged");
#else
    Backend backend("/profiles/unavailable");
    check(!backend.available(), "unsupported backend unavailable");
    check(backend.encrypt({}).status == Status::Unavailable, "no plaintext fallback");
    puts("PASS unsupported native safe storage fails unavailable");
#endif
    return 0;
}
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
