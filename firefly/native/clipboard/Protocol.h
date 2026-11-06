/** Platform-independent clipboard wire validation; prepare every byte before OS ownership changes.
 */
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Firefly::ClipboardProtocol {
inline constexpr qsizetype byte_budget = 8 * 1024 * 1024;

struct Representation {
    QString mime;
    QByteArray bytes;
};

struct PreparedWrite {
    QVector<Representation> representations;
    QJsonObject error;

    bool valid() const
    {
        return error.isEmpty();
    }
};

QJsonObject failure(char const* code, char const* message);

bool supported_mime(QString const&);

// An empty successful representation list means clear. Failed preparation owns
// no OS resources and cannot mutate an existing clipboard, including on budget failure.
PreparedWrite prepare_write(QJsonObject const&);
}
