/** Copy byte snapshots at the OS boundary; never retain VM objects or promised providers. */
#include "ClipboardMac.h"
#include "Clipboard.h"
#include "Protocol.h"
#include <AppKit/AppKit.h>
#include <QJsonArray>

namespace Firefly {
namespace {
    constexpr auto budget = ClipboardProtocol::byte_budget;
    using ClipboardProtocol::failure;

    NSString* native_type(QString const& mime)
    {
        if (mime == "text/plain")
            return NSPasteboardTypeString;
        if (mime == "text/html")
            return NSPasteboardTypeHTML;
        if (mime == "text/rtf")
            return NSPasteboardTypeRTF;
        return nil;
    }

    QString mime_type(NSString* type)
    {
        if ([type isEqualToString:NSPasteboardTypeString])
            return "text/plain";
        if ([type isEqualToString:NSPasteboardTypeHTML])
            return "text/html";
        if ([type isEqualToString:NSPasteboardTypeRTF])
            return "text/rtf";
        if ([type isEqualToString:@"public.utf16-external-plain-text"])
            return "text/plain";
        return {};
    }
}

QJsonObject clipboard_request_on(NSPasteboard* board, QJsonObject const& request)
{
    @autoreleasepool {
        if (!board)
            return failure(
                "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "No native pasteboard is available");
        auto op = request["op"].toString();
        if (op == "clear") {
            [board clearContents];
            return {};
        }

        if (op == "has") {
            auto* type = native_type(request["format"].toString());
            if (!type)
                return failure("ERR_FIREFLY_UNSUPPORTED_API", "Unsupported clipboard MIME type");
            auto* types = [type isEqualToString:NSPasteboardTypeString]
                ? @[ type, @"public.utf16-external-plain-text" ]
                : @[ type ];
            return { { "has", [board availableTypeFromArray:types] != nil } };
        }

        if (op == "readText") {
            // String conversion belongs to AppKit, including external UTF-16 providers.
            auto* text = [board stringForType:NSPasteboardTypeString];
            if (!text && [board availableTypeFromArray:@[ @"public.utf16-external-plain-text" ]]) {
                auto* data = [board dataForType:@"public.utf16-external-plain-text"];
                if (data.length > budget)
                    return failure(
                        "ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
                text = [[[NSString alloc] initWithData:data
                                              encoding:NSUTF16StringEncoding] autorelease];
                if (!text)
                    return failure(
                        "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "Cannot decode clipboard text");
            }

            auto* bytes = [text dataUsingEncoding:NSUTF8StringEncoding];
            if (bytes.length > budget)
                return failure("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
            return { { "text",
                bytes ? QString::fromUtf8(static_cast<char const*>(bytes.bytes), bytes.length)
                      : QString {} } };
        }

        if (op == "read") {
            auto change = board.changeCount;
            QJsonArray result;
            qsizetype size = 0;
            for (NSPasteboardItem* item in board.pasteboardItems) {
                QJsonArray representations;
                for (NSString* type in item.types) {
                    bool utf16 = [type isEqualToString:@"public.utf16-external-plain-text"];
                    // AppKit adds this alias to our UTF-8 writes. Do not expose it twice.
                    if (utf16 && [item.types containsObject:NSPasteboardTypeString])
                        continue;
                    auto mime = mime_type(type);
                    if (mime.isEmpty())
                        return failure("ERR_FIREFLY_UNSUPPORTED_API",
                            "Clipboard contains an unsupported native format");
                    auto* data = [item dataForType:type];
                    if (data.length > static_cast<NSUInteger>(budget - size))
                        return failure(
                            "ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
                    if (utf16) {
                        auto* text =
                            [[[NSString alloc] initWithData:data
                                                   encoding:NSUTF16StringEncoding] autorelease];
                        data = [text dataUsingEncoding:NSUTF8StringEncoding];
                    }

                    if (!data)
                        return failure("ERR_FIREFLY_CLIPBOARD_UNAVAILABLE",
                            "Clipboard provider did not supply data");
                    if (data.length > static_cast<NSUInteger>(budget - size))
                        return failure(
                            "ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
                    size += data.length;
                    auto bytes = QByteArray(static_cast<char const*>(data.bytes), data.length);
                    representations.append(QJsonObject {
                        { "format", mime }, { "data", QString::fromLatin1(bytes.toBase64()) } });
                }

                if (!representations.isEmpty())
                    result.append(representations);
            }

            if (change != board.changeCount)
                return failure("ERR_FIREFLY_CLIPBOARD_CHANGED", "Clipboard changed during read");
            return { { "items", result } };
        }

        auto prepared = ClipboardProtocol::prepare_write(request);
        if (!prepared.valid())
            return prepared.error;
        if (prepared.representations.isEmpty()) {
            [board clearContents];
            return {};
        }

        auto* item = [[[NSPasteboardItem alloc] init] autorelease];
        for (auto const& representation : prepared.representations) {
            auto* type = native_type(representation.mime);
            auto const& data = representation.bytes;
            auto* bytes = [NSData dataWithBytes:data.constData() length:data.size()];
            if (![item setData:bytes forType:type])
                return failure(
                    "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "Cannot prepare clipboard representation");
        }
        // Resolve/validate every representation before acquiring clipboard ownership.
        [board clearContents];
        if (![board writeObjects:@[ item ]])
            return failure(
                "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "Cannot publish native clipboard data");
        return {};
    }
}

QJsonObject clipboard_request(QJsonObject const& request)
{
    @autoreleasepool {
        return clipboard_request_on([NSPasteboard generalPasteboard], request);
    }
}
}
