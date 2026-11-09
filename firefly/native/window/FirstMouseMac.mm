/** Per-instance native view policy. Qt's original implementation retains input-transparency guards.
 */
#include "FirstMouse.h"
#include <AppKit/AppKit.h>
#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cstdio>
#include <cstring>
#include <objc/runtime.h>
#include <vector>

@interface ElytraFirstMouseState : NSObject {
@public
    BOOL accepts;
    Class original_class;
    Class reported_class;
    Class installed_class;
    IMP original_implementation;
    void* owner;
}

@end
@implementation ElytraFirstMouseState
@end

namespace Firefly {
namespace {
    char policy_key;
    constexpr size_t class_limit = 16;

    struct PolicyClass {
        Class original;
        Class installed;
        Class reported;
        IMP implementation;
    };

    std::vector<PolicyClass>& classes()
    {
        // Registered Objective-C classes cannot be destroyed while instances can
        // still exist. One bounded class per original Qt runtime class is reused.
        static std::vector<PolicyClass> values;
        return values;
    }

    BOOL invoke_original(id view, SEL selector, NSEvent* event, IMP implementation)
    {
        return implementation
            ? reinterpret_cast<BOOL (*)(id, SEL, NSEvent*)>(implementation)(view, selector, event)
            : NO;
    }

    BOOL accepts_first_mouse(id view, SEL selector, NSEvent* event)
    {
        if (![NSThread isMainThread])
            return NO;
        auto* state
            = static_cast<ElytraFirstMouseState*>(objc_getAssociatedObject(view, &policy_key));
        if (state) {
            if (!state->accepts)
                return NO;
            return invoke_original(view, selector, event, state->original_implementation);
        }
        // Restoration never overwrites somebody else's later runtime subclass.
        // If that subclass still inherits our method, delegate safely through the
        // recorded original rather than recursively sending the same selector.
        for (Class type = object_getClass(view); type; type = class_getSuperclass(type))
            for (auto const& entry : classes())
                if (entry.installed == type)
                    return invoke_original(view, selector, event, entry.implementation);
        return NO;
    }

    Class reported_view_class(id view, SEL)
    {
        auto* state
            = static_cast<ElytraFirstMouseState*>(objc_getAssociatedObject(view, &policy_key));
        if (state)
            return state->reported_class;
        for (Class type = object_getClass(view); type; type = class_getSuperclass(type))
            for (auto const& entry : classes())
                if (entry.installed == type)
                    return entry.reported;
        return Nil;
    }

    bool is_qt_view(Class type)
    {
        for (; type; type = class_getSuperclass(type)) {
            // Qt's actual NSView class may sit below its native KVO subclass.
            if (std::strcmp(class_getName(type), "QNSView") == 0)
                return true;
        }

        return false;
    }

    PolicyClass const* policy_class(Class original, Class reported)
    {
        auto& values = classes();
        for (auto const& entry : values)
            if (entry.original == original)
                return entry.reported == reported ? &entry : nullptr;
        if (!reported || values.size() >= class_limit || !is_qt_view(original))
            return nullptr;
        auto selector = @selector(acceptsFirstMouse:);
        auto method = class_getInstanceMethod(original, selector);
        auto class_method = class_getInstanceMethod(original, @selector(class));
        if (!method || !class_method)
            return nullptr;
        char name[96];
        std::snprintf(name, sizeof(name), "ElytraFirstMousePolicy_%zu", values.size() + 1);
        // An unrelated runtime class with our name is not ours to reuse or alter.
        if (objc_lookUpClass(name))
            return nullptr;
        auto installed = objc_allocateClassPair(original, name, 0);
        if (!installed)
            return nullptr;
        if (!class_addMethod(installed, selector, reinterpret_cast<IMP>(&accepts_first_mouse),
                method_getTypeEncoding(method))
            || !class_addMethod(installed, @selector(class),
                reinterpret_cast<IMP>(&reported_view_class),
                method_getTypeEncoding(class_method))) {
            objc_disposeClassPair(installed);
            return nullptr;
        }

        auto implementation = method_getImplementation(method);
        objc_registerClassPair(installed);
        values.push_back({ original, installed, reported, implementation });
        return &values.back();
    }

    class Controller final : public QObject {
    public:
        explicit Controller(QWidget& content)
            : QObject(&content)
            , m_content(&content)
        {
            content.installEventFilter(this);
            QObject::connect(&content, &QObject::destroyed, this, [this] {
                m_stopped = true;
                restore();
            });
        }

        ~Controller() override
        {
            m_stopped = true;
            restore();
        }

        bool configure(QWidget& window, bool accepts)
        {
            if (m_stopped || !m_content || m_content->window() != &window)
                return false;
            if (!restore())
                return false;
            m_owner = &window;
            m_accepts = accepts;
            m_surface_stopping = false;
            return bind();
        }

    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if (object == m_content
                && (event->type() == QEvent::MouseButtonPress
                    || event->type() == QEvent::MouseButtonDblClick
                    || event->type() == QEvent::MouseButtonRelease)) {
                // A replaced/unsupported native surface must not fall back to
                // Qt's default click-through while its policy is unavailable.
                // Consume content mouse input until the real native gate rebinds.
                if (!current_policy()) {
                    schedule_bind();
                    return true;
                }
            }

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

            if (object == m_content && event->type() == QEvent::Destroy) {
                m_stopped = true;
                restore();
                return false;
            }

            // A replacement QWindow can be born after the old observed window
            // has already died, so its SurfaceCreated event was not observable.
            // A new non-null native handle is the safe point to bind it; never
            // manufacture a handle while SurfaceAboutToBeDestroyed is running.
            if (event->type() == QEvent::WinIdChange && m_content && m_content->windowHandle()
                && m_content->windowHandle() != m_native && m_content->effectiveWinId())
                m_surface_stopping = false;
            switch (event->type()) {
            case QEvent::WinIdChange:
            case QEvent::ParentChange:
            case QEvent::Show:
                schedule_bind();
                break;
            default:
                break;
            }

            return false;
        }

    private:
        bool current_policy() const
        {
            if (!m_ready || !m_content || !m_owner || !m_view || !m_state
                || m_content->window() != m_owner || m_content->windowHandle() != m_native)
                return false;
            auto* actual = (__bridge NSView*)reinterpret_cast<void*>(m_content->effectiveWinId());
            auto* owner = (__bridge NSView*)reinterpret_cast<void*>(m_owner->effectiveWinId());
            return actual == m_view && owner && actual.window == owner.window
                && object_getClass(actual) == m_state->installed_class
                && objc_getAssociatedObject(actual, &policy_key) == m_state
                && m_state->owner == this;
        }

        bool restore()
        {
            m_ready = false;
            auto* view = m_view;
            auto* state = m_state;
            m_view = nil;
            m_state = nil;
            if (!view || !state)
                return true;
            if (objc_getAssociatedObject(view, &policy_key) != state || state->owner != this)
                return false;
            state->owner = nullptr;
            if (object_getClass(view) != state->installed_class) {
                // Never replace a foreign class mutation. Leave a bounded,
                // fail-closed associated policy if our inherited method remains.
                state->accepts = NO;
                return false;
            }

            object_setClass(view, state->original_class);
            objc_setAssociatedObject(view, &policy_key, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            return true;
        }

        bool bind()
        {
            @autoreleasepool {
                if (m_stopped || m_surface_stopping || !m_content || !m_owner
                    || m_content->window() != m_owner || !first_mouse_available())
                    return false;
                if (current_policy()) {
                    m_state->accepts = m_accepts;
                    return true;
                }

                if (!restore())
                    return false;
                m_owner->winId();
                auto* owner_view = (__bridge NSView*)reinterpret_cast<void*>(m_owner->winId());
                auto* view = (__bridge NSView*)reinterpret_cast<void*>(m_content->winId());
                if (!owner_view || !view || ![owner_view isKindOfClass:[NSView class]]
                    || ![view isKindOfClass:[NSView class]] || !view.window
                    || view.window != owner_view.window
                    || objc_getAssociatedObject(view, &policy_key))
                    return false;
                // KVO's inherited -class implementation assumes its own isa.
                // An extra subclass breaks that lookup and can report Nil to
                // AppKit's computed-property machinery (including safeAreaInsets).
                // Preserve the reported identity from before our instance change.
                auto reported = [view class];
                auto entry = policy_class(object_getClass(view), reported);
                if (!entry)
                    return false;
                auto* state = [ElytraFirstMouseState new];
                state->accepts = m_accepts;
                state->original_class = entry->original;
                state->reported_class = reported;
                state->installed_class = entry->installed;
                state->original_implementation = entry->implementation;
                state->owner = this;
                objc_setAssociatedObject(
                    view, &policy_key, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                object_setClass(view, entry->installed);
                if (object_getClass(view) != entry->installed) {
                    objc_setAssociatedObject(
                        view, &policy_key, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                    return false;
                }

                m_view = view;
                m_state = state;
                auto* native = m_content->windowHandle();
                if (native != m_native) {
                    if (m_native)
                        m_native->removeEventFilter(this);
                    m_native = native;
                    if (native)
                        native->installEventFilter(this);
                }

                m_ready = true;
                m_reported_failure = false;
                return true;
            }
        }

        void schedule_bind()
        {
            if (m_stopped || m_surface_stopping || m_scheduled)
                return;
            m_scheduled = true;
            QTimer::singleShot(0, this, [this] {
                m_scheduled = false;
                if (m_stopped || m_surface_stopping || !m_content || !m_owner)
                    return;
                if (m_content->window() != m_owner) {
                    restore(); // A new explicit owner must install its own policy.
                    return;
                }

                if (!bind() && !m_reported_failure) {
                    m_reported_failure = true;
                    qWarning("ERR_FIREFLY_FIRST_MOUSE: native content-view policy rebind failed; "
                             "content mouse input is blocked");
                }
            });
        }

        QPointer<QWidget> m_content;
        QPointer<QWidget> m_owner;
        QPointer<QWindow> m_native;
        __weak NSView* m_view { nil };
        __strong ElytraFirstMouseState* m_state { nil };
        bool m_ready { false };
        bool m_reported_failure { false };
        bool m_accepts { false };
        bool m_surface_stopping { false };
        bool m_stopped { false };
        bool m_scheduled { false };
    };
}

bool first_mouse_available()
{
    auto* app = QCoreApplication::instance();
    return app && QGuiApplication::platformName() == QStringLiteral("cocoa")
        && QThread::currentThread() == app->thread() && [NSThread isMainThread];
}

bool install_first_mouse(QWidget& window, QWidget& content, bool accepts)
{
    if (!first_mouse_available() || content.thread() != QThread::currentThread()
        || window.thread() != QThread::currentThread() || content.window() != &window)
        return false;
    for (auto* child : content.children())
        if (auto* controller = dynamic_cast<Controller*>(child))
            return controller->configure(window, accepts);
    auto* controller = new Controller(content);
    if (controller->configure(window, accepts))
        return true;
    delete controller;
    return false;
}

void clear_first_mouse(QWidget& content)
{
    if (!first_mouse_available() || content.thread() != QThread::currentThread())
        return;
    for (auto* child : content.children()) {
        if (auto* controller = dynamic_cast<Controller*>(child)) {
            delete controller;
            return;
        }
    }
}

}
