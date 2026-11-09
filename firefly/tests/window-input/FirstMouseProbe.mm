/** Direct callbacks on hidden real Qt Cocoa views; never post mouse events or show a window. */
#include "../../native/window/FirstMouse.h"
#include <AppKit/AppKit.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QWidget>
#include <QWindow>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <objc/runtime.h>

namespace {
class RecreatableContent final : public QWidget {
public:
    using QWidget::QWidget;

    void recreate_native_surface()
    {
        // Match the genuine upstream WebContentView native-surface lifecycle.
        destroy();
        create();
    }
};

template <typename Predicate> bool wait_for_native_rebind(Predicate predicate)
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

NSView* materialize(QWidget& widget)
{
    auto identifier = widget.winId();
    return (__bridge NSView*)reinterpret_cast<void*>(identifier);
}

Class first_mouse_policy_class(Class type)
{
    constexpr char prefix[] = "ElytraFirstMousePolicy_";
    for (; type; type = class_getSuperclass(type)) {
        if (std::strncmp(class_getName(type), prefix, sizeof(prefix) - 1) == 0)
            return type;
    }

    return Nil;
}

bool retired_as_control(NSView* view, Class control)
{
    return object_getClass(view) == control
        && first_mouse_policy_class(object_getClass(view)) == Nil;
}

void check_reported_class_and_safe_area(
    QWidget& owner, QWidget& content, NSView* view, Class original)
{
    require(
        [view class] == original, "Scoped native policy preserves Qt's reported Objective-C class");
    auto insets = view.safeAreaInsets;
    require(std::isfinite(insets.top) && std::isfinite(insets.bottom) && std::isfinite(insets.left)
            && std::isfinite(insets.right),
        "Actual AppKit computed safeAreaInsets remains valid after policy installation");
    require(content.windowHandle() && owner.windowHandle(),
        "Real Qt native windows exist for safe-area reads");
    auto content_margins = content.windowHandle()->safeAreaMargins();
    auto owner_margins = owner.windowHandle()->safeAreaMargins();
    // Read the actual Qt/AppKit computed property even for hidden windows. Values
    // remain platform-owned; no guessed title height or fake zero inset is asserted.
    volatile int observed = content_margins.top() + owner_margins.top();
    (void)observed;
    auto original_size = content.size();
    content.resize(original_size + QSize(1, 1));
    content.windowHandle()->resize(content.size());
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
    require(materialize(content) == view && [view class] == original,
        "Hidden real native resize preserves NSView and reported Qt class identity");
    auto resized_insets = view.safeAreaInsets;
    require(std::isfinite(resized_insets.top) && std::isfinite(resized_insets.bottom)
            && std::isfinite(resized_insets.left) && std::isfinite(resized_insets.right),
        "Actual resized AppKit view retains valid computed safe-area properties");
    volatile int resized = content.windowHandle()->safeAreaMargins().top();
    (void)resized;
    content.resize(original_size);
    content.windowHandle()->resize(content.size());
}
}

int main(int argc, char** argv)
{
    @autoreleasepool {
        QApplication application(argc, argv);
        require(QApplication::platformName() == "cocoa", "Cocoa backend required without fallback");
        require(Firefly::first_mouse_available(), "Real Cocoa first mouse capability");
        QWidget first;
        QWidget second;
        QWidget unowned_window;
        first.resize(320, 220);
        second.resize(320, 220);
        unowned_window.resize(320, 220);
        QWidget first_content(&first);
        QWidget second_content(&second);
        QWidget unowned_content(&unowned_window);
        first_content.setGeometry(0, 0, 160, 100);
        second_content.setGeometry(0, 0, 160, 100);
        unowned_content.setGeometry(0, 0, 160, 100);
        NSView* first_view = materialize(first_content);
        NSView* second_view = materialize(second_content);
        NSView* unowned_view = materialize(unowned_content);
        require(first_view && second_view && unowned_view, "Actual Qt native NSViews exist");
        auto first_class = object_getClass(first_view);
        auto second_class = object_getClass(second_view);
        auto first_reported_class = [first_view class];
        auto second_reported_class = [second_view class];
        auto unowned_class = object_getClass(unowned_view);
        auto unowned_method = class_getInstanceMethod(unowned_class, @selector(acceptsFirstMouse:));
        require(unowned_method != nullptr, "Original Qt first mouse callback exists");
        auto unowned_implementation = method_getImplementation(unowned_method);
        auto original_true = [second_view acceptsFirstMouse:nil];
        auto unowned_result = [unowned_view acceptsFirstMouse:nil];
        require(original_true == YES, "Enabled ordinary Qt view accepts first mouse");

        require(Firefly::install_first_mouse(first, first_content, false), "Install false policy");
        require(Firefly::install_first_mouse(second, second_content, true), "Install true policy");
        check_reported_class_and_safe_area(first, first_content, first_view, first_reported_class);
        check_reported_class_and_safe_area(
            second, second_content, second_view, second_reported_class);
        require([first_view acceptsFirstMouse:nil] == NO,
            "False owner rejects actual first mouse callback");
        require([second_view acceptsFirstMouse:nil] == original_true,
            "True owner preserves original Qt callback");
        require(object_getClass(first_view) != first_class
                && object_getClass(second_view) != second_class,
            "Policies affect only scoped runtime subclasses");
        require(!first.isVisible() && !second.isVisible() && !unowned_window.isVisible(),
            "Every native window remains hidden");
        std::puts("PASS native first mouse hidden Cocoa callbacks and per-view isolation");

        require(object_getClass(unowned_view) == unowned_class, "Unowned view class unchanged");
        require(method_getImplementation(
                    class_getInstanceMethod(unowned_class, @selector(acceptsFirstMouse:)))
                == unowned_implementation,
            "Original Qt runtime class method never swizzled");
        require(
            [unowned_view acceptsFirstMouse:nil] == unowned_result, "Unowned callback unchanged");
        std::puts("PASS native first mouse unowned Qt class and callback preserved");

        require(Firefly::install_first_mouse(first, first_content, true),
            "Reinstall true on same view");
        require([first_view acceptsFirstMouse:nil] == original_true, "Same view true replacement");
        require([second_view acceptsFirstMouse:nil] == original_true,
            "Other owner unaffected by replacement");
        require(Firefly::install_first_mouse(first, first_content, false),
            "Reinstall false on same view");
        require([first_view acceptsFirstMouse:nil] == NO, "Same view false replacement");
        Firefly::clear_first_mouse(first_content);
        require(object_getClass(first_view) == first_class,
            "Clear restores exact original runtime class");
        first_content.setParent(&second);
        NSView* rebound = materialize(first_content);
        auto rebound_class = object_getClass(rebound);
        require(Firefly::install_first_mouse(second, first_content, true),
            "Rebind moved view to true owner");
        require([rebound acceptsFirstMouse:nil] == YES, "Moved view follows new true owner");
        Firefly::clear_first_mouse(first_content);
        require(object_getClass(rebound) == rebound_class,
            "Moved view clear restores its original Qt class");
        first_content.setParent(&first);
        auto returned_class = object_getClass(materialize(first_content));
        require(Firefly::install_first_mouse(first, first_content, false),
            "Moved view returns to false owner");
        require([materialize(first_content) acceptsFirstMouse:nil] == NO,
            "Returned view follows false owner");

        auto* unowned_temporary = new QWidget(&first);
        unowned_temporary->setGeometry(0, 0, 120, 80);
        NSView* retained_control = materialize(*unowned_temporary);
        auto control_original = object_getClass(retained_control);
        delete unowned_temporary;
        auto control_destroyed = object_getClass(retained_control);
        require(first_mouse_policy_class(control_destroyed) == Nil,
            "Unowned teardown control has no Firefly policy class");

        auto* temporary = new QWidget(&first);
        temporary->setGeometry(0, 0, 120, 80);
        NSView* retained_view = materialize(*temporary);
        auto original_class = object_getClass(retained_view);
        require(original_class == control_original,
            "Owned and independent unowned controls start with identical Qt runtime class");
        require(
            Firefly::install_first_mouse(first, *temporary, false), "Temporary owned view policy");
        require([retained_view acceptsFirstMouse:nil] == NO,
            "Temporary policy active before destruction");
        auto installed_class = object_getClass(retained_view);
        delete temporary;
        // ARC retains the NSView for class inspection only. Calling Qt methods after
        // QWidget teardown would be invalid, so no event/callback is sent here.
        auto destroyed_class = object_getClass(retained_view);
        if (!retired_as_control(retained_view, control_destroyed)) {
            std::fprintf(stderr,
                "Temporary QWidget teardown NSView=%p original=%s installed=%s after-delete=%s "
                "control-original=%s control-after=%s\n",
                (__bridge void*)retained_view, class_getName(original_class),
                class_getName(installed_class), class_getName(destroyed_class),
                class_getName(control_original), class_getName(control_destroyed));
        }

        require(retired_as_control(retained_view, control_destroyed),
            "QObject destruction matches independent Qt teardown normalization without a policy "
            "class");

        RecreatableContent unowned_recreated(&first);
        unowned_recreated.setGeometry(0, 0, 120, 80);
        NSView* unowned_old_surface = materialize(unowned_recreated);
        auto recreation_original = object_getClass(unowned_old_surface);
        unowned_recreated.recreate_native_surface();
        NSView* unowned_new_surface = materialize(unowned_recreated);
        require(unowned_new_surface != unowned_old_surface,
            "Independent Qt recreation control creates a distinct actual native handle");
        auto recreation_retired = object_getClass(unowned_old_surface);
        require(first_mouse_policy_class(recreation_retired) == Nil,
            "Independent Qt recreation retires without any policy class");

        RecreatableContent recreated(&first);
        recreated.setGeometry(0, 0, 120, 80);
        NSView* old_surface = materialize(recreated);
        auto surface_class = object_getClass(old_surface);
        auto surface_reported_class = [old_surface class];
        require(surface_class == recreation_original,
            "Owned and independent recreation controls start with identical Qt runtime class");
        require(Firefly::install_first_mouse(first, recreated, false),
            "Install false before actual surface recreation");
        recreated.recreate_native_surface();
        NSView* false_surface = materialize(recreated);
        require(
            false_surface != old_surface, "QWidget destroy/create produced a distinct real NSView");
        require(wait_for_native_rebind([&] {
            return retired_as_control(old_surface, recreation_retired)
                && first_mouse_policy_class(object_getClass(false_surface)) != Nil &&
                [false_surface acceptsFirstMouse:nil] == NO;
        }),
            "False policy automatically rebinds to actual recreated NSView without reinstall");
        check_reported_class_and_safe_area(first, recreated, false_surface, surface_reported_class);
        require(Firefly::install_first_mouse(first, recreated, true),
            "Set true before second actual surface recreation");
        require([false_surface acceptsFirstMouse:nil] == YES,
            "True policy before recreation calls original Qt callback");
        recreated.recreate_native_surface();
        NSView* true_surface = materialize(recreated);
        require(true_surface != false_surface,
            "Second QWidget destroy/create produced another real NSView");
        require(wait_for_native_rebind([&] {
            return retired_as_control(false_surface, recreation_retired)
                && first_mouse_policy_class(object_getClass(true_surface)) != Nil &&
                [true_surface acceptsFirstMouse:nil] == YES;
        }),
            "True policy automatically rebinds to actual recreated NSView without reinstall");
        check_reported_class_and_safe_area(first, recreated, true_surface, surface_reported_class);
        std::puts("PASS native first mouse preserves reported class and real AppKit Qt safe-area "
                  "getters");
        auto active_policy_class = first_mouse_policy_class(object_getClass(true_surface));
        require(
            active_policy_class != Nil, "Recreated true view has a genuine installed policy class");
        auto active_original = class_getSuperclass(active_policy_class);
        Firefly::clear_first_mouse(recreated);
        require(object_getClass(true_surface) == active_original
                && first_mouse_policy_class(object_getClass(true_surface)) == Nil,
            "Recreated surface cleanup restores original Qt class");
        std::puts("PASS native first mouse actual NSView recreation automatically preserves false "
                  "and true policies");
        Firefly::clear_first_mouse(first_content);
        Firefly::clear_first_mouse(second_content);
        require(object_getClass(materialize(first_content)) == returned_class,
            "Final first-view cleanup restores class");
        require(object_getClass(second_view) == second_class,
            "Final second-view cleanup restores class");
        require(!first.isVisible() && !second.isVisible(), "Lifecycle tests never display windows");
        std::puts("PASS native first mouse reinstall and teardown restore native class");
        return 0;
    }
}
