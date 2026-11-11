/** Versioned AES-256-GCM envelopes, with keys owned exclusively by the OS keychain.
 * Production macOS uses the default OS keychain. Legacy Security UI suppression
 * is process-wide: our short serialized scopes restore its previous setting on
 * every path. Concurrent credential consumers can temporarily observe UI denial.
 */
#include "Backend.h"
#include "../../../mite/runtime/platform/WindowsSockets.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace Firefly::SafeStorage {
namespace {
    constexpr std::array<uint8_t, 8> header { 'F', 'F', 'S', 'S', 1, 0, 0, 0 };
    constexpr size_t nonce_size = 12;
    constexpr size_t tag_size = 16;
    constexpr size_t overhead = header.size() + nonce_size + tag_size;

#ifdef __APPLE__
    struct Key {
        std::array<uint8_t, 32> bytes {};

        ~Key()
        {
            OPENSSL_cleanse(bytes.data(), bytes.size());
        }
    };

    using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

    bool identity(std::string const& profile, std::array<uint8_t, 32>& digest)
    {
        if (profile.empty() || profile.size() > 16384 || profile.find('\0') != std::string::npos)
            return false;
        unsigned int length = 0;
        return EVP_Digest(
                   profile.data(), profile.size(), digest.data(), &length, EVP_sha256(), nullptr)
            == 1
            && length == digest.size();
    }

    template <typename T> class CF {
    public:
        explicit CF(T value = nullptr)
            : value(value)
        {
        }

        ~CF()
        {
            if (value)
                CFRelease(value);
        }

        CF(CF const&) = delete;
        CF& operator=(CF const&) = delete;
        T value;
    };

    bool unlocked(SecKeychainRef keychain)
    {
        SecKeychainStatus status = 0;
        return SecKeychainGetStatus(keychain, &status) == errSecSuccess
            && (status & kSecUnlockStateStatus);
    }

    std::string account(std::array<uint8_t, 32> const& digest)
    {
        constexpr char hex[] = "0123456789abcdef";
        std::string value;
        value.reserve(64);
        for (auto byte : digest) {
            value.push_back(hex[byte >> 4]);
            value.push_back(hex[byte & 15]);
        }

        return value;
    }

    std::mutex keychain_mutex;

    // Security's legacy interaction switch is process-wide, not thread-local.
    // Serialize our scopes and restore the exact prior value, including errors.
    class NoInteraction {
    public:
        NoInteraction()
            : m_lock(keychain_mutex)
        {
            if (SecKeychainGetUserInteractionAllowed(&m_previous) != errSecSuccess)
                return;
            m_saved = true;
            m_active = SecKeychainSetUserInteractionAllowed(false) == errSecSuccess;
        }

        ~NoInteraction()
        {
            if (m_saved)
                SecKeychainSetUserInteractionAllowed(m_previous);
        }

        bool active() const
        {
            return m_active;
        }

        bool restore()
        {
            if (!m_saved)
                return false;
            bool active = m_active;
            m_active = false;
            if (SecKeychainSetUserInteractionAllowed(m_previous) != errSecSuccess)
                return false;
            m_saved = false;
            return active;
        }

    private:
        std::unique_lock<std::mutex> m_lock;
        Boolean m_previous {};
        bool m_saved {}, m_active {};
    };

    SecKeychainRef selected_keychain(SecKeychainRef explicit_keychain)
    {
        if (explicit_keychain) {
            CFRetain(explicit_keychain);
            return explicit_keychain;
        }

        SecKeychainRef result = nullptr;
        if (SecKeychainCopyDefault(&result) != errSecSuccess)
            return nullptr;
        return result;
    }

    void base_query(
        CFMutableDictionaryRef query, CFStringRef account, SecKeychainRef keychain, bool add)
    {
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, CFSTR("org.elytra.firefly.safe-storage.v1"));
        CFDictionarySetValue(query, kSecAttrAccount, account);
        CFDictionarySetValue(query, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
        if (add)
            CFDictionarySetValue(query, kSecUseKeychain, keychain);
        else {
            void const* values[] = { keychain };
            CF<CFArrayRef> list(CFArrayCreate(nullptr, values, 1, &kCFTypeArrayCallBacks));
            CFDictionarySetValue(query, kSecMatchSearchList, list.value);
        }
    }

    OSStatus read_key(CFStringRef account, SecKeychainRef keychain, Key& key)
    {
        CF<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
            nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        base_query(query.value, account, keychain, false);
        CFDictionarySetValue(query.value, kSecReturnData, kCFBooleanTrue);
        CFTypeRef result = nullptr;
        auto status = SecItemCopyMatching(query.value, &result);
        CF<CFTypeRef> data(result);
        if (status != errSecSuccess)
            return status;
        if (!result || CFGetTypeID(result) != CFDataGetTypeID()
            || CFDataGetLength(static_cast<CFDataRef>(result))
                != static_cast<CFIndex>(key.bytes.size()))
            return errSecDecode;
        memcpy(
            key.bytes.data(), CFDataGetBytePtr(static_cast<CFDataRef>(result)), key.bytes.size());
        return errSecSuccess;
    }

    Status load_selected_key(
        std::array<uint8_t, 32> const& digest, SecKeychainRef keychain, bool create, Key& key)
    {
        // Never unlock the selected keychain implicitly, even for initial creation.
        if (keychain && !unlocked(keychain))
            return Status::Unavailable;
        auto name = account(digest);
        CF<CFStringRef> name_ref(
            CFStringCreateWithBytes(nullptr, reinterpret_cast<UInt8 const*>(name.data()),
                name.size(), kCFStringEncodingUTF8, false));
        auto status = read_key(name_ref.value, keychain, key);
        if (status == errSecSuccess)
            return Status::Ok;
        if (status != errSecItemNotFound || !create)
            return status == errSecInteractionNotAllowed || status == errSecNotAvailable
                    || status == errSecAuthFailed
                ? Status::Unavailable
                : Status::Keychain;
        if (RAND_bytes(key.bytes.data(), key.bytes.size()) != 1)
            return Status::Crypto;
        CF<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
            nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        base_query(query.value, name_ref.value, keychain, true);
        // Borrow the cleansed key buffer instead of creating another native secret copy.
        CF<CFDataRef> data(CFDataCreateWithBytesNoCopy(
            nullptr, key.bytes.data(), key.bytes.size(), kCFAllocatorNull));
        CFDictionarySetValue(query.value, kSecValueData, data.value);
        status = SecItemAdd(query.value, nullptr);
        if (status == errSecDuplicateItem) {
            // A concurrent first writer won; never encrypt with our discarded candidate.
            OPENSSL_cleanse(key.bytes.data(), key.bytes.size());
            status = read_key(name_ref.value, keychain, key);
        }

        if (status == errSecSuccess)
            return Status::Ok;
        return status == errSecInteractionNotAllowed || status == errSecNotAvailable
                || status == errSecAuthFailed
            ? Status::Unavailable
            : Status::Keychain;
    }

    Status load_key(std::array<uint8_t, 32> const& digest, SecKeychainRef explicit_keychain,
        bool create, Key& key)
    {
        NoInteraction interaction;
        if (!interaction.active()) {
            interaction.restore();
            return Status::Unavailable;
        }

        CF<SecKeychainRef> keychain(selected_keychain(explicit_keychain));
        auto result = keychain.value ? load_selected_key(digest, keychain.value, create, key)
                                     : Status::Unavailable;
        if (!interaction.restore())
            return Status::Unavailable;
        return result;
    }

#endif
}

Backend::Backend(std::string profile)
    : m_profile(std::move(profile))
{
}
#ifdef __APPLE__
Backend::Backend(std::string profile, SecKeychainRef keychain)
    : m_profile(std::move(profile))
    , m_keychain(keychain)
{
    if (m_keychain)
        CFRetain(m_keychain);
}
#endif
Backend::~Backend()
{
#ifdef __APPLE__
    if (m_keychain)
        CFRelease(m_keychain);
#endif
}

bool Backend::available() const
{
#ifdef __APPLE__
    std::array<uint8_t, 32> digest {};
    if (!identity(m_profile, digest))
        return false;
    NoInteraction interaction;
    if (!interaction.active()) {
        interaction.restore();
        return false;
    }

    CF<SecKeychainRef> keychain(selected_keychain(m_keychain));
    SecKeychainStatus flags = 0;
    bool available = false;
    if (keychain.value && SecKeychainGetStatus(keychain.value, &flags) == errSecSuccess
        && (flags & kSecUnlockStateStatus) && (flags & kSecReadPermStatus)) {
        auto name = account(digest);
        CF<CFStringRef> name_ref(
            CFStringCreateWithBytes(nullptr, reinterpret_cast<UInt8 const*>(name.data()),
                name.size(), kCFStringEncodingUTF8, false));
        Key key;
        auto status = read_key(name_ref.value, keychain.value, key);
        available = status == errSecSuccess
            || (status == errSecItemNotFound && (flags & kSecWritePermStatus));
    }

    return interaction.restore() && available;
#else
    return false;
#endif
}

Result Backend::encrypt(std::span<uint8_t const> plaintext) const
{
    if (plaintext.size() > plaintext_limit)
        return { Status::Limit, {} };
#ifndef __APPLE__
    return { Status::Unavailable, {} };
#else
    std::array<uint8_t, 32> digest {};
    if (!identity(m_profile, digest))
        return { Status::InvalidData, {} };
    Key key;
    auto status = load_key(digest, m_keychain, true, key);
    if (status != Status::Ok)
        return { status, {} };
    std::vector<uint8_t> output(plaintext.size() + overhead);
    std::copy(header.begin(), header.end(), output.begin());
    auto* nonce = output.data() + header.size();
    auto* ciphertext = nonce + nonce_size;
    Context context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int length = 0, written = 0;
    if (!context || RAND_bytes(nonce, nonce_size) != 1
        || EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, key.bytes.data(), nonce)
            != 1
        || EVP_EncryptUpdate(context.get(), nullptr, &length, header.data(), header.size()) != 1
        || EVP_EncryptUpdate(context.get(), nullptr, &length, digest.data(), digest.size()) != 1
        || EVP_EncryptUpdate(
               context.get(), ciphertext, &written, plaintext.data(), plaintext.size())
            != 1
        || EVP_EncryptFinal_ex(context.get(), ciphertext + written, &length) != 1
        || static_cast<size_t>(written + length) != plaintext.size()
        || EVP_CIPHER_CTX_ctrl(
               context.get(), EVP_CTRL_GCM_GET_TAG, tag_size, ciphertext + plaintext.size())
            != 1)
        return { Status::Crypto, {} };
    return { Status::Ok, std::move(output) };
#endif
}

Result Backend::decrypt(std::span<uint8_t const> encrypted) const
{
    if (encrypted.size() > plaintext_limit + overhead)
        return { Status::Limit, {} };
    if (encrypted.size() < overhead || !std::equal(header.begin(), header.end(), encrypted.begin()))
        return { Status::InvalidData, {} };
#ifndef __APPLE__
    return { Status::Unavailable, {} };
#else
    std::array<uint8_t, 32> digest {};
    if (!identity(m_profile, digest))
        return { Status::InvalidData, {} };
    Key key;
    auto status = load_key(digest, m_keychain, false, key);
    if (status != Status::Ok)
        return { status, {} };
    auto size = encrypted.size() - overhead;
    auto* nonce = encrypted.data() + header.size();
    auto* ciphertext = nonce + nonce_size;
    // Allocate a byte even for empty strings: EVP's final output pointer remains valid.
    std::vector<uint8_t> plaintext(std::max(size, size_t { 1 }));
    Context context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int length = 0, written = 0;
    bool valid = context
        && EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, key.bytes.data(), nonce)
            == 1
        && EVP_DecryptUpdate(context.get(), nullptr, &length, header.data(), header.size()) == 1
        && EVP_DecryptUpdate(context.get(), nullptr, &length, digest.data(), digest.size()) == 1
        && EVP_DecryptUpdate(context.get(), plaintext.data(), &written, ciphertext, size) == 1
        && EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, tag_size,
               const_cast<uint8_t*>(ciphertext + size))
            == 1
        && EVP_DecryptFinal_ex(context.get(), plaintext.data() + written, &length) == 1
        && static_cast<size_t>(written + length) == size;
    if (!valid) {
        OPENSSL_cleanse(plaintext.data(), plaintext.size());
        return { Status::InvalidData, {} };
    }

    plaintext.resize(size);
    return { Status::Ok, std::move(plaintext) };
#endif
}
}
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
