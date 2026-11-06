/**
 * Builds one explicit OpenSSL provider from pinned inputs when the size profile selects it.
 * The standalone Mite dependency graph remains independent.
 */

/* The full pinned SSL/Crypto archives provide this library's public API.
 * Keep an ordinary translation unit so CMake owns linking and install names. */
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR != 3
#error Review the Firefly provider before changing OpenSSL's major ABI
#endif
