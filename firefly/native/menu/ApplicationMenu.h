/** Native application-menu ownership; no engine object or callback crosses this Qt boundary. */
#pragma once
#include <QJsonArray>
#include <QMenuBar>
#include <QPointer>
#include <QVector>
#include <functional>

namespace Firefly {
class ApplicationMenu {
public:
    using Activate
        = std::function<void(int generation, int command, bool checked, QWidget*, int modifiers)>;
    ApplicationMenu(QObject& context, Activate);

    ~ApplicationMenu();

    bool apply(QByteArray const& metadata, QString& error);

    void attach(QWidget&);

    void stop();

    // Native diagnostic access, not exposed to application JavaScript.
    QMenuBar* bar() const;

private:
    void clear_bars();

    void populate(QWidget&, QJsonArray const&, bool enabled = true, bool visible = true);

    QMenuBar* create_bar(QWidget* parent);

    QObject& m_context;
    Activate m_activate;
    QVector<QPointer<QWidget>> m_windows;
    QVector<QPointer<QMenuBar>> m_bars;
    QJsonArray m_items;
    int m_generation { 0 };
    bool m_installed { false };
    bool m_stopped { false };
};
}
