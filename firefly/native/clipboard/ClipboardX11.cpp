/** Native Qt/X11 clipboard ownership and byte snapshots. All calls stay on the GUI thread. */
#include "Clipboard.h"
#include "Protocol.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QMimeData>
#include <QPointer>
#include <QScopeGuard>
#include <QThread>
#include <memory>

namespace Firefly {
namespace {
    bool transport_target(QString const& format)
    {
        // These X11 protocol targets describe transfer mechanics, not content.
        // Qt already maps STRING/UTF8_STRING/TEXT aliases to text/plain.
        return format == "TARGETS" || format == "MULTIPLE" || format == "TIMESTAMP"
            || format == "SAVE_TARGETS";
    }
}

QJsonObject clipboard_request(QJsonObject const& request)
{
    using namespace ClipboardProtocol;
    auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!application || QGuiApplication::platformName() != "xcb"
        || QThread::currentThread() != application->thread())
        return failure("ERR_FIREFLY_CLIPBOARD_UNAVAILABLE",
            "The X11 clipboard requires the native xcb GUI application thread");
    auto* board = QGuiApplication::clipboard();
    if (!board)
        return failure("ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "No native X11 clipboard is available");
    auto op = request["op"].toString();
    if (op == "clear") {
        board->clear();
        return {};
    }

    if (op == "write") {
        auto prepared = prepare_write(request);
        if (!prepared.valid())
            return prepared.error;
        if (prepared.representations.isEmpty()) {
            board->clear();
            return {};
        }

        auto data = std::make_unique<QMimeData>();
        for (auto const& representation : prepared.representations)
            data->setData(representation.mime, representation.bytes);
        // Qt owns the immutable prepared byte representations after publication;
        // its event loop serves selection requests from other native processes.
        board->setMimeData(data.release());
        if (!board->ownsClipboard())
            return failure(
                "ERR_FIREFLY_CLIPBOARD_UNAVAILABLE", "Cannot acquire X11 clipboard ownership");
        return {};
    }

    if (op != "read" && op != "readText" && op != "has")
        return failure("ERR_INVALID_ARG_VALUE", "Invalid clipboard operation");
    auto format = request["format"].toString();
    if (op == "has" && !supported_mime(format))
        return failure("ERR_FIREFLY_UNSUPPORTED_API", "Unsupported clipboard MIME type");

    // An external owner may process events/change ownership during a synchronous
    // selection transfer. Never keep using a deleted provider or return mixed data.
    bool changed = false;
    auto connection = QObject::connect(
        board, &QClipboard::dataChanged, board, [&] { changed = true; }, Qt::DirectConnection);
    auto disconnect = qScopeGuard([&] { QObject::disconnect(connection); });
    QPointer<QMimeData const> data(board->mimeData());
    auto change_error
        = [&] { return failure("ERR_FIREFLY_CLIPBOARD_CHANGED", "Clipboard changed during read"); };
    if (changed)
        return change_error();
    if (!data) {
        if (op == "read")
            return { { "items", QJsonArray {} } };
        return op == "has" ? QJsonObject { { "has", false } } : QJsonObject { { "text", "" } };
    }

    if (op == "has") {
        auto present = data->hasFormat(format);
        return changed || !data ? change_error() : QJsonObject { { "has", present } };
    }

    if (op == "readText") {
        auto bytes = data->text().toUtf8();
        if (changed || !data)
            return change_error();
        if (bytes.size() > byte_budget)
            return failure("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
        return { { "text", QString::fromUtf8(bytes) } };
    }

    auto formats = data->formats();
    if (changed || !data)
        return change_error();
    QJsonArray representations;
    qsizetype size = 0;
    for (auto const& mime : formats) {
        if (transport_target(mime))
            continue;
        if (!supported_mime(mime))
            return failure(
                "ERR_FIREFLY_UNSUPPORTED_API", "Clipboard contains an unsupported native format");
        auto bytes = data->data(mime);
        if (changed || !data)
            return change_error();
        if (bytes.size() > byte_budget - size)
            return failure("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
        size += bytes.size();
        representations.append(
            QJsonObject { { "format", mime }, { "data", QString::fromLatin1(bytes.toBase64()) } });
    }

    QJsonArray items;
    if (!representations.isEmpty())
        items.append(representations);
    return { { "items", items } };
}
}
