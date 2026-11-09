/** Native Qt window-manager size hints with checked outer/content DIP translation. */
#pragma once
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <vector>

class QWidget;
class QWindow;

namespace Firefly {
struct WindowLimits {
    QSize minimum { 0, 0 };
    QSize maximum { 0, 0 }; // Zero is independently unbounded on each axis.
};

class WindowSizeConstraints final : public QObject {
public:
    explicit WindowSizeConstraints(QWidget&);

    static bool valid(WindowLimits const&);

    bool initialize(WindowLimits const&, bool content);

    bool set_minimum(QSize outer);

    bool set_maximum(QSize outer);

    QSize minimum();

    QSize maximum();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    void bind_window();

    void apply();

    QPointer<QWidget> m_widget;
    QPointer<QWindow> m_window;
    WindowLimits m_requested;
    bool m_minimum_content { false };
    bool m_maximum_content { false };
    bool m_applying { false };
    std::vector<QMetaObject::Connection> m_connections;
};
}
