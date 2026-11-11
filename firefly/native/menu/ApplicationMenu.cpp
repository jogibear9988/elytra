/** Rebuild validated native trees atomically; retired QAction generations cannot call a new menu.
 */
#include "ApplicationMenu.h"
#include "Roles.h"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLayout>
#include <QMenu>
#include <QSet>
#include <cmath>

namespace Firefly {
namespace {
    class MenuAction final : public QAction {
    public:
        using QAction::QAction;

        bool keyboard_activation() const
        {
            return m_keyboard;
        }

    protected:
        bool event(QEvent* event) override
        {
            bool old = m_keyboard;
            m_keyboard = event->type() == QEvent::Shortcut;
            bool handled = QAction::event(event);
            m_keyboard = old;
            return handled;
        }

    private:
        bool m_keyboard { false };
    };

    bool integer(QJsonValue value, int& result)
    {
        double number = value.toDouble(-1);
        if (!value.isDouble() || number < 1 || number > 2147483647 || std::trunc(number) != number)
            return false;
        result = static_cast<int>(number);
        return true;
    }

    void checked(QJsonArray& items, int command, bool value)
    {
        for (int i = 0; i < items.size(); ++i) {
            auto node = items[i].toObject();
            if (node.value("command").toInt() == command)
                node.insert("checked", value);
            auto children = node.value("submenu").toArray();
            checked(children, command, value);
            node.insert("submenu", children);
            items[i] = node;
        }
    }

    void checked(QWidget& widget, int command, bool value)
    {
        for (auto* action : widget.actions()) {
            if (action->data().toInt() == command && action->isCheckable())
                action->setChecked(value);
            if (action->menu())
                checked(*action->menu(), command, value);
        }
    }

    bool accelerator(QJsonValue value, QKeySequence& result)
    {
        if (!value.isObject())
            return false;
        auto chord = value.toObject();
        auto key = chord.value("key").toString();
        auto flagsValue = chord.value("modifiers");
        int flags = flagsValue.toInt(-1);
        if (chord.size() != 2 || !chord.value("key").isString() || !flagsValue.isDouble()
            || flags < 0 || flags > 15 || flagsValue.toDouble() != flags)
            return false;
        Qt::Key code = Qt::Key_unknown;
        if (key.size() == 1 && key[0].unicode() >= 33 && key[0].unicode() <= 126
            && !(key[0] >= QChar('a') && key[0] <= QChar('z')))
            code = static_cast<Qt::Key>(key[0].unicode());
        else {
            static QHash<QString, Qt::Key> const names { { "Space", Qt::Key_Space },
                { "Tab", Qt::Key_Tab }, { "Enter", Qt::Key_Return }, { "Escape", Qt::Key_Escape },
                { "Backspace", Qt::Key_Backspace }, { "Delete", Qt::Key_Delete },
                { "Insert", Qt::Key_Insert }, { "Home", Qt::Key_Home }, { "End", Qt::Key_End },
                { "PageUp", Qt::Key_PageUp }, { "PageDown", Qt::Key_PageDown },
                { "Up", Qt::Key_Up }, { "Down", Qt::Key_Down }, { "Left", Qt::Key_Left },
                { "Right", Qt::Key_Right } };
            code = names.value(key, Qt::Key_unknown);
            if (code == Qt::Key_unknown && key.startsWith('F')) {
                bool valid = false;
                int number = key.mid(1).toInt(&valid);
                if (valid && number >= 1 && number <= 24 && key == "F" + QString::number(number))
                    code = static_cast<Qt::Key>(Qt::Key_F1 + number - 1);
            }
        }
        if (code == Qt::Key_unknown)
            return false;
        Qt::KeyboardModifiers modifiers;
        if (flags & 1)
            modifiers |= Qt::ShiftModifier;
        if (flags & 4)
            modifiers |= Qt::AltModifier;
#if defined(Q_OS_MACOS)
        if (flags & 2)
            modifiers |= Qt::MetaModifier;
        if (flags & 8)
            modifiers |= Qt::ControlModifier;
#else
        if (flags & 2)
            modifiers |= Qt::ControlModifier;
        if (flags & 8)
            modifiers |= Qt::MetaModifier;
#endif
        result = QKeySequence(QKeyCombination(modifiers, code));
        return !result.isEmpty();
    }

    bool validate(QJsonArray const& items, QSet<int>& commands, QSet<QString>& roles, int depth = 1)
    {
        if (depth > 8)
            return false;
        for (auto const& item : items) {
            if (!item.isObject())
                return false;
            auto node = item.toObject();
            int command;
            auto type = node.value("type").toString();
            static QSet<QString> const fields { "command", "label", "type", "enabled", "visible",
                "checked", "submenu", "accelerator", "role" };
            for (auto field = node.begin(); field != node.end(); ++field)
                if (!fields.contains(field.key()))
                    return false;
            QKeySequence shortcut;
            auto role = node.value("role");
            if ((node.contains("accelerator")
                    && (type == "submenu" || type == "separator"
                        || !accelerator(node.value("accelerator"), shortcut)))
                || (node.contains("role")
                    && (!role.isString() || type != "submenu"
                        || (role.toString() != "window" && role.toString() != "help"
                            && role.toString() != "services")
                        || !MenuRoles::available())))
                return false;
            if (node.size() < 7 || node.size() > 9 || !integer(node.value("command"), command)
                || commands.contains(command) || commands.size() >= 256
                || !node.value("label").isString()
                || node.value("label").toString().toUtf8().size() > 1024
                || node.value("label").toString().contains(QChar(0))
                || !node.value("enabled").isBool() || !node.value("visible").isBool()
                || !node.value("checked").isBool() || !node.value("submenu").isArray()
                || (type != "normal" && type != "checkbox" && type != "separator"
                    && type != "submenu")
                || (depth == 1 && type != "submenu")
                || (type != "checkbox" && node.value("checked").toBool()))
                return false;
            if (node.contains("role")) {
                if (roles.contains(role.toString()))
                    return false;
                roles.insert(role.toString());
            }
            auto children = node.value("submenu").toArray();
            if (type != "submenu" && !children.isEmpty())
                return false;
            commands.insert(command);
            if (type == "submenu" && !validate(children, commands, roles, depth + 1))
                return false;
        }

        return true;
    }
}

ApplicationMenu::ApplicationMenu(QObject& context, Activate activate)
    : m_context(context)
    , m_activate(std::move(activate))
{
}

ApplicationMenu::~ApplicationMenu()
{
    revoke_native_captures();
    stop();
}

void ApplicationMenu::clear_bars()
{
    MenuRoles::clear();
    for (auto bar : m_bars) {
        if (!bar)
            continue;
        if (auto* parent = bar->parentWidget(); parent && parent->layout())
            parent->layout()->setMenuBar(nullptr);
        delete bar;
    }

    m_bars.clear();
}

void ApplicationMenu::populate(QWidget& target, QJsonArray const& items, bool enabled, bool visible)
{
    for (auto const& value : items) {
        auto node = value.toObject();
        auto type = node.value("type").toString();
        QAction* action;
        if (type == "submenu") {
            auto* menu = new QMenu(node.value("label").toString(), &target);
            action = menu->menuAction();
            action->setMenuRole(QAction::NoRole);
            target.addAction(action);
            populate(*menu, node.value("submenu").toArray(),
                enabled && node.value("enabled").toBool(),
                visible && node.value("visible").toBool());
            if (node.contains("role") && !MenuRoles::bind(*menu, node.value("role").toString()))
                qFatal("Native menu role registration failed after validated admission");
        } else {
            action = new MenuAction(node.value("label").toString(), &target);
            // Set before insertion: Cocoa may infer and merge roles as soon as
            // an action enters a menu, based on translated labels such as Quit.
            action->setMenuRole(QAction::NoRole);
            action->setSeparator(type == "separator");
            action->setCheckable(type == "checkbox");
            action->setChecked(node.value("checked").toBool());
            if (node.contains("accelerator")) {
                QKeySequence shortcut;
                if (!accelerator(node.value("accelerator"), shortcut))
                    qFatal("Invalid shortcut after validated menu admission");
                action->setShortcut(shortcut);
                action->setShortcutContext(Qt::ApplicationShortcut);
            }
            target.addAction(action);
            int generation = m_generation, command = node.value("command").toInt();
            QObject::connect(action, &QAction::triggered, &m_context,
                [this, action, generation, command](bool checked) {
                    if (generation != m_generation || !action->isEnabled() || !action->isVisible())
                        return;
                    if (action->isCheckable()) {
                        Firefly::checked(m_items, command, checked);
                        for (auto bar : m_bars)
                            if (bar)
                                Firefly::checked(*bar, command, checked);
                    }

                    auto modifiers = QApplication::keyboardModifiers();
#if defined(Q_OS_MACOS)
                    bool control = modifiers.testFlag(Qt::MetaModifier),
                         meta = modifiers.testFlag(Qt::ControlModifier);
#else
                bool control = modifiers.testFlag(Qt::ControlModifier), meta = modifiers.testFlag(Qt::MetaModifier);
#endif
                    int flags = (modifiers.testFlag(Qt::ShiftModifier) ? 1 : 0) | (control ? 2 : 0)
                        | (modifiers.testFlag(Qt::AltModifier) ? 4 : 0) | (meta ? 8 : 0);
                    if (static_cast<MenuAction*>(action)->keyboard_activation()
                        || MenuRoles::activation_is_keyboard())
                        flags |= 16;
                    if (m_activate)
                        m_activate(
                            generation, command, checked, QApplication::activeWindow(), flags);
                });
        }

        action->setData(node.value("command").toInt());
        action->setEnabled(enabled && node.value("enabled").toBool());
        action->setVisible(visible && node.value("visible").toBool());
    }
}

QMenuBar* ApplicationMenu::create_bar(QWidget* parent)
{
    auto* bar = new QMenuBar(parent);
#if defined(Q_OS_MACOS)
    bar->setNativeMenuBar(true);
#else
    bar->setNativeMenuBar(false);
#endif
    populate(*bar, m_items);
    m_bars.append(bar);
    return bar;
}

bool ApplicationMenu::apply(QByteArray const& metadata, QString& error)
{
    if (m_stopped || metadata.size() > 65536) {
        error = "Native application menu is stopped or over budget";
        return false;
    }

    auto parsed = QJsonDocument::fromJson(metadata);
    auto object = parsed.object();
    int generation;
    QSet<int> commands;
    QSet<QString> roles;
    auto items = object.value("items");
    if (!parsed.isObject() || object.size() != 3 || object.value("schema") != QJsonValue(1)
        || !integer(object.value("generation"), generation) || generation <= m_generation
        || (!items.isNull() && !items.isArray())
        || (items.isArray() && !validate(items.toArray(), commands, roles))) {
        error = "Invalid native application menu schema, generation or budget";
        return false;
    }

    clear_bars();
    m_generation = generation;
    m_items = items.toArray();
    m_installed = !items.isNull();
    if (!m_installed)
        return true;
#if defined(Q_OS_MACOS)
    create_bar(nullptr);
#else
    for (auto window : m_windows)
        if (window && window->layout())
            window->layout()->setMenuBar(create_bar(window));
#endif
    return true;
}

void ApplicationMenu::attach(QWidget& window)
{
    if (m_stopped)
        return;
    m_windows.removeIf([](auto pointer) { return pointer.isNull(); });
    m_windows.append(&window);
#if !defined(Q_OS_MACOS)
    if (m_installed && window.layout())
        window.layout()->setMenuBar(create_bar(&window));
#endif
}

QMenuBar* ApplicationMenu::bar() const
{
    return m_bars.isEmpty() ? nullptr : m_bars.first().data();
}

void ApplicationMenu::stop()
{
    revoke_native_captures();
    if (m_stopped)
        return;
    m_stopped = true;
    m_activate = {};
    m_generation = 0;
    clear_bars();
    m_installed = false;
}
}
