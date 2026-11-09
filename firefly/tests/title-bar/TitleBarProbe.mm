/** Real hidden Cocoa styles, native controls and Qt-local drag events; never OS input or show(). */
#include "../../native/qt/ChromeWindow.h"
#include "../../native/window/TitleBar.h"
#include <AppKit/AppKit.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMouseEvent>
#include <QThread>
#include <QVBoxLayout>
#include <QWindow>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
class RecreatableWindow final : public QWidget {
public:
    explicit RecreatableWindow(Qt::WindowFlags flags)
    {
        setWindowFlags(flags);
        setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
        resize(320, 220);
    }

    void recreate_native_surface()
    {
        destroy();
        create();
    }
};

template <typename Predicate> bool wait_for_rebind(Predicate predicate)
{
    QElapsedTimer elapsed;
    elapsed.start();
    do {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
        if (predicate())
            return true;
        QThread::msleep(1);
    } while (elapsed.elapsed() < 500);
    return false;
}

void require(bool condition, char const* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

NSWindow* materialize(QWidget& widget)
{
    auto* view = (__bridge NSView*)reinterpret_cast<void*>(widget.winId());
    require(view && view.window, "Real Qt Cocoa NSWindow exists");
    return view.window;
}

bool same_dimension(CGFloat first, CGFloat second)
{
    return std::abs(first - second) < 1;
}

unsigned check_native_hit_targets(NSWindow* window)
{
    auto* frame_view = window.contentView.superview;
    require(frame_view != nil, "Actual native frame view exists for hit testing");
    unsigned unavailable = 0;
    for (NSButton* button in @[
             [window standardWindowButton:NSWindowCloseButton],
             [window standardWindowButton:NSWindowMiniaturizeButton],
             [window standardWindowButton:NSWindowZoomButton]
         ]) {
        auto center = NSMakePoint(NSMidX(button.bounds), NSMidY(button.bounds));
        // NSView.hitTest takes a point in its superview's coordinates (nil means
        // window coordinates). Never test the button directly or invent a fallback.
        auto point = [button convertPoint:center toView:frame_view.superview];
        auto* hit = [frame_view hitTest:point];
        if (!hit) {
            require(!window.visible, "Unexpected unavailable hit test for a visible native window");
            ++unavailable;
            continue;
        }

        require(hit == button || [hit isDescendantOf:button],
            "Full content does not intercept an actual native traffic-light hit target");
    }

    return unavailable;
}

void check_hidden_native(NSWindow* window)
{
    require((window.styleMask & NSWindowStyleMaskTitled) != 0,
        "Hidden title bar preserves actual titled NSWindow");
    require((window.styleMask & NSWindowStyleMaskFullSizeContentView) != 0,
        "Actual NSWindow full-size content style");
    require(window.titleVisibility == NSWindowTitleHidden && window.titlebarAppearsTransparent,
        "Native title caption hidden and titlebar transparent");
    NSButton* close = [window standardWindowButton:NSWindowCloseButton];
    NSButton* minimize = [window standardWindowButton:NSWindowMiniaturizeButton];
    NSButton* zoom = [window standardWindowButton:NSWindowZoomButton];
    require(close && minimize && zoom && close != minimize && minimize != zoom && close != zoom,
        "Three distinct AppKit standard native control pointers exist");
    for (NSButton* button in @[ close, minimize, zoom ]) {
        require([button isKindOfClass:[NSButton class]] && button.window == window
                && button.superview && !button.hidden && button.action,
            "Actual standard Cocoa button remains attached, wired and unhidden");
    }

    require(same_dimension(window.contentView.frame.size.width, window.frame.size.width)
            && same_dimension(window.contentView.frame.size.height, window.frame.size.height),
        "Actual Cocoa content view covers entire native window without guessed title height");
}
}

int main(int argc, char** argv)
{
    @autoreleasepool {
        QApplication application(argc, argv);
        require(QApplication::platformName() == "cocoa" && Firefly::title_bar_available(),
            "Actual Cocoa title-bar capability");
        Firefly::ChromeWindow standard(true, false);
        standard.resize(320, 220);
        NSWindow* standard_native = materialize(standard);
        auto original_mask = standard_native.styleMask;
        auto original_visibility = standard_native.titleVisibility;
        auto original_transparency = standard_native.titlebarAppearsTransparent;
        require(
            Firefly::configure_title_bar(standard, false), "Default title-bar adapter admission");
        require(standard_native.styleMask == original_mask
                && standard_native.titleVisibility == original_visibility
                && standard_native.titlebarAppearsTransparent == original_transparency,
            "Default adapter preserves original actual Qt native style");
        require(!standard.is_custom_chrome(), "Default window keeps native chrome policy");

        Firefly::ChromeWindow hidden(true, true);
        require(hidden.windowFlags().testFlag(Qt::ExpandedClientAreaHint)
                && hidden.windowFlags().testFlag(Qt::NoTitleBarBackgroundHint)
                && !hidden.windowFlags().testFlag(Qt::FramelessWindowHint),
            "Expanded transparent titled Qt flags set before native creation");
        require(!hidden.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea),
            "Full-size content does not reserve safe-area title margins");
        hidden.resize(320, 220);
        QVBoxLayout layout(&hidden);
        layout.setContentsMargins(0, 0, 0, 0);
        layout.setSpacing(0);
        QWidget content(&hidden);
        layout.addWidget(&content);
        hidden.observe(&content);
        NSWindow* hidden_native = materialize(hidden);
        require(
            Firefly::configure_title_bar(hidden, true), "Hidden Cocoa title-bar adapter admission");
        layout.setGeometry(hidden.rect());
        layout.activate();
        check_hidden_native(hidden_native);
        require(hidden.is_custom_chrome(), "Hidden title bar owns custom app-region policy");
        std::puts("PASS native title bar hidden titled full-size transparent window preserves "
                  "native controls");
        require(content.geometry() == hidden.rect(),
            "Actual native Qt layout child fills full window including title area");

        Firefly::ChromeWindow combined(false, true);
        combined.resize(320, 220);
        NSWindow* combined_native = materialize(combined);
        require(Firefly::configure_title_bar(combined, true),
            "Hidden style with frame false retains native controls");
        check_hidden_native(combined_native);
        auto unavailable_hits
            = check_native_hit_targets(hidden_native) + check_native_hit_targets(combined_native);
        if (unavailable_hits) {
            std::printf("LIMIT native title bar traffic-light hit testing unavailable for %u "
                        "hidden native targets\n",
                unavailable_hits);
        } else {
            std::puts(
                "PASS native title bar native traffic-light hit targets remain above full content");
        }

        require(standard_native.styleMask == original_mask
                && standard_native.titleVisibility == original_visibility,
            "Hidden peer does not alter default native window");
        std::puts(
            "PASS native title bar actual full content geometry with default window unchanged");

        hidden.set_regions({ { QRect(0, 0, 300, 40), true }, { QRect(0, 0, 90, 40), false } });
        require(hidden.draggable({ 120, 20 }) && !hidden.draggable({ 50, 20 })
                && !hidden.draggable({ 120, 80 }),
            "Drag and no-drag region subtraction");
        auto drag = [&](QPoint local) {
            auto global = content.mapToGlobal(local);
            QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton,
                Qt::LeftButton, Qt::NoModifier);
            QMouseEvent move(QEvent::MouseMove, local + QPoint(30, 20), global + QPoint(30, 20),
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, local + QPoint(30, 20),
                global + QPoint(30, 20), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&content, &press);
            QCoreApplication::sendEvent(&content, &move);
            QCoreApplication::sendEvent(&content, &release);
        };

        auto before = hidden.pos();
        drag({ 120, 20 });
        require(hidden.pos() == before + QPoint(30, 20),
            "Qt-local drag event moves actual native QWidget geometry");
        auto after_drag = hidden.pos();
        drag({ 50, 20 });
        require(
            hidden.pos() == after_drag, "No-drag region leaves native window position unchanged");
        require(!standard.isVisible() && !hidden.isVisible() && !combined.isVisible(),
            "All native windows remain hidden throughout controls and Qt drag tests");
        std::puts("PASS native title bar drag and no-drag region policy without OS input");

        RecreatableWindow control(hidden.windowFlags());
        NSWindow* old_control = materialize(control);
        auto control_caption = old_control.titleVisibility;
        control.recreate_native_surface();
        NSWindow* new_control = materialize(control);
        require(old_control != new_control,
            "Independent Qt control recreates a genuine native NSWindow");
        auto retired_caption = old_control.titleVisibility;
        RecreatableWindow recreated(hidden.windowFlags());
        NSWindow* old_native = materialize(recreated);
        require(old_native.titleVisibility == control_caption,
            "Owned and independent recreation controls start with the same native caption");
        require(Firefly::configure_title_bar(recreated, true),
            "Configure caption policy before actual NSWindow recreation");
        recreated.recreate_native_surface();
        NSWindow* new_native = materialize(recreated);
        require(old_native != new_native, "Adapter follows a distinct actual recreated NSWindow");
        // Keep a real, controllable native caption state until queued binding runs;
        // successful repair must come from the adapter, not just Qt's creation flags.
        new_native.titleVisibility = NSWindowTitleVisible;
        require(wait_for_rebind([&] {
            return new_native.titleVisibility == NSWindowTitleHidden
                && old_native.titleVisibility == retired_caption;
        }),
            "Native caption policy automatically rebinds after destroy/create without reinstall");
        check_hidden_native(new_native);
        require(!control.isVisible() && !recreated.isVisible(),
            "Recreation controls never display windows");
        std::puts("PASS native title bar actual NSWindow recreation restores old caption and "
                  "rebinds hidden policy");
        return 0;
    }
}
