/** Main-thread native duration events; bounded recording owns no VM or renderer data. */
#pragma once
#include "../../../mite/runtime/core/NativeCaptureLifetime.h"
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <chrono>
#include <cstdint>
#include <deque>

namespace Firefly::Tracing {
class Recorder : public Mite::NativeCaptureOwner<Recorder> {
public:
    ~Recorder()
    {
        this->revoke_native_captures();
    }

    template <typename Visitor> void visit_capture_edges(Visitor&) const
    {
        // Only native policy/Qt data; callbacks are native transport hooks.
    }

    static constexpr size_t capacity = 8192;
    static QStringList categories();

    bool start(QByteArray const&, QString& error);

    QByteArray finish();

    double usage() const;

    void reset();

    // A scope may outlive stop/restart inside its JS callback. Its generation
    // prevents that old duration from leaking into the next recording.
    class Scope {
    public:
        Scope(Recorder&, char const* category, char const* name);

        ~Scope();

        Scope(Scope const&) = delete;
        Scope& operator=(Scope const&) = delete;

    private:
        Recorder& m_owner;
        char const* m_category;
        char const* m_name;
        uint64_t m_generation { 0 };
        std::chrono::steady_clock::time_point m_begin;
    };

private:
    bool accepts(QString const&) const;

    bool m_recording { false };
    bool m_continuous { false };
    uint64_t m_generation { 0 };
    uint64_t m_dropped { 0 };
    QStringList m_included, m_excluded;
    std::deque<QJsonObject> m_events;
};
}
