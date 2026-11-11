/** Native application-menu ownership; no engine object or callback crosses this Qt boundary. */
#pragma once
#include "../../../mite/runtime/core/NativeCaptureLifetime.h"
#include <QJsonArray>
#include <QMenuBar>
#include <QPointer>
#include <QVector>
#include <functional>

class QMenu;

namespace Firefly {
class ApplicationMenu : public Mite::NativeCaptureOwner<ApplicationMenu> {
public:
    template <typename Visitor> void visit_capture_edges(Visitor&) const
    {
        // Only native policy/Qt data; callbacks are native transport hooks.
    }

    using Activate
        = std::function<void(int generation, int command, bool checked, QWidget*, int modifiers)>;
    ApplicationMenu(QObject& context, Activate, bool dock = false);

    ~ApplicationMenu();

    bool apply(QByteArray const& metadata, QString& error);

    void attach(QWidget&);

    void stop();

    // Native diagnostic access, not exposed to application JavaScript.
    QMenuBar* bar() const;

private:
    void clear_bars();

    void populate(
        QWidget&, QJsonArray const&, bool enabled = true, bool visible = true, int generation = -1);

    QMenuBar* create_bar(QWidget* parent);

    QObject& m_context;
    Activate m_activate;
    QVector<QPointer<QWidget>> m_windows;
    QVector<QPointer<QMenuBar>> m_bars;
    QJsonArray m_items;
    int m_generation { 0 };
    bool m_installed { false };
    bool m_stopped { false };
    bool m_dock_mode { false };
    QPointer<QMenu> m_dock;
};
}
