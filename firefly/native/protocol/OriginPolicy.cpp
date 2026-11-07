/**
 * Origin identity is scheme/normalized-host/port, never an alias for file: or https:.
 * Publish a bounded process-lifetime table once: renderer worker reads cannot race
 * reconfiguration, reset, or destruction of security policy.
 */
#include "OriginPolicy.h"
#include <AK/OwnPtr.h>
#include <AK/Vector.h>
#include <LibURL/Parser.h>
#include <atomic>

namespace Firefly::Protocols {
namespace {
    struct Table {
        AK_ALLOC_WITH_KMALLOC;
        Vector<OriginScheme> schemes;
    };

    std::atomic<Table const*> policy { nullptr };

    bool valid_name(StringView name)
    {
        if (name.is_empty() || name.length() > 64 || name[0] < 'a' || name[0] > 'z')
            return false;
        for (auto ch : name) {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '+' || ch == '-'
                    || ch == '.'))
                return false;
        }

        return true;
    }

    OriginScheme const* find(StringView name)
    {
        auto const* table = policy.load(std::memory_order_acquire);
        if (!table)
            return nullptr;
        for (auto const& entry : table->schemes) {
            if (entry.name == name)
                return &entry;
        }

        return nullptr;
    }
}

ErrorOr<void> install_origin_schemes(ReadonlySpan<OriginScheme> schemes)
{
    if (schemes.size() > 32)
        return Error::from_string_literal("At most 32 custom origin schemes are supported");
    auto table = TRY(try_make<Table>());
    for (auto const& entry : schemes) {
        auto name = entry.name.bytes_as_string_view();
        if (!valid_name(name))
            return Error::from_string_literal(
                "Custom origin schemes require canonical lowercase RFC names");
        if (name.is_one_of("http"sv, "https"sv, "ws"sv, "wss"sv, "ftp"sv, "file"sv, "about"sv,
                "blob"sv, "data"sv, "javascript"sv, "resource"sv, "chrome"sv, "chrome-extension"sv,
                "devtools"sv, "webkit"sv, "moz-extension"sv, "mite"sv, "mite-host"sv, "firefly"sv))
            return Error::from_string_literal(
                "Built-in and internal origin schemes cannot be replaced");
        for (auto const& existing : table->schemes) {
            if (existing.name == name)
                return Error::from_string_literal("Duplicate custom origin scheme");
        }

        TRY(table->schemes.try_append(entry));
    }

    Table const* expected = nullptr;
    if (!policy.compare_exchange_strong(
            expected, table.ptr(), std::memory_order_acq_rel, std::memory_order_acquire))
        return Error::from_string_literal("Custom origin policy is already installed");
    // Bounded immutable process security state deliberately outlives all worker
    // threads/static destructors. There is no public reset or teardown callback.
    (void)table.leak_ptr();
    return {};
}

Optional<URL::Origin> custom_origin(URL::URL const& url)
{
    auto const* entry = find(url.scheme());
    if (!entry || !entry->standard || url.has_an_opaque_path() || url.serialized_host().is_empty())
        return {};
    // Rust's non-special URL parser retains opaque host spelling. Use the real
    // domain/IP parser for origin normalization and IDNA; invalid hosts stay opaque.
    auto host = URL::Parser::parse_host(url.serialized_host(), false);
    if (!host.has_value())
        return {};
    return URL::Origin { entry->name, *host, url.port() };
}

bool is_secure_origin(URL::Origin const& origin)
{
    if (origin.is_opaque() || !origin.scheme().has_value())
        return false;
    auto const* entry = find(origin.scheme()->bytes_as_string_view());
    return entry && entry->standard && entry->secure;
}
}
