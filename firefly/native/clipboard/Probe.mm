/** Native acceptance on a unique pasteboard; never read or overwrite the general clipboard. */
#include "ClipboardMac.h"
#include <AppKit/AppKit.h>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <cstdio>

namespace {
QJsonObject representation(char const* mime, QByteArray const& bytes)
{
    return { { "format", mime }, { "data", QString::fromLatin1(bytes.toBase64()) } };
}

int check(NSPasteboard* board)
{
    using Firefly::clipboard_request_on;
    auto text = QByteArray("Grüße 🪲\nline");
    text.append('\0');
    text.append("tail");
    auto html = QByteArray("<b>hello</b>");
    auto rtf = QByteArray("{\\rtf1\\ansi hello}");
    QJsonArray values { representation("text/plain", text), representation("text/html", html),
        representation("text/rtf", rtf) };
    auto write
        = clipboard_request_on(board, { { "op", "write" }, { "items", QJsonArray { values } } });
    if (write.contains("error") || board.pasteboardItems.count != 1)
        return 1;
    auto* actual = [board dataForType:NSPasteboardTypeString];
    if (!actual || QByteArray(static_cast<char const*>(actual.bytes), actual.length) != text)
        return 2;
    auto* native_html = [board dataForType:NSPasteboardTypeHTML];
    auto* native_rtf = [board dataForType:NSPasteboardTypeRTF];
    if (QByteArray(static_cast<char const*>(native_html.bytes), native_html.length) != html
        || QByteArray(static_cast<char const*>(native_rtf.bytes), native_rtf.length) != rtf)
        return 11;
    auto read = clipboard_request_on(board, { { "op", "read" } });
    if (read["items"].toArray() != QJsonArray { values }) {
        fputs(QJsonDocument(read).toJson().constData(), stderr);
        return 3;
    }

    if (clipboard_request_on(board, { { "op", "readText" } })["text"].toString().toUtf8() != text)
        return 4;
    if (!clipboard_request_on(board, { { "op", "has" }, { "format", "text/html" } })["has"]
            .toBool())
        return 5;
    puts("PASS native private pasteboard text HTML RTF bytes and MIME metadata");

    auto unchanged = board.changeCount;
    for (auto invalid : { QJsonArray { representation("image/png", "invalid") },
             QJsonArray { representation("text/plain", "x"), representation("text/plain", "y") },
             QJsonArray { QJsonObject { { "format", "text/plain" }, { "data", "!!!" } } },
             QJsonArray { representation("text/plain", QByteArray(8 * 1024 * 1024 + 1, 'x')) } }) {
        auto result = clipboard_request_on(
            board, { { "op", "write" }, { "items", QJsonArray { invalid } } });
        if (!result.contains("error") || board.changeCount != unchanged)
            return 6;
    }

    puts("PASS native preparation rejects unsupported duplicate malformed oversized writes before "
         "ownership");

    [board clearContents];
    [board setString:@"external text" forType:NSPasteboardTypeString];
    if (clipboard_request_on(board, { { "op", "readText" } })["text"] != "external text")
        return 7;
    [board clearContents];
    [board setData:[@"external Grüße" dataUsingEncoding:NSUTF16StringEncoding]
           forType:@"public.utf16-external-plain-text"];
    auto utf16 = clipboard_request_on(board, { { "op", "read" } });
    if (utf16["items"].toArray()
        != QJsonArray { QJsonArray { representation("text/plain", QByteArray("external Grüße")) } })
        return 10;
    if (clipboard_request_on(board, { { "op", "readText" } })["text"] != "external Grüße"
        || !clipboard_request_on(board, { { "op", "has" }, { "format", "text/plain" } })["has"]
            .toBool())
        return 12;
    [board clearContents];
    [board setData:[NSData data] forType:@"org.elytra.clipboard-test"];
    if (!clipboard_request_on(board, { { "op", "read" } }).contains("error"))
        return 8;
    clipboard_request_on(board, { { "op", "clear" } });
    if (clipboard_request_on(board, { { "op", "readText" } })["text"] != ""
        || !clipboard_request_on(board, { { "op", "read" } })["items"].toArray().isEmpty())
        return 9;
    puts("PASS native external writer unknown-format refusal and clear");
    return 0;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    @autoreleasepool {
        auto* board = [NSPasteboard pasteboardWithUniqueName];
        auto status = check(board);
        [board releaseGlobally];
        if (status)
            fprintf(stderr, "Native clipboard acceptance failed: %d\n", status);
        return status;
    }
}
