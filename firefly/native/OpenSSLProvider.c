/* The full pinned SSL/Crypto archives provide this library's public API.
 * Keep an ordinary translation unit so CMake owns linking and install names. */
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR != 3
#    error Review the Firefly provider before changing OpenSSL's major ABI
#endif
