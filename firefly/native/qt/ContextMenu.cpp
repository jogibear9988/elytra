/**
 * Provides the owned Qt context-menu integration required by the embedded view. Browser
 * chrome behavior remains separate from the public desktop Menu API.
 */

#include <LibWebView/Menu.h>
#include <QAction>
#include <QMenu>
#include <UI/Qt/Menu.h>
#include <UI/Qt/StringUtils.h>

// WebContentView's toolkit seam. Upstream's Menu.cpp assumes its tabbed browser
// Application; Firefly hosts a view in its own window instead.
namespace Ladybird {
static void populate(QMenu& target, WebView::Menu& source)
{
    for (auto const& item : source.items()) {
        item.visit(
            [&](NonnullRefPtr<WebView::Action> const& action) {
                auto* native = target.addAction(qstring_from_ak_string(action->text()));
                native->setEnabled(action->enabled());
                native->setVisible(action->visible());
                native->setCheckable(action->is_checkable());
                if (action->is_checkable())
                    native->setChecked(action->checked());
                QObject::connect(native, &QAction::triggered, native,
                    [weak = action->make_weak_ptr()](bool checked) {
                        if (auto action = weak.strong_ref();
                            action && action->enabled() && action->visible()) {
                            if (action->is_checkable())
                                action->set_checked(checked);
                            action->activate();
                        }
                    });
            },
            [&](NonnullRefPtr<WebView::Menu> const& menu) {
                auto* child = target.addMenu(qstring_from_ak_string(menu->title()));
                child->menuAction()->setVisible(menu->visible());
                populate(*child, *menu);
            },
            [&](WebView::Separator const&) { target.addSeparator(); });
    }
}

void execute_context_menu_later(
    QPointer<QMenu>& shown, QWidget& parent, WebView::Menu& source, QPoint const& position)
{
    if (shown)
        shown->deleteLater();
    auto* menu = new QMenu(&parent);
    shown = menu;
    populate(*menu, source);
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(position);
}
}
