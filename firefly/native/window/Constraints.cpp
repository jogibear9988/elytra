/** Set real QWindow hints without QWidget's immediate constraint-setter resize side effect. */
#include "Constraints.h"
#include <QEvent>
#include <QMargins>
#include <QPlatformSurfaceEvent>
#include <QWidget>
#include <QWindow>
#include <algorithm>
#include <limits>

namespace Firefly {
namespace {
    bool valid_size(QSize value)
    {
        return value.width() >= 0 && value.height() >= 0 && value.width() <= 16384
            && value.height() <= 16384;
    }

    QSize frame_size(QWindow const* window)
    {
        auto margins = window ? window->frameMargins() : QMargins {};
        auto sum = [](int first, int second) {
            return static_cast<int>(std::clamp<qint64>(
                static_cast<qint64>(first) + second, 0, std::numeric_limits<int>::max()));
        };

        return { sum(margins.left(), margins.right()), sum(margins.top(), margins.bottom()) };
    }

    int to_client(int value, int frame, bool content, bool maximum)
    {
        if (!value)
            return maximum ? QWIDGETSIZE_MAX : 0;
        return content ? value : std::max(1, value - frame);
    }

    int to_outer(int value, int frame, bool unset)
    {
        if (unset)
            return 0;
        return static_cast<int>(
            std::min<qint64>(static_cast<qint64>(value) + frame, std::numeric_limits<int>::max()));
    }
}

WindowSizeConstraints::WindowSizeConstraints(QWidget& widget)
    : QObject(&widget)
    , m_widget(&widget)
{
    widget.winId();
    widget.installEventFilter(this);
    bind_window();
}

bool WindowSizeConstraints::valid(WindowLimits const& limits)
{
    return valid_size(limits.minimum) && valid_size(limits.maximum);
}

void WindowSizeConstraints::bind_window()
{
    auto* window = m_widget ? m_widget->windowHandle() : nullptr;
    if (m_window == window)
        return;
    for (auto const& connection : m_connections)
        QObject::disconnect(connection);
    m_connections.clear();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (!window)
        return;
    window->installEventFilter(this);
    auto restore = [this] { apply(); };
    m_connections.push_back(QObject::connect(window, &QWindow::minimumWidthChanged, this, restore));
    m_connections.push_back(
        QObject::connect(window, &QWindow::minimumHeightChanged, this, restore));
    m_connections.push_back(QObject::connect(window, &QWindow::maximumWidthChanged, this, restore));
    m_connections.push_back(
        QObject::connect(window, &QWindow::maximumHeightChanged, this, restore));
}

void WindowSizeConstraints::apply()
{
    if (m_applying || !m_widget)
        return;
    bind_window();
    if (!m_window)
        return;
    m_applying = true;
    auto geometry = m_window->geometry();
    auto frame = frame_size(m_window);
    QSize minimum(to_client(m_requested.minimum.width(), frame.width(), m_minimum_content, false),
        to_client(m_requested.minimum.height(), frame.height(), m_minimum_content, false));
    QSize maximum(to_client(m_requested.maximum.width(), frame.width(), m_maximum_content, true),
        to_client(m_requested.maximum.height(), frame.height(), m_maximum_content, true));
    // A minimum wins a conflicting effective maximum, but the raw requested
    // maximum survives so clearing the minimum reveals it again, as in Electron.
    maximum = maximum.expandedTo(minimum);
    if (m_window->minimumSize() != minimum)
        m_window->setMinimumSize(minimum);
    if (m_window->maximumSize() != maximum)
        m_window->setMaximumSize(maximum);
    // Some QPA backends immediately resize the platform window while applying
    // hints, even though QWidget itself remains unchanged. Programmatic native
    // geometry is not hint-clamped: restore the existing rect so a setter changes
    // future resize policy without unexpectedly resizing the current window.
    if (m_window->geometry() != geometry)
        m_window->setGeometry(geometry);
    m_applying = false;
}

bool WindowSizeConstraints::initialize(WindowLimits const& limits, bool content)
{
    if (!valid(limits))
        return false;
    m_requested = limits;
    m_minimum_content = m_maximum_content = content;
    apply();
    return true;
}

bool WindowSizeConstraints::set_minimum(QSize outer)
{
    if (!valid_size(outer))
        return false;
    m_requested.minimum = outer;
    m_minimum_content = false;
    apply();
    return true;
}

bool WindowSizeConstraints::set_maximum(QSize outer)
{
    if (!valid_size(outer))
        return false;
    m_requested.maximum = outer;
    m_maximum_content = false;
    apply();
    return true;
}

QSize WindowSizeConstraints::minimum()
{
    apply();
    if (!m_window)
        return {};
    auto frame = frame_size(m_window);
    auto size = m_window->minimumSize();
    return { to_outer(size.width(), frame.width(), !m_requested.minimum.width()),
        to_outer(size.height(), frame.height(), !m_requested.minimum.height()) };
}

QSize WindowSizeConstraints::maximum()
{
    apply();
    if (!m_window)
        return {};
    auto frame = frame_size(m_window);
    auto size = m_window->maximumSize();
    return { to_outer(size.width(), frame.width(), !m_requested.maximum.width()),
        to_outer(size.height(), frame.height(), !m_requested.maximum.height()) };
}

bool WindowSizeConstraints::eventFilter(QObject*, QEvent* event)
{
    if (event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
        return false;
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Expose:
    case QEvent::WinIdChange:
    case QEvent::PlatformSurface:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ScreenChangeInternal:
        apply();
        break;
    default:
        break;
    }

    return false;
}
}
