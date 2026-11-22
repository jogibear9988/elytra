/** Own title visibility and placement of the existing Cocoa caption buttons. */
#include "TitleBar.h"
#include <AppKit/AppKit.h>
#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <array>

namespace Firefly {
namespace {
    class Controller final : public QObject {
    public:
        explicit Controller(QWidget& widget)
            : QObject(&widget)
            , m_widget(&widget)
        {
            widget.installEventFilter(this);
            QObject::connect(&widget, &QObject::destroyed, this, [this] {
                m_stopped = true;
                restore();
            });
        }

        ~Controller() override
        {
            m_stopped = true;
            restore();
        }

        bool configure(bool hidden)
        {
            if (m_stopped || !m_widget)
                return false;
            if (!hidden) {
                restore_buttons();
                m_position.reset();
            }

            m_hidden = hidden;
            m_surface_stopping = false;
            return bind();
        }

        bool set_position(std::optional<QPoint> position)
        {
            if (!m_hidden || !bind())
                return false;
            if (!position.has_value()) {
                restore_buttons();
            } else if (!place_buttons(*position)) {
                return false;
            }

            m_position = position;
            return true;
        }

        std::optional<QPoint> position() const
        {
            return m_position;
        }

    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if (event->type() == QEvent::PlatformSurface) {
                auto type = static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType();
                if (type == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                    m_surface_stopping = true;
                    restore();
                } else {
                    m_surface_stopping = false;
                    schedule_bind();
                }

                return false;
            }

            if (object == m_widget && event->type() == QEvent::Destroy) {
                m_stopped = true;
                restore();
                return false;
            }

            if (event->type() == QEvent::WinIdChange && m_widget && m_widget->windowHandle()
                && m_widget->windowHandle() != m_window && m_widget->effectiveWinId())
                m_surface_stopping = false;
            switch (event->type()) {
            case QEvent::Show:
            case QEvent::WinIdChange:
            case QEvent::WindowStateChange:
            case QEvent::WindowTitleChange:
            case QEvent::Resize:
                schedule_bind();
                break;
            default:
                break;
            }

            return false;
        }

    private:
        struct ButtonPlacement {
            __weak NSButton* button { nil };
            NSPoint original {};
            NSPoint applied {};
        };

        void restore_buttons()
        {
            for (auto const& placement : m_buttons) {
                auto* button = placement.button;
                if (button && NSEqualPoints(button.frame.origin, placement.applied))
                    [button setFrameOrigin:placement.original];
            }

            m_buttons = {};
        }

        bool place_buttons(QPoint position)
        {
            auto* native = m_native;
            if (!native || (native.styleMask & NSWindowStyleMaskFullScreen))
                return false;
            auto* close = [native standardWindowButton:NSWindowCloseButton];
            auto* minimize = [native standardWindowButton:NSWindowMiniaturizeButton];
            auto* zoom = [native standardWindowButton:NSWindowZoomButton];
            auto* parent = close.superview;
            if (!parent || minimize.superview != parent || zoom.superview != parent
                || !native.contentView)
                return false;

            auto content = [native.contentView convertRect:native.contentView.bounds toView:nil];
            auto close_frame = [parent convertRect:close.frame toView:nil];
            auto minimize_frame = [parent convertRect:minimize.frame toView:nil];
            auto zoom_frame = [parent convertRect:zoom.frame toView:nil];
            auto parent_area = [parent convertRect:parent.bounds toView:nil];
            auto left = NSMinX(content) + position.x();
            auto top = NSMaxY(content) - position.y();
            auto group = NSUnionRect(close_frame, NSUnionRect(minimize_frame, zoom_frame));
            auto placed
                = NSOffsetRect(group, left - NSMinX(close_frame), top - NSMaxY(close_frame));
            // Outside the native control container the buttons can lose hit-testing.
            if (!NSContainsRect(content, placed) || !NSContainsRect(parent_area, placed))
                return false;

            auto source = [parent convertPoint:NSMakePoint(NSMinX(close_frame), NSMaxY(close_frame))
                                      fromView:nil];
            auto target = [parent convertPoint:NSMakePoint(left, top) fromView:nil];
            auto delta = NSMakePoint(target.x - source.x, target.y - source.y);
            NSArray<NSButton*>* buttons = @[ close, minimize, zoom ];
            for (size_t index = 0; index < m_buttons.size(); ++index) {
                auto* button = buttons[index];
                auto& placement = m_buttons[index];
                auto current = button.frame.origin;
                if (placement.button != button || !NSEqualPoints(current, placement.applied))
                    placement.original = current;
                placement.button = button;
                placement.applied = NSMakePoint(current.x + delta.x, current.y + delta.y);
                [button setFrameOrigin:placement.applied];
            }

            return true;
        }

        void restore()
        {
            restore_buttons();
            auto* native = m_native;
            m_native = nil;
            if (native
                && native.titleVisibility
                    == (m_hidden ? NSWindowTitleHidden : NSWindowTitleVisible))
                native.titleVisibility = m_original;
        }

        bool bind()
        {
            @autoreleasepool {
                if (m_stopped || m_surface_stopping || !m_widget || !title_bar_available())
                    return false;
                auto flags = m_widget->windowFlags();
                if (m_hidden
                    && (!(flags & Qt::ExpandedClientAreaHint)
                        || !(flags & Qt::NoTitleBarBackgroundHint)
                        || (flags & Qt::FramelessWindowHint)))
                    return false;
                auto* view = (__bridge NSView*)reinterpret_cast<void*>(m_widget->winId());
                if (!view || ![view isKindOfClass:[NSView class]] || !view.window)
                    return false;
                auto* native = view.window;
                if (!(native.styleMask & NSWindowStyleMaskTitled)) {
                    // Existing frame:false windows legitimately have no title
                    // caption to configure. Preserve their ordinary native path.
                    return !m_hidden;
                }

                if (m_hidden && !(native.styleMask & NSWindowStyleMaskFullSizeContentView))
                    return false;
                if (native != m_native) {
                    restore();
                    m_original = native.titleVisibility;
                    m_native = native;
                }

                native.titleVisibility = m_hidden ? NSWindowTitleHidden : NSWindowTitleVisible;
                auto* window = m_widget->windowHandle();
                if (window != m_window) {
                    if (m_window)
                        m_window->removeEventFilter(this);
                    m_window = window;
                    if (window)
                        window->installEventFilter(this);
                }

                if (m_position.has_value() && !place_buttons(*m_position))
                    restore_buttons();
                m_reported_failure = false;
                return native.titleVisibility
                    == (m_hidden ? NSWindowTitleHidden : NSWindowTitleVisible)
                    && (!m_hidden || native.titlebarAppearsTransparent);
            }
        }

        void schedule_bind()
        {
            if (m_scheduled || m_stopped || m_surface_stopping)
                return;
            m_scheduled = true;
            QTimer::singleShot(0, this, [this] {
                m_scheduled = false;
                if (m_stopped || m_surface_stopping || !m_widget)
                    return;
                if (!bind() && !m_reported_failure) {
                    m_reported_failure = true;
                    qWarning("ERR_FIREFLY_TITLE_BAR: native caption policy rebind failed");
                }
            });
        }

        std::optional<QPoint> m_position;
        std::array<ButtonPlacement, 3> m_buttons;
        QPointer<QWidget> m_widget;
        QPointer<QWindow> m_window;
        __weak NSWindow* m_native { nil };
        NSWindowTitleVisibility m_original { NSWindowTitleVisible };
        bool m_hidden { false };
        bool m_stopped { false };
        bool m_scheduled { false };
        bool m_surface_stopping { false };
        bool m_reported_failure { false };
    };
}

bool title_bar_available()
{
    auto* app = QCoreApplication::instance();
    return app && QGuiApplication::platformName() == QStringLiteral("cocoa")
        && QThread::currentThread() == app->thread() && [NSThread isMainThread];
}

bool configure_title_bar(QWidget& window, bool hidden)
{
    if (!title_bar_available() || window.thread() != QThread::currentThread() || !window.isWindow())
        return !hidden && !title_bar_available();
    for (auto* child : window.children())
        if (auto* controller = dynamic_cast<Controller*>(child))
            return controller->configure(hidden);
    auto* controller = new Controller(window);
    if (controller->configure(hidden))
        return true;
    delete controller;
    return false;
}

bool set_title_bar_button_position(QWidget& window, std::optional<QPoint> position)
{
    if (!title_bar_available() || window.thread() != QThread::currentThread())
        return false;
    for (auto* child : window.children())
        if (auto* controller = dynamic_cast<Controller*>(child))
            return controller->set_position(position);
    return false;
}

std::optional<QPoint> title_bar_button_position(QWidget& window)
{
    if (title_bar_available() && window.thread() == QThread::currentThread()) {
        for (auto* child : window.children())
            if (auto* controller = dynamic_cast<Controller*>(child))
                return controller->position();
    }

    return {};
}
}
