/** App-profile scoped OS key storage and authenticated Firefly ciphertexts; no plaintext fallback.
 */
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <Security/SecKeychain.h>
#endif

namespace Firefly::SafeStorage {
enum class Status { Ok, Unavailable, InvalidData, Limit, Crypto, Keychain };

struct Result {
    Status status;
    std::vector<uint8_t> bytes;
};

class Backend {
public:
    static constexpr size_t plaintext_limit = 1024 * 1024;
    explicit Backend(std::string profile);
#ifdef __APPLE__
    // Explicit keychain ownership is retained. For isolated native tests only;
    // never changes the user's default keychain or search list.
    Backend(std::string profile, SecKeychainRef);
#endif
    ~Backend();

    Backend(Backend const&) = delete;
    Backend& operator=(Backend const&) = delete;
    bool available() const;

    Result encrypt(std::span<uint8_t const>) const;

    Result decrypt(std::span<uint8_t const>) const;

private:
    std::string m_profile;
#ifdef __APPLE__
    SecKeychainRef m_keychain {};
#endif
};
}
