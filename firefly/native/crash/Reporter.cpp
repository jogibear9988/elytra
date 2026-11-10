/** Reuse upstream signal-safe capture/private storage; publish immutable application metadata once.
 */
#include "Reporter.h"
#include "DirectoryPolicy.h"
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/LexicalPath.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <AK/Utf8View.h>
#include <LibCore/StandardPaths.h>
#include <LibWebView/CrashReport.h>
#include <LibWebView/CrashReportStore.h>
#include <atomic>

namespace Firefly::CrashReporting {
namespace {
    struct Configuration {
        AK_ALLOC_WITH_KMALLOC;
        ByteString directory;
        JsonObject main;
        JsonObject child;
        JsonObject extra;
        ByteString main_description;
        ByteString child_description;
    };

    std::atomic<Configuration const*> published { nullptr };
    bool attempted { false };
    bool embedded_profile { false };
    NeverDestroyed<ByteString> requested_directory;
    NeverDestroyed<AK::ThreadID> owner { AK::ThreadID::current() };

    void verify_owner()
    {
        VERIFY(owner.get() == AK::ThreadID::current());
    }

    ErrorOr<void> annotations(JsonObject const& object)
    {
        if (object.size() > 64)
            return Error::from_errno(E2BIG);
        TRY(object.try_for_each_member(
            [](String const& key, JsonValue const& value) -> ErrorOr<void> {
                auto name = key.bytes_as_string_view();
                if (name.is_empty() || name.length() > 39 || !value.is_string()
                    || value.as_string().byte_count() > 2048)
                    return Error::from_errno(EINVAL);
                for (auto ch : name)
                    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                            || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                        return Error::from_errno(EINVAL);
                return {};
            }));
        return {};
    }

    ByteString encode_metadata(JsonObject const& metadata)
    {
        // Upstream validates persisted build descriptions as printable ASCII/newlines.
        // JSON Unicode escapes preserve application annotations during next-launch recovery.
        auto json = metadata.serialized();
        StringBuilder result;
        for (auto point : Utf8View(json)) {
            if (point < 128) {
                result.append(static_cast<char>(point));
            } else if (point <= 0xffff) {
                result.appendff("\\u{:04x}", point);
            } else {
                point -= 0x10000;
                result.appendff(
                    "\\u{:04x}\\u{:04x}", 0xd800 + (point >> 10), 0xdc00 + (point & 0x3ff));
            }
        }

        return result.to_byte_string();
    }

    ErrorOr<NonnullOwnPtr<Configuration>> parse(StringView text)
    {
        if (text.length() > 16 * 1024)
            return Error::from_errno(E2BIG);
        auto parsed = TRY(JsonValue::from_string(text));
        if (!parsed.is_object())
            return Error::from_errno(EINVAL);
        auto const& object = parsed.as_object();
        auto product = object.get_string("productName"sv);
        auto company = object.get_string("companyName"sv);
        auto extra = object.get_object("extra"sv);
        auto global = object.get_object("globalExtra"sv);
        if (object.size() != 4 || !product.has_value() || !company.has_value() || !extra.has_value()
            || !global.has_value() || product->byte_count() > 256 || company->byte_count() > 256)
            return Error::from_errno(EINVAL);
        TRY(annotations(*extra));
        TRY(annotations(*global));
        auto config = TRY(try_make<Configuration>());
        config->directory = directory();
        config->extra = *extra;
        auto combined = *extra;
        global->for_each_member(
            [&](auto const& key, auto const& value) { combined.set(key, value); });
        config->main.set("productName"sv, *product);
        config->main.set("companyName"sv, *company);
        config->main.set("annotations"sv, move(combined));
        config->child.set("productName"sv, *product);
        config->child.set("companyName"sv, *company);
        config->child.set("annotations"sv, *global);
        config->main_description = encode_metadata(config->main);
        config->child_description = encode_metadata(config->child);
        auto main_size = config->main_description.length();
        auto child_size = config->child_description.length();
        if (main_size > 8192 || child_size > 8192
            || WebView::CrashReport::describe_current_build(WebView::ProcessType::Browser).length()
                    + main_size + 64
                > 16 * 1024)
            return Error::from_errno(E2BIG);
        return config;
    }
}

void begin_embedded_profile()
{
    verify_owner();
    embedded_profile = true;
}

bool owns_embedded_profile()
{
    verify_owner();
    return embedded_profile;
}

ByteString directory()
{
    verify_owner();
    if (auto const* config = published.load(std::memory_order_acquire))
        return config->directory;
    if (requested_directory.get().is_empty())
        requested_directory.get() = LexicalPath::join(
            Core::StandardPaths::user_data_directory(), "Firefly"sv, "CrashReports"sv)
                                        .string();
    return requested_directory.get();
}

ErrorOr<void> set_directory(StringView path)
{
    verify_owner();
    if (attempted)
        return Error::from_errno(EINVAL);
    auto error = validate_report_directory(
        std::string_view(path.characters_without_null_termination(), path.length()));
    if (error) {
        // Windows system-category values are Win32 codes, not errno. Translate
        // only when the standard library provides a portable generic condition.
        auto condition = error.default_error_condition();
        if (condition.category() == std::generic_category())
            return Error::from_errno(condition.value());
        return Error::from_string_literal("Cannot inspect crash report directory");
    }

    requested_directory.get() = ByteString(path);
    return {};
}

ErrorOr<void> start(StringView text)
{
    verify_owner();
    if (attempted || !WebView::CrashReport::is_supported())
        return Error::from_errno(ENOTSUP);
    auto config = TRY(parse(text));
    // A failed native initialization may have installed signal state: require restart,
    // never destroy its descriptor or pretend that resetting the JS API can undo it.
    attempted = true;
    auto path = config->directory;
    auto* immutable = config.leak_ptr();
    published.store(immutable, std::memory_order_release);
    return WebView::CrashReportStore(path).initialize_browser_crash_handler();
}

ByteString parameters()
{
    verify_owner();
    auto const* config = published.load(std::memory_order_acquire);
    return config ? config->extra.serialized().to_byte_string() : ByteString("{}");
}

Optional<ByteString> directory_override()
{
    auto const* config = published.load(std::memory_order_acquire);
    return config ? Optional<ByteString> { config->directory } : Optional<ByteString> {};
}

void append_description(StringBuilder& builder, WebView::ProcessType process)
{
    auto const* config = published.load(std::memory_order_acquire);
    if (!config)
        return;
    auto const& metadata = process == WebView::ProcessType::Browser ? config->main_description
                                                                    : config->child_description;
    builder.appendff("Firefly application: {}\n", metadata);
}

void append_saved_description(StringBuilder& builder, WebView::ProcessType process)
{
    // A recovered main record retains the metadata of the crashed launch, not this one.
    // Helpers started before start() have no app snapshot; their surviving parent can add
    // its frozen global fields when saving the report. Main-only extra never goes there.
    if (process != WebView::ProcessType::Browser
        && !builder.string_view().contains("\nFirefly application: "sv))
        append_description(builder, process);
}
}
