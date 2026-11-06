/** Copy byte snapshots at the OS boundary; never retain VM objects or promised providers. */
#include "ClipboardMac.h"
#include "Clipboard.h"
#include <AppKit/AppKit.h>
#include <QJsonArray>

namespace Firefly {
namespace {
    constexpr qsizetype budget = 8 * 1024 * 1024;

    QJsonObject failure(char const* code, char const* message)
    {
        return { { "error", QJsonObject { { "code", code }, { "message", message } } } };
    }

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

        if (op != "write" || !request["items"].isArray())
            return failure("ERR_INVALID_ARG_VALUE", "Invalid clipboard operation");
        auto items = request["items"].toArray();
        if (items.size() > 1)
            return failure(
                "ERR_FIREFLY_UNSUPPORTED_API", "Multiple clipboard items are not implemented");
        if (items.isEmpty()) {
            [board clearContents];
            return {};
        }

        if (!items[0].isArray() || items[0].toArray().isEmpty() || items[0].toArray().size() > 3)
            return failure("ERR_INVALID_ARG_VALUE", "Invalid clipboard representations");
        auto* prepared = [[[NSPasteboardItem alloc] init] autorelease];
        qsizetype size = 0;
        for (auto const& value : items[0].toArray()) {
            auto representation = value.toObject();
            auto* type = native_type(representation["format"].toString());
            if (!type)
                return failure("ERR_FIREFLY_UNSUPPORTED_API", "Unsupported clipboard MIME type");
            if ([prepared.types containsObject:type] || !representation["data"].isString())
                return failure(
                    "ERR_INVALID_ARG_VALUE", "Invalid or duplicate clipboard representation");
            auto encoded = representation["data"].toString().toLatin1();
            if (encoded.size() > (budget * 4 / 3 + 4))
                return failure("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
            auto decoded
                = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded || decoded.decoded.toBase64() != encoded)
                return failure("ERR_INVALID_ARG_VALUE", "Invalid clipboard byte encoding");
            if (decoded.decoded.size() > budget - size)
                return failure("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
            size += decoded.decoded.size();
            auto* bytes = [NSData dataWithBytes:decoded.decoded.constData()
                                         length:decoded.decoded.size()];
            if (![prepared setData:bytes forType:type])
                return failure(
                    "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "Cannot prepare clipboard representation");
        }
        // Resolve/validate every representation before acquiring clipboard ownership.
        [board clearContents];
        if (![board writeObjects:@[ prepared ]])
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
