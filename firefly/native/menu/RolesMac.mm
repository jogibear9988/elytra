/** Qt owns menu objects; restore only AppKit registrations still owned by this adapter. */
#include "Roles.h"
#include <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QMenu>
#include <QThread>
#include <algorithm>
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

    struct ServicesPlacement {
        __strong NSMenu* application { nil };
        __strong NSMenu* source { nil };
        __strong NSMenuItem* installed { nil };
        __strong NSMenuItem* previous { nil };
        __strong NSMenu* authored_submenu { nil };
        NSInteger source_index { 0 };
        NSInteger application_index { 0 };
    } services_placement;

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

    bool place_services(NSMenu* native)
    {
        NSMenu* application
            = NSApp.mainMenu.numberOfItems ? [NSApp.mainMenu itemAtIndex:0].submenu : nil;
        NSMenu* source = native.supermenu;
        NSMenu* provider = NSApp.servicesMenu;
        if (!application || !source || !provider)
            return false;
        if (source == application && native == provider)
            return true;
        NSInteger source_index = [source indexOfItemWithSubmenu:native];
        NSInteger application_index = [application indexOfItemWithSubmenu:provider];
        if (source_index < 0 || application_index < 0 || services_placement.installed)
            return false;
        NSMenuItem* installed = [source itemAtIndex:source_index];
        NSMenuItem* previous = [application itemAtIndex:application_index];
        services_placement
            = { application, source, installed, previous, native, source_index, application_index };
        // AppKit retains the first Services provider even after later setters.
        // Reuse that exact OS-managed menu, as Qt's application menu does.
        // Services is an OS-populated role, not an ordinary delegated submenu
        // (Electron's role uses a plain native menu rather than its child model).
        [source removeItem:installed];
        [application removeItem:previous];
        previous.submenu = nil;
        installed.submenu = provider;
        NSInteger insertion = application_index;
        if (source == application && source_index < application_index)
            --insertion;
        [application insertItem:installed atIndex:std::min(insertion, application.numberOfItems)];
        return installed.submenu == provider && NSApp.servicesMenu == provider;
    }

    void restore_services_placement(NSMenu* provider)
    {
        auto& placement = services_placement;
        if (placement.installed && placement.installed.menu == placement.application
            && placement.installed.submenu == provider && NSApp.servicesMenu == provider
            && !placement.previous.menu && !placement.previous.submenu) {
            [placement.application removeItem:placement.installed];
            placement.installed.submenu = nil;
            placement.previous.submenu = provider;
            placement.installed.submenu = placement.authored_submenu;
            auto restore_source = [&] {
                [placement.source
                    insertItem:placement.installed
                       atIndex:std::min(placement.source_index, placement.source.numberOfItems)];
            };

            auto restore_previous = [&] {
                [placement.application insertItem:placement.previous
                                          atIndex:std::min(placement.application_index,
                                                      placement.application.numberOfItems)];
            };

            if (placement.source == placement.application
                && placement.source_index < placement.application_index) {
                restore_source();
                restore_previous();
            } else {
                restore_previous();
                restore_source();
            }
        }

        services_placement = {};
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
    if (!available()) {
        qWarning("Native menu role binding refused: Cocoa owner unavailable");
        return false;
    }

    size_t index;
    if (role == "window")
        index = 0;
    else if (role == "help")
        index = 1;
    else if (role == "services")
        index = 2;
    else {
        qWarning("Native menu role binding refused: unknown role");
        return false;
    }

    NSMenu* native = menu.toNSMenu();
    if (!native) {
        qWarning("Native menu role binding refused: no Qt native menu (role %zu)", index);
        return false;
    }

    auto& owned = registrations[index];
    if (owned.installed) {
        if (current(index) == owned.installed
            && (owned.installed == native
                || (index == 2 && services_placement.authored_submenu == native
                    && services_placement.installed.submenu == owned.installed)))
            return true;
        qWarning("Native menu role binding refused: another owned menu (role %zu)", index);
        return false;
    }

    if (index == 0 && native.numberOfItems == 0) {
        // Match Qt's Cocoa menu adapter workaround for FB13369198: AppKit's
        // windowsMenu setter can access itemAtIndex:-1 on an empty menu.
        NSMenuItem* placeholder = [[NSMenuItem alloc] initWithTitle:@""
                                                             action:nil
                                                      keyEquivalent:@""];
        placeholder.hidden = YES;
        [native addItem:placeholder];
    }

    owned.previous = current(index);
    if (index == 2 && !place_services(native)) {
        owned.previous = nil;
        qWarning("Native menu role binding refused: Services application-menu placement");
        return false;
    }

    if (index == 2)
        native = owned.previous; // The exact existing AppKit provider is now in our role item.
    else
        assign(index, native);
    if (current(index) != native) {
        NSMenu* application_menu
            = NSApp.mainMenu.numberOfItems ? [NSApp.mainMenu itemAtIndex:0].submenu : nil;
        qWarning("Native menu registration state: role=%zu actual=%u previous=%u unchanged=%u "
                 "bundle=%u in_app_menu=%u",
            index, current(index) ? 1u : 0u, owned.previous ? 1u : 0u,
            current(index) == owned.previous ? 1u : 0u,
            NSBundle.mainBundle.bundleIdentifier.length ? 1u : 0u,
            application_menu && native.supermenu == application_menu ? 1u : 0u);
        if (index == 2)
            restore_services_placement(native);
        owned.previous = nil;
        qWarning("Native menu role binding refused: AppKit registration changed (role %zu)", index);
        return false;
    }

    owned.installed = native;
    return true;
}

void clear()
{
    if (!available())
        return;
    for (size_t index = 0; index < registrations.size(); ++index) {
        auto& owned = registrations[index];
        bool registered = owned.installed && current(index) == owned.installed;
        if (index == 2)
            restore_services_placement(owned.installed);
        if (registered && index != 2)
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
