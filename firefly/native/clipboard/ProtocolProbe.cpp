/** Exercise the real protocol parser without creating a window or touching an OS clipboard. */
#include "Protocol.h"
#include <QJsonArray>
#include <cstdio>
#include <cstdlib>

namespace {
void check(bool valid, char const* message)
{
    if (!valid) {
        std::fprintf(stderr, "FAIL clipboard protocol: %s\n", message);
        std::exit(1);
    }
}

QJsonObject representation(QString mime, QByteArray bytes)
{
    return { { "format", mime }, { "data", QString::fromLatin1(bytes.toBase64()) } };
}

QJsonObject request(QJsonArray values)
{
    return { { "op", "write" }, { "items", QJsonArray { values } } };
}
}

int main()
{
    using namespace Firefly::ClipboardProtocol;
    QByteArray text("Grüße 🪲\nline");
    text.append('\0');
    text.append("tail");
    auto prepared = prepare_write(request({ representation("text/plain", text),
        representation("text/html", "<b>hello</b>"), representation("text/rtf", "{\\rtf1 hi}") }));
    check(prepared.valid() && prepared.representations.size() == 3, "three representations");
    check(prepared.representations[0].mime == "text/plain"
            && prepared.representations[0].bytes == text,
        "owned Unicode and embedded-NUL bytes");
    auto clear = prepare_write({ { "op", "write" }, { "items", QJsonArray {} } });
    check(clear.valid() && clear.representations.isEmpty(), "clear record");

    for (auto const& invalid :
        { QJsonObject {}, QJsonObject { { "op", "read" }, { "items", QJsonArray {} } },
            QJsonObject { { "op", "write" }, { "items", false } }, request({}),
            QJsonObject { { "op", "write" }, { "items", QJsonArray { false } } },
            request({ representation("image/png", "x") }),
            request({ representation("text/plain", "a"), representation("text/plain", "b") }),
            request({ QJsonObject { { "format", "text/plain" }, { "data", false } } }),
            request({ QJsonObject { { "format", "text/plain" }, { "data", "!!!" } } }),
            request({ QJsonObject { { "format", "text/plain" }, { "data", "Zg" } } }),
            QJsonObject {
                { "op", "write" }, { "items", QJsonArray { QJsonArray {}, QJsonArray {} } } } }) {
        auto rejected = prepare_write(invalid);
        check(!rejected.valid() && rejected.representations.isEmpty(), "atomic malformed refusal");
    }

    auto boundary
        = prepare_write(request({ representation("text/plain", QByteArray(byte_budget, 'x')) }));
    check(boundary.valid() && boundary.representations[0].bytes.size() == byte_budget,
        "exact byte budget");
    auto excess = prepare_write(request({
        representation("text/plain", QByteArray(byte_budget / 2 + 1, 'a')),
        representation("text/html", QByteArray(byte_budget / 2 + 1, 'b')),
    }));
    check(!excess.valid() && excess.representations.isEmpty()
            && excess.error["error"].toObject()["code"] == "ERR_FIREFLY_CLIPBOARD_LIMIT",
        "aggregate byte budget, no partial preparation");
    puts("PASS native clipboard wire bytes, shapes, MIME, canonical encoding and aggregate limits");
    return 0;
}
