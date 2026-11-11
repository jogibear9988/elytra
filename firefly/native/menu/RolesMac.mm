/** Qt owns menu objects; restore only AppKit registrations still owned by this adapter. */
#include "Roles.h"
#include <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QMenu>
#include <QThread>
#include <array>

@protocol FireflyNativeDockDelegate <NSObject>
@property (nonatomic, retain) NSMenu* dockMenu;
@end

namespace Firefly::MenuRoles {
namespace {
    struct Registration {
        __strong NSMenu* previous { nil };
        __weak NSMenu* installed { nil };
    };

    std::array<Registration, 3> registrations;
    Registration dock;
    __weak id<FireflyNativeDockDelegate> dock_delegate;

    NSMenu* current(size_t index)
    {
        if (index == 0)
            return NSApp.windowsMenu;
        if (index == 1)
            return NSApp.helpMenu;
        return NSApp.servicesMenu;
    }

    void assign(size_t index, NSMenu* menu)
    {
        if (index == 0)
            NSApp.windowsMenu = menu;
        else if (index == 1)
            NSApp.helpMenu = menu;
        else
            NSApp.servicesMenu = menu;
    }
}

bool available()
{
    return qGuiApp && QThread::currentThread() == qGuiApp->thread()
        && QGuiApplication::platformName() == QStringLiteral("cocoa") && NSApp;
}

bool activation_is_keyboard()
{
    return available() && NSApp.currentEvent && NSApp.currentEvent.type == NSEventTypeKeyDown;
}

bool dock_available()
{
    return available() && [NSApp.delegate respondsToSelector:@selector(dockMenu)] &&
        [NSApp.delegate respondsToSelector:@selector(setDockMenu:)];
}

bool install_dock(QMenu& menu)
{
    if (!dock_available())
        return false;
    auto delegate = (id<FireflyNativeDockDelegate>)NSApp.delegate;
    NSMenu* native = menu.toNSMenu();
    if (!native)
        return false;
    if (dock.installed && delegate.dockMenu != dock.installed)
        return false;
    if (!dock.installed)
        dock.previous = delegate.dockMenu;
    menu.setAsDockMenu();
    if (delegate.dockMenu != native)
        return false;
    dock_delegate = delegate;
    dock.installed = native;
    return true;
}

void clear_dock()
{
    auto delegate = dock_delegate;
    if (delegate && dock.installed && delegate.dockMenu == dock.installed)
        delegate.dockMenu = dock.previous;
    dock_delegate = nil;
    dock.installed = nil;
    dock.previous = nil;
}

bool bind(QMenu& menu, QString const& role)
{
    if (!available())
        return false;
    size_t index;
    if (role == "window")
        index = 0;
    else if (role == "help")
        index = 1;
    else if (role == "services")
        index = 2;
    else
        return false;
    NSMenu* native = menu.toNSMenu();
    if (!native)
        return false;
    auto& owned = registrations[index];
    if (owned.installed)
        return false;
    owned.previous = current(index);
    owned.installed = native;
    assign(index, native);
    return current(index) == native;
}

void clear()
{
    if (!available())
        return;
    for (size_t index = 0; index < registrations.size(); ++index) {
        auto& owned = registrations[index];
        if (owned.installed && current(index) == owned.installed)
            assign(index, owned.previous);
        owned.installed = nil;
        owned.previous = nil;
    }
}

bool perform(QString const& role)
{
    if (!available())
        return false;
    if (role == "hide")
        [NSApp hide:nil];
    else if (role == "hideOthers")
        [NSApp hideOtherApplications:nil];
    else if (role == "unhide")
        [NSApp unhideAllApplications:nil];
    else if (role == "front")
        [NSApp arrangeInFront:nil];
    else
        return false;
    return true;
}
}
