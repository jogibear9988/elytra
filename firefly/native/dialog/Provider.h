/** Owned native dialogs; completion data never retains VM objects. */
#pragma once
#include <QJsonObject>
#include <QString>
#include <functional>
#include <memory>
#include <string>

class QWidget;

namespace Firefly::Dialogs {
struct Result {
    QJsonObject value;
    std::string code;
    std::string message;
};

using Completion = std::function<void(Result)>;

class Provider {
public:
    static std::unique_ptr<Provider> create();

    virtual ~Provider() = default;
    virtual Result start(
        int id, QString const& kind, QJsonObject const& options, QWidget* parent, Completion)
        = 0;
    virtual Result message_sync(QJsonObject const&, QWidget*) = 0;
    virtual void cancel(int id) = 0;
    virtual void stop() = 0;
};
}
