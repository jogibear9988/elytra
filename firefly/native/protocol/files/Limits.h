/** Shared wire budgets; importing codecs must not import native filesystem ownership. */
#pragma once
#include <AK/Types.h>

namespace Firefly::Protocols {
inline constexpr size_t document_file_request_limit = 8;
inline constexpr size_t process_file_request_limit = 64;
inline constexpr size_t file_chunk_limit = 64 * 1024;
inline constexpr u64 file_size_limit = 64ull * 1024 * 1024 * 1024;
inline constexpr size_t response_head_limit = 32 * 1024;
inline constexpr size_t response_header_limit = 64;
}
