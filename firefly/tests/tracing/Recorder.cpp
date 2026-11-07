/** Actual clock durations, exact category filters, bounded retention and scope retirement. */
#include "native/tracing/Recorder.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <cstdlib>
#include <iostream>

using Firefly::Tracing::Recorder;

namespace {
void check(bool value, char const* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

QByteArray config(bool continuous, QJsonArray included = { "*" }, QJsonArray excluded = {})
{
    return QJsonDocument(QJsonObject { { "schema", 1 }, { "continuous", continuous },
                             { "included", included }, { "excluded", excluded } })
        .toJson(QJsonDocument::Compact);
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Recorder recorder;
    QString error;
    for (auto wire :
        { QByteArray("null"), QByteArray(" ").repeated(4097), config(false, { "v8" }) })
        check(!recorder.start(wire, error), "native invalid config accepted");
    check(recorder.start(config(false, { "firefly.main" }), error), "start failed");
    {
        Recorder::Scope ignored(recorder, "firefly.window", "excluded");
        Recorder::Scope measured(recorder, "firefly.main", "measured");
        QThread::usleep(1000);
    }

    auto trace = QJsonDocument::fromJson(recorder.finish()).object();
    auto events = trace.value("traceEvents").toArray();
    check(events.size() == 3, "category filtering failed");
    auto event = events.last().toObject();
    check(event.value("name") == "measured" && event.value("dur").toDouble() >= 500
            && event.value("ts").toDouble() > 0
            && event.value("pid").toInteger() == app.applicationPid(),
        "real duration/identity missing");
    for (bool continuous : { false, true }) {
        check(recorder.start(config(continuous), error), "restart failed");
        for (size_t index = 0; index < Recorder::capacity + 3; ++index) {
            Recorder::Scope scope(recorder, "firefly.main", index == 0 ? "first" : "later");
        }

        check(recorder.usage() == 1, "buffer usage not bounded");
        auto full = QJsonDocument::fromJson(recorder.finish()).object();
        auto saved = full.value("traceEvents").toArray();
        check(saved.size() == Recorder::capacity + 2, "capacity exceeded");
        check((saved[2].toObject().value("name") == "first") == !continuous,
            "retention mode ignored");
        check(full.value("metadata").toObject().value("droppedEvents").toInt() == 3,
            "loss not reported");
    }

    check(recorder.start(config(false), error), "old start failed");
    {
        Recorder::Scope old(recorder, "firefly.main", "retired");
        recorder.finish();
        check(recorder.start(config(false), error), "new start failed");
    }

    check(QJsonDocument::fromJson(recorder.finish()).object().value("traceEvents").toArray().size()
            == 2,
        "old scope leaked into next generation");
    check(recorder.start(config(false, { "*" }, { "*" }), error), "exclude start failed");
    {
        Recorder::Scope scope(recorder, "firefly.main", "excluded");
    }

    check(recorder.usage() == 0, "exclude wildcard ignored");
    recorder.reset();
    std::cout
        << "PASS native tracing clock/filter/capacity/retention/loss and generation retirement\n";
}
