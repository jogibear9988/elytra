/** Observe the real renderer and native controls during one coordinated visible Cocoa window. */
#include "../../native/qt/ChromeWindow.h"
#include <AppKit/AppKit.h>
#include <QApplication>
#include <QImage>
#include <QTimer>
#include <QWindow>
#include <UI/Qt/WebContentView.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
struct Observation {
    bool geometry { false };
    bool styles { false };
    bool buttons { false };
    bool regions { false };
    bool pixels { false };
    QImage canvas;
};

bool native_buttons_are_hit_targets(NSWindow* window)
{
    auto* frame = window.contentView.superview;
    if (!frame)
        return false;
    for (auto kind : { NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton }) {
        auto* button = [window standardWindowButton:kind];
        if (!button || !button.superview || button.window != window || button.hidden
            || !button.enabled || !button.action)
            return false;
        id target = button.target;
        if (target) {
            if (![target respondsToSelector:button.action]
                || ([target isKindOfClass:[NSWindow class]] && target != window))
                return false;
        } else if (![window respondsToSelector:button.action]) {
            return false;
        }

        auto center = NSMakePoint(NSMidX(button.bounds), NSMidY(button.bounds));
        auto point = [button convertPoint:center toView:frame.superview];
        auto* hit = [frame hitTest:point];
        if (!hit || (hit != button && ![hit isDescendantOf:button]))
            return false;
    }

    return true;
}

void diagnose_buttons(NSWindow* window)
{
    if (!window)
        return;
    auto* frame = window.contentView.superview;
    for (auto kind : { NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton }) {
        auto* button = [window standardWindowButton:kind];
        auto point = button && frame
            ? [button convertPoint:NSMakePoint(NSMidX(button.bounds), NSMidY(button.bounds))
                            toView:frame.superview]
            : NSZeroPoint;
        auto* hit = frame && button ? [frame hitTest:point] : nil;
        std::fprintf(stderr,
            "Title bar native button kind=%ld hidden=%d enabled=%d action=%s target=%s hit=%s\n",
            static_cast<long>(kind), button.hidden, button.enabled,
            button.action ? NSStringFromSelector(button.action).UTF8String : "none",
            button.target ? NSStringFromClass([button.target class]).UTF8String : "responder-chain",
            hit ? NSStringFromClass([hit class]).UTF8String : "none");
    }
}

bool toolbar_pixel_matches(QImage const& image, QSize viewport)
{
    if (image.isNull() || viewport.width() <= 150 || viewport.height() <= 8)
        return false;
    auto x = std::clamp(static_cast<int>(std::floor(150.0 * image.width() / viewport.width())), 0,
        image.width() - 1);
    auto y = std::clamp(static_cast<int>(std::floor(8.0 * image.height() / viewport.height())), 0,
        image.height() - 1);
    auto color = image.pixelColor(x, y);
    return color.alpha() == 255 && std::abs(color.red() - 18) <= 1
        && std::abs(color.green() - 52) <= 1 && std::abs(color.blue() - 86) <= 1;
}

Observation observe(Firefly::ChromeWindow& window, Ladybird::WebContentView& view)
{
    Observation result;
    auto* native_view = (__bridge NSView*)reinterpret_cast<void*>(window.effectiveWinId());
    auto* native = native_view.window;
    result.geometry = view.mapTo(&window, QPoint(0, 0)) == QPoint(0, 0)
        && view.size() == window.size() && window.is_custom_chrome();
    result.styles = native && native.visible && (native.styleMask & NSWindowStyleMaskTitled)
        && (native.styleMask & NSWindowStyleMaskFullSizeContentView)
        && native.titleVisibility == NSWindowTitleHidden && native.titlebarAppearsTransparent;
    result.buttons = result.styles && native_buttons_are_hit_targets(native);
    result.regions = window.draggable({ 180, 20 }) && !window.draggable({ 270, 20 });
    // The public upstream preview reads the accepted compositor CPU bitmap. A
    // QWidget grab cannot inspect macOS's separate CALayer and is not an oracle.
    if (auto preview = view.tab_preview_pixmap(view.size()); preview.has_value())
        result.canvas = preview->toImage();
    result.pixels = toolbar_pixel_matches(result.canvas, view.size());
    return result;
}

void start_observer()
{
    QTimer::singleShot(0, QCoreApplication::instance(), [] {
        auto* timer = new QTimer(QCoreApplication::instance());
        QObject::connect(timer, &QTimer::timeout, timer, [timer] {
            static unsigned attempts = 0;
            for (auto* widget : QApplication::topLevelWidgets()) {
                auto* window = dynamic_cast<Firefly::ChromeWindow*>(widget);
                if (!window || window->windowTitle() != "elytra-title-bar-runtime"
                    || !window->isVisible())
                    continue;
                Ladybird::WebContentView* view = nullptr;
                for (auto* child : window->findChildren<QWidget*>()) {
                    if (auto* candidate = dynamic_cast<Ladybird::WebContentView*>(child)) {
                        view = candidate;
                        break;
                    }
                }

                if (!view)
                    continue;
                auto result = observe(*window, *view);
                if (++attempts == 200) {
                    std::fprintf(stderr,
                        "Title bar runtime waiting geometry=%d styles=%d buttons=%d regions=%d "
                        "pixels=%d viewport=%dx%d canvas=%dx%d origin=%d,%d\n",
                        result.geometry, result.styles, result.buttons, result.regions,
                        result.pixels, view->width(), view->height(), result.canvas.width(),
                        result.canvas.height(), view->mapTo(window, QPoint(0, 0)).x(),
                        view->mapTo(window, QPoint(0, 0)).y());
                    auto* native
                        = (__bridge NSView*)reinterpret_cast<void*>(window->effectiveWinId());
                    if (!result.buttons)
                        diagnose_buttons(native.window);
                    std::fflush(stderr);
                }

                if (!result.geometry || !result.styles || !result.buttons || !result.regions
                    || !result.pixels)
                    continue;
                std::puts("PASS native title bar runtime real toolbar pixels full viewport native "
                          "button hit targets and HTML drag regions");
                std::fflush(stdout);
                window->setWindowTitle("elytra-title-bar-runtime-ok");
                timer->stop();
                return;
            }
        });
        timer->start(20);
    });
}

Q_COREAPP_STARTUP_FUNCTION(start_observer)
}
