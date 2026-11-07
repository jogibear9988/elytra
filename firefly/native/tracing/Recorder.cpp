/** Legacy trace-event JSON from measured native work, not Chromium/V8 profiling. */
#include "Recorder.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>

namespace Firefly::Tracing {
QStringList Recorder::categories()
{
    return { "firefly.main", "firefly.window", "firefly.ipc" };
}

bool Recorder::accepts(QString const& category) const
{
    return (m_included.contains("*") || m_included.contains(category))
        && !(m_excluded.contains("*") || m_excluded.contains(category));
}

bool Recorder::start(QByteArray const& wire, QString& error)
{
    if (wire.size() > 4096) {
        error = "Tracing configuration exceeds budget";
        return false;
    }

    auto document = QJsonDocument::fromJson(wire);
    auto config = document.object();
    if (!document.isObject() || config.size() != 4 || config.value("schema") != QJsonValue(1)
        || !config.value("continuous").isBool()) {
        error = "Invalid tracing schema";
        return false;
    }

    auto parse = [&](char const* key, QStringList& target) {
        auto value = config.value(key);
        if (!value.isArray() || value.toArray().size() > 32)
            return false;
        for (auto item : value.toArray()) {
            if (!item.isString()
                || (item.toString() != "*" && !categories().contains(item.toString())))
                return false;
            target.append(item.toString());
        }

        return true;
    };

    QStringList included, excluded;
    if (!parse("included", included) || !parse("excluded", excluded)) {
        error = "Unknown native tracing category";
        return false;
    }
    // Repeated starts preserve the active recording; configuration is not changed.
    if (m_recording)
        return true;
    if (m_generation == std::numeric_limits<uint64_t>::max()) {
        error = "Tracing generation exhausted";
        return false;
    }

    ++m_generation;
    m_events.clear();
    m_dropped = 0;
    m_included = included;
    m_excluded = excluded;
    m_continuous = config.value("continuous").toBool();
    m_recording = true;
    return true;
}

Recorder::Scope::Scope(Recorder& owner, char const* category, char const* name)
    : m_owner(owner)
    , m_category(category)
    , m_name(name)
{
    if (owner.m_recording && owner.accepts(QString::fromUtf8(category))) {
        m_generation = owner.m_generation;
        m_begin = std::chrono::steady_clock::now();
    }
}

Recorder::Scope::~Scope()
{
    if (!m_generation || !m_owner.m_recording || m_generation != m_owner.m_generation)
        return;
    auto end = std::chrono::steady_clock::now();
    if (m_owner.m_events.size() == capacity) {
        ++m_owner.m_dropped;
        if (!m_owner.m_continuous)
            return;
        m_owner.m_events.pop_front();
    }

    auto micros
        = [](auto duration) { return std::chrono::duration<double, std::micro>(duration).count(); };
    m_owner.m_events.push_back(QJsonObject { { "cat", QString::fromUtf8(m_category) },
        { "name", QString::fromUtf8(m_name).left(256) }, { "ph", "X" },
        { "ts", micros(m_begin.time_since_epoch()) }, { "dur", micros(end - m_begin) },
        { "pid", QCoreApplication::applicationPid() }, { "tid", 1 } });
}

QByteArray Recorder::finish()
{
    m_recording = false;
    QJsonArray events;
    events.append(QJsonObject { { "ph", "M" }, { "name", "process_name" },
        { "pid", QCoreApplication::applicationPid() }, { "tid", 1 },
        { "args", QJsonObject { { "name", "Firefly main" } } } });
    events.append(QJsonObject { { "ph", "M" }, { "name", "thread_name" },
        { "pid", QCoreApplication::applicationPid() }, { "tid", 1 },
        { "args", QJsonObject { { "name", "Mite/Qt owner" } } } });
    for (auto const& event : m_events)
        events.append(event);
    auto result
        = QJsonDocument(QJsonObject { { "traceEvents", events }, { "displayTimeUnit", "ms" },
                            { "metadata",
                                QJsonObject { { "format", "firefly-trace-events-v1" },
                                    { "scope", "main-process-only" },
                                    { "droppedEvents", static_cast<double>(m_dropped) } } } })
              .toJson(QJsonDocument::Compact);
    m_events.clear();
    return result;
}

double Recorder::usage() const
{
    return static_cast<double>(m_events.size()) / capacity;
}

void Recorder::reset()
{
    m_recording = false;
    m_events.clear();
}
}
