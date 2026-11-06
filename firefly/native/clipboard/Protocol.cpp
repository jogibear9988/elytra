/** Decode a bounded one-item clipboard record without retaining VM or OS provider objects. */
#include "Protocol.h"
#include <QJsonArray>
#include <QSet>
#include <utility>

namespace Firefly::ClipboardProtocol {
QJsonObject failure(char const* code, char const* message)
{
    return { { "error", QJsonObject { { "code", code }, { "message", message } } } };
}

bool supported_mime(QString const& mime)
{
    return mime == "text/plain" || mime == "text/html" || mime == "text/rtf";
}

PreparedWrite prepare_write(QJsonObject const& request)
{
    auto fail = [](char const* code, char const* message) {
        return PreparedWrite { {}, failure(code, message) };
    };

    if (request["op"] != "write" || !request["items"].isArray())
        return fail("ERR_INVALID_ARG_VALUE", "Invalid clipboard operation");
    auto items = request["items"].toArray();
    if (items.size() > 1)
        return fail("ERR_FIREFLY_UNSUPPORTED_API", "Multiple clipboard items are not implemented");
    if (items.isEmpty())
        return {};
    if (!items[0].isArray() || items[0].toArray().isEmpty() || items[0].toArray().size() > 3)
        return fail("ERR_INVALID_ARG_VALUE", "Invalid clipboard representations");
    PreparedWrite result;
    QSet<QString> seen;
    qsizetype size = 0;
    for (auto const& value : items[0].toArray()) {
        auto representation = value.toObject();
        auto mime = representation["format"].toString();
        if (!supported_mime(mime))
            return fail("ERR_FIREFLY_UNSUPPORTED_API", "Unsupported clipboard MIME type");
        if (seen.contains(mime) || !representation["data"].isString())
            return fail("ERR_INVALID_ARG_VALUE", "Invalid or duplicate clipboard representation");
        seen.insert(mime);
        auto encoded = representation["data"].toString().toLatin1();
        if (encoded.size() > byte_budget * 4 / 3 + 4)
            return fail("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
        auto decoded
            = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.toBase64() != encoded)
            return fail("ERR_INVALID_ARG_VALUE", "Invalid clipboard byte encoding");
        if (decoded.decoded.size() > byte_budget - size)
            return fail("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
        size += decoded.decoded.size();
        result.representations.append({ std::move(mime), std::move(decoded.decoded) });
    }

    return result;
}
}
