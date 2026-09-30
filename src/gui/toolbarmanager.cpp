/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "toolbarmanager.h"

#include "widgets/menuheader.h"

#include <gui/editablelayout.h>
#include <gui/fywidget.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/id.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>

using namespace Qt::StringLiterals;

namespace Fooyin {
ToolbarManager::ToolbarManager(QMainWindow* window, QMenuBar* menuBar, ActionManager* actionManager,
                               WidgetProvider* widgetProvider, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_window{window}
    , m_menuBar{menuBar}
    , m_widgetProvider{widgetProvider}
    , m_settings{settings}
    , m_menuToolbar{new QToolBar{tr("Menu"), window}}
    , m_menuAction{nullptr}
    , m_menuBarMovableAction{new QAction{tr("&Place menu bar in a toolbar"), this}}
    , m_lockToolbarsAction{new QAction{tr("&Lock toolbars"), this}}
    , m_menuBarMovableCommand{actionManager->registerAction(m_menuBarMovableAction,
                                                            Constants::Actions::MenuBarInToolbar)}
    , m_lockToolbarsCommand{actionManager->registerAction(m_lockToolbarsAction, Constants::Actions::LockToolbars)}
    , m_toolbarsMenu{new QMenu{tr("Toolbars"), window}}
    , m_menuBarSizePolicy{m_menuBar->sizePolicy()}
    , m_menuBarNative{m_menuBar->isNativeMenuBar()}
    , m_menuInToolbar{false}
    , m_menuVisible{true}
    , m_toolbarsLocked{true}
    , m_editing{false}
{
    qApp->installEventFilter(this);

    m_menuToolbar->setFloatable(true);
    m_menuToolbar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    m_menuToolbar->toggleViewAction()->setVisible(false);

    m_window->setMenuBar(m_menuBar);
    m_window->addToolBar(Qt::TopToolBarArea, m_menuToolbar);

    m_menuBarMovableCommand->setCategories({tr("Layout")});
    m_lockToolbarsCommand->setCategories({tr("Layout")});

    m_menuBarMovableAction->setCheckable(true);
    m_menuBarMovableAction->setChecked(m_settings->value<Settings::Gui::MenuBarMovable>());
    m_menuBarMovableAction->setStatusTip(tr("Allow the menu bar to share rows and positions with other toolbars"));
    QObject::connect(
        m_menuBarMovableAction, &QAction::triggered, this,
        [this](bool movable) { m_settings->set<Settings::Gui::MenuBarMovable>(movable); }, Qt::QueuedConnection);
    m_settings->subscribe<Settings::Gui::MenuBarMovable>(m_menuBarMovableAction, &QAction::setChecked);

    m_lockToolbarsAction->setCheckable(true);
    m_lockToolbarsAction->setChecked(m_settings->value<Settings::Gui::ToolbarsLocked>());
    m_lockToolbarsAction->setStatusTip(tr("Prevent toolbars from being moved"));
    QObject::connect(m_lockToolbarsAction, &QAction::triggered, this,
                     [this](bool locked) { m_settings->set<Settings::Gui::ToolbarsLocked>(locked); });
    m_settings->subscribe<Settings::Gui::ToolbarsLocked>(m_lockToolbarsAction, &QAction::setChecked);

    QObject::connect(m_toolbarsMenu, &QMenu::aboutToShow, this, [this] { populateToolbarsMenu(m_toolbarsMenu); });
    if(auto* layoutMenu = actionManager->actionContainer(Constants::Menus::Layout); layoutMenu && layoutMenu->menu()) {
        auto setupLayoutMenu = [this, menu = layoutMenu->menu()] {
            const auto actions = menu->actions();
            if(actions.contains(m_menuBarMovableCommand->action())) {
                return;
            }
            const auto separator = std::ranges::find_if(actions, &QAction::isSeparator);
            QAction* before      = separator != actions.cend() ? *separator : nullptr;
            menu->insertAction(before, m_lockToolbarsCommand->action());
            menu->insertAction(before, m_menuBarMovableCommand->action());
            menu->insertMenu(before, m_toolbarsMenu);
        };
        QObject::connect(layoutMenu->menu(), &QMenu::aboutToShow, this, setupLayoutMenu);
        setupLayoutMenu();
    }

    m_settings->subscribe<Settings::Gui::LayoutEditing>(this, [this](bool editing) { setEditing(editing); });
    m_settings->subscribe<Settings::Gui::MenuBarMovable>(this, [this](bool movable) { setMenuInToolbar(movable); });
    m_settings->subscribe<Settings::Gui::ToolbarsLocked>(this, [this](bool locked) { setToolbarsLocked(locked); });
    setMenuInToolbar(m_settings->value<Settings::Gui::MenuBarMovable>());
    setToolbarsLocked(m_settings->value<Settings::Gui::ToolbarsLocked>());
    setEditing(m_settings->value<Settings::Gui::LayoutEditing>());
}

ToolbarManager::~ToolbarManager()
{
    qApp->removeEventFilter(this);
}

QJsonArray ToolbarManager::saveLayout() const
{
    QJsonArray result;

    for(const auto& entry : m_toolbars) {
        if(!entry.toolbar || entry.items.empty()) {
            continue;
        }

        QJsonArray items;
        for(const auto& item : entry.items) {
            if(!item.action) {
                continue;
            }

            if(item.action->isSeparator()) {
                items.append(QJsonObject{{u"Separator"_s, true}});
                continue;
            }

            const QJsonObject widget = item.widget ? EditableLayout::saveWidget(item.widget) : item.savedWidget;
            if(!widget.isEmpty()) {
                items.append(QJsonObject{{u"Widget"_s, widget}});
            }
        }
        if(items.empty()) {
            continue;
        }

        QJsonObject toolbar;
        toolbar["ID"_L1]      = entry.id;
        toolbar["Area"_L1]    = static_cast<int>(m_window->toolBarArea(entry.toolbar));
        toolbar["Visible"_L1] = !entry.toolbar->isHidden();
        toolbar["Items"_L1]   = items;
        result.append(toolbar);
    }

    return result;
}

QByteArray ToolbarManager::saveState() const
{
    return m_window->saveState();
}

void ToolbarManager::loadLayout(const QJsonArray& toolbars, const QByteArray& state)
{
    clear();

    for(const auto& value : toolbars) {
        const auto toolbar = value.toObject();
        QJsonArray items   = toolbar.value("Items"_L1).toArray();
        if(items.empty()) {
            if(const auto widget = toolbar.value("Widget"_L1).toObject(); !widget.isEmpty()) {
                items.append(QJsonObject{{u"Widget"_s, widget}});
            }
        }
        if(items.empty()) {
            continue;
        }

        QString title{tr("Toolbar")};
        for(const auto& itemValue : std::as_const(items)) {
            const auto widget = itemValue.toObject().value("Widget"_L1).toObject();
            if(!widget.isEmpty()) {
                title = m_widgetProvider->displayName(widget.constBegin().key());
                break;
            }
        }

        auto* created = createToolbar(title, toolbar.value("ID"_L1).toString());
        for(const auto& itemValue : std::as_const(items)) {
            const auto item = itemValue.toObject();
            if(item.value("Separator"_L1).toBool()) {
                insertSeparator(created, nullptr);
                continue;
            }

            const auto widget = item.value("Widget"_L1).toObject();
            if(widget.isEmpty()) {
                continue;
            }

            const QString key = widget.constBegin().key();
            if(m_widgetProvider->supportsToolbar(key)) {
                insertWidget(created, key, nullptr, widget);
            }
            else if(!m_widgetProvider->widgetExists(key)) {
                insertMissingWidget(created, key, nullptr, widget);
            }
        }

        if(const auto* entry = entryFor(created); entry && !entry->items.empty()) {
            const auto area = static_cast<Qt::ToolBarArea>(toolbar.value("Area"_L1).toInt(Qt::TopToolBarArea));
            if(created->allowedAreas().testFlag(area)) {
                m_window->addToolBar(area, created);
            }
            created->setVisible(toolbar.value("Visible"_L1).toBool(true));
        }
        else {
            removeToolbar(created);
        }
    }

    if(!state.isEmpty()) {
        m_window->restoreState(state);
    }
    setMenuVisible(m_settings->value<Settings::Gui::ShowMenuBar>());
}

void ToolbarManager::clear()
{
    const auto toolbars = std::exchange(m_toolbars, {});
    for(const auto& entry : toolbars) {
        if(entry.toolbar) {
            m_window->removeToolBar(entry.toolbar);
            delete entry.toolbar;
        }
    }
}

std::vector<FyWidget*> ToolbarManager::widgets() const
{
    std::vector<FyWidget*> result;
    for(const auto& entry : m_toolbars) {
        for(const auto& item : entry.items) {
            if(item.widget) {
                result.push_back(item.widget);
            }
        }
    }
    return result;
}

void ToolbarManager::setMenuVisible(bool visible)
{
    m_menuVisible = visible;
    updateMenuVisibility();
}

bool ToolbarManager::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::ContextMenu) {
        auto* contextMenuEvent = static_cast<QContextMenuEvent*>(event);
        if(auto* toolbar = toolbarFor(watched)) {
            auto* source = static_cast<QWidget*>(watched);
            showContextMenu(toolbar, source->mapTo(toolbar, contextMenuEvent->pos()), contextMenuEvent->globalPos(),
                            source);
            event->accept();
            return true;
        }
        if(m_editing && watched == m_window) {
            showContextMenu(m_menuToolbar, m_window->mapTo(m_menuToolbar, contextMenuEvent->pos()),
                            contextMenuEvent->globalPos(), m_window);
            event->accept();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void ToolbarManager::setEditing(bool editing)
{
    m_editing = editing;
}

void ToolbarManager::setMenuInToolbar(bool inToolbar)
{
    if(std::exchange(m_menuInToolbar, inToolbar) != inToolbar) {
        if(inToolbar) {
            m_menuBar->setNativeMenuBar(false);
            m_menuBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
            if(m_menuAction) {
                m_menuToolbar->addAction(m_menuAction);
            }
            else {
                m_menuAction = m_menuToolbar->addWidget(m_menuBar);
            }
        }
        else {
            m_menuToolbar->removeAction(m_menuAction);
            m_menuBar->setSizePolicy(m_menuBarSizePolicy);
            m_menuBar->setNativeMenuBar(m_menuBarNative);
            m_window->setMenuBar(m_menuBar);
        }
    }
    updateMenuVisibility();
}

void ToolbarManager::setToolbarsLocked(bool locked)
{
    m_toolbarsLocked = locked;

    const auto updateToolbar = [locked](QToolBar* toolbar) {
        toolbar->setMovable(!locked);
        toolbar->layout()->activate();
        toolbar->updateGeometry();
    };

    updateToolbar(m_menuToolbar);
    for(const auto& entry : m_toolbars) {
        if(entry.toolbar) {
            updateToolbar(entry.toolbar);
        }
    }
    m_window->layout()->activate();
}

void ToolbarManager::updateMenuVisibility()
{
    m_menuBar->setVisible(m_menuVisible);
    m_menuToolbar->setVisible(m_menuVisible && m_menuInToolbar);
}

void ToolbarManager::addToolbar(const QString& key, const QString& id, const QJsonObject& layout)
{
    if(!m_widgetProvider->supportsToolbar(key)) {
        return;
    }

    auto* toolbar = createToolbar(m_widgetProvider->displayName(key), id);
    if(!insertWidget(toolbar, key, nullptr, layout)) {
        removeToolbar(toolbar);
    }
}

QToolBar* ToolbarManager::createToolbar(const QString& title, const QString& id)
{
    auto* toolbar = new QToolBar{title, m_window};

    const QString toolbarId
        = id.isEmpty() ? u"Fooyin.Toolbar.%1"_s.arg(UId::create().toString(QUuid::WithoutBraces)) : id;
    toolbar->setObjectName(toolbarId);
    toolbar->setAllowedAreas(Qt::AllToolBarAreas);
    toolbar->setFloatable(false);
    toolbar->setMovable(!m_toolbarsLocked);
    m_window->addToolBar(Qt::TopToolBarArea, toolbar);
    m_toolbars.push_back({.id = toolbarId, .toolbar = toolbar, .items = {}});

    return toolbar;
}

bool ToolbarManager::insertWidget(QToolBar* toolbar, const QString& key, QAction* before, const QJsonObject& layout)
{
    auto* entry = entryFor(toolbar);
    if(!entry || !m_widgetProvider->supportsToolbar(key)) {
        return false;
    }

    if(!(toolbar->allowedAreas() & m_widgetProvider->toolbarAreas(key))) {
        return false;
    }

    auto* widget = m_widgetProvider->createWidget(key);
    if(!widget) {
        return false;
    }

    if(layout.isEmpty()) {
        const auto defaults = m_widgetProvider->toolbarDefaults(key);
        if(!defaults.isEmpty()) {
            widget->loadLayout(defaults);
        }
    }
    else {
        const auto data = layout.constBegin();
        if(data != layout.constEnd() && data->isObject()) {
            widget->loadLayout(data->toObject());
        }
    }

    auto* action = before ? toolbar->insertWidget(before, widget) : toolbar->addWidget(widget);
    const auto itemPosition
        = before ? std::ranges::find_if(entry->items, [before](const auto& item) { return item.action == before; })
                 : entry->items.end();
    entry->items.insert(itemPosition, {.key = key, .action = action, .widget = widget, .savedWidget = {}});
    updateAllowedAreas(entry);
    widget->finalise();
    return true;
}

void ToolbarManager::insertMissingWidget(QToolBar* toolbar, const QString& key, QAction* before,
                                         const QJsonObject& layout)
{
    auto* entry = entryFor(toolbar);
    if(!entry) {
        return;
    }

    auto* label  = new QLabel{tr("Missing Widget: %1").arg(key), toolbar};
    auto* action = before ? toolbar->insertWidget(before, label) : toolbar->addWidget(label);
    const auto itemPosition
        = before ? std::ranges::find_if(entry->items, [before](const auto& item) { return item.action == before; })
                 : entry->items.end();
    entry->items.insert(itemPosition, {.key = key, .action = action, .widget = nullptr, .savedWidget = layout});
}

void ToolbarManager::insertSeparator(QToolBar* toolbar, QAction* before)
{
    auto* entry = entryFor(toolbar);
    if(!entry) {
        return;
    }

    auto* action = before ? toolbar->insertSeparator(before) : toolbar->addSeparator();
    const auto itemPosition
        = before ? std::ranges::find_if(entry->items, [before](const auto& item) { return item.action == before; })
                 : entry->items.end();
    entry->items.insert(itemPosition, {.key = {}, .action = action, .widget = nullptr, .savedWidget = {}});
}

void ToolbarManager::removeItem(QToolBar* toolbar, QAction* action)
{
    auto* entry = entryFor(toolbar);
    if(!entry || !action) {
        return;
    }

    const auto item
        = std::ranges::find_if(entry->items, [action](const auto& candidate) { return candidate.action == action; });
    if(item == entry->items.end()) {
        return;
    }

    entry->items.erase(item);
    delete action;

    if(std::ranges::none_of(entry->items, [](const auto& toolbarItem) { return !toolbarItem.key.isEmpty(); })) {
        removeToolbar(toolbar);
    }
    else {
        updateAllowedAreas(entry);
    }
}

void ToolbarManager::updateAllowedAreas(ToolbarEntry* entry)
{
    if(!entry || !entry->toolbar) {
        return;
    }

    Qt::ToolBarAreas areas{Qt::AllToolBarAreas};
    for(const auto& item : entry->items) {
        if(item.widget) {
            areas &= m_widgetProvider->toolbarAreas(item.key);
        }
    }
    entry->toolbar->setAllowedAreas(areas);
}

void ToolbarManager::removeToolbar(QToolBar* toolbar)
{
    const auto it = std::ranges::find_if(m_toolbars, [toolbar](const auto& entry) { return entry.toolbar == toolbar; });
    if(it == m_toolbars.end()) {
        return;
    }
    m_toolbars.erase(it);
    m_window->removeToolBar(toolbar);
    toolbar->deleteLater();
}

void ToolbarManager::showContextMenu(QToolBar* toolbar, const QPoint& toolbarPos, const QPoint& globalPos,
                                     QWidget* source)
{
    auto* menu = new QMenu{m_window};
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(new MenuHeaderAction(tr("Toolbar"), menu));

    auto* toolbarsMenu = menu->addMenu(tr("Toolbars"));
    populateToolbarsMenu(toolbarsMenu);

    auto* selectedWidget = populateItemContextMenu(menu, toolbar, toolbarPos, source);

    menu->addSeparator();

    menu->addAction(m_lockToolbarsCommand->action());

    if(toolbar != m_menuToolbar) {
        auto* remove = menu->addAction(tr("Remove toolbar"));
        QObject::connect(remove, &QAction::triggered, menu, [this, toolbar] { removeToolbar(toolbar); });
    }

    if(selectedWidget) {
        const auto actionCount = menu->actions().size();
        auto* header           = new MenuHeaderAction(selectedWidget->name(), menu);
        menu->addAction(header);
        selectedWidget->populateContextMenu(menu);
        if(menu->actions().size() == actionCount + 1) {
            delete header;
        }
    }

    menu->popup(globalPos);
}

QAction* ToolbarManager::contextAction(QToolBar* toolbar, const ToolbarEntry& entry, const QPoint& toolbarPos,
                                       QWidget* source)
{
    QAction* selectedAction{nullptr};
    if(source && source != toolbar) {
        const auto toolbarActions = toolbar->actions();
        const auto sourceAction   = std::ranges::find_if(toolbarActions, [toolbar, source](QAction* action) {
            const auto* actionWidget = toolbar->widgetForAction(action);
            return actionWidget && (actionWidget == source || actionWidget->isAncestorOf(source));
        });
        if(sourceAction != toolbarActions.cend()) {
            selectedAction = *sourceAction;
        }
    }
    if(!selectedAction) {
        selectedAction = toolbar->actionAt(toolbarPos);
    }
    if(selectedAction) {
        return selectedAction;
    }

    const auto nearbySeparator = std::ranges::find_if(entry.items, [toolbar, toolbarPos](const auto& item) {
        return item.action && item.action->isSeparator()
            && toolbar->actionGeometry(item.action).adjusted(-4, -4, 4, 4).contains(toolbarPos);
    });
    return nearbySeparator != entry.items.cend() ? nearbySeparator->action.data() : nullptr;
}

FyWidget* ToolbarManager::populateItemContextMenu(QMenu* menu, QToolBar* toolbar, const QPoint& toolbarPos,
                                                  QWidget* source)
{
    if(toolbar == m_menuToolbar) {
        return nullptr;
    }

    const auto* entry = entryFor(toolbar);
    if(!entry) {
        return nullptr;
    }

    auto* selectedAction    = contextAction(toolbar, *entry, toolbarPos, source);
    const auto selectedItem = std::ranges::find_if(
        entry->items, [selectedAction](const auto& item) { return item.action == selectedAction; });
    if(selectedItem == entry->items.cend()) {
        auto* addWidget = menu->addMenu(tr("Add widget"));
        populateInsertWidgetMenu(addWidget, toolbar, nullptr);

        auto* addSeparator = menu->addAction(tr("Add separator"));
        QObject::connect(addSeparator, &QAction::triggered, menu,
                         [this, toolbar] { insertSeparator(toolbar, nullptr); });
        return nullptr;
    }

    const auto next = std::next(selectedItem);
    QAction* after  = next != entry->items.cend() ? next->action.data() : nullptr;

    auto* insertBefore = menu->addMenu(tr("Insert widget before"));
    populateInsertWidgetMenu(insertBefore, toolbar, selectedAction);

    auto* insertAfter = menu->addMenu(tr("Insert widget after"));
    populateInsertWidgetMenu(insertAfter, toolbar, after);

    auto* separatorBefore = menu->addAction(tr("Insert separator before"));
    QObject::connect(separatorBefore, &QAction::triggered, menu,
                     [this, toolbar, selectedAction] { insertSeparator(toolbar, selectedAction); });

    auto* separatorAfter = menu->addAction(tr("Insert separator after"));
    QObject::connect(separatorAfter, &QAction::triggered, menu,
                     [this, toolbar, after] { insertSeparator(toolbar, after); });

    const QString removeText = selectedAction->isSeparator() ? tr("Remove separator") : tr("Remove widget");
    auto* remove             = menu->addAction(removeText);
    QObject::connect(remove, &QAction::triggered, menu,
                     [this, toolbar, selectedAction] { removeItem(toolbar, selectedAction); });

    return selectedItem->widget;
}

void ToolbarManager::populateToolbarsMenu(QMenu* menu)
{
    m_widgetProvider->setupToolbarWidgetMenu(menu, [this](const QString& key) { addToolbar(key); });
}

void ToolbarManager::populateInsertWidgetMenu(QMenu* menu, QToolBar* toolbar, QAction* before)
{
    m_widgetProvider->setupToolbarWidgetMenu(
        menu, [this, toolbar, before](const QString& key) { insertWidget(toolbar, key, before); });
}

ToolbarManager::ToolbarEntry* ToolbarManager::entryFor(QToolBar* toolbar)
{
    const auto entry
        = std::ranges::find_if(m_toolbars, [toolbar](const auto& candidate) { return candidate.toolbar == toolbar; });
    return entry != m_toolbars.end() ? &*entry : nullptr;
}

const ToolbarManager::ToolbarEntry* ToolbarManager::entryFor(const QToolBar* toolbar) const
{
    const auto entry
        = std::ranges::find_if(m_toolbars, [toolbar](const auto& candidate) { return candidate.toolbar == toolbar; });
    return entry != m_toolbars.end() ? &*entry : nullptr;
}

QToolBar* ToolbarManager::toolbarFor(QObject* object) const
{
    auto* widget = qobject_cast<QWidget*>(object);
    while(widget) {
        if(auto* toolbar = qobject_cast<QToolBar*>(widget)) {
            if(toolbar == m_menuToolbar
               || std::ranges::any_of(m_toolbars, [toolbar](const auto& entry) { return entry.toolbar == toolbar; })) {
                return toolbar;
            }
            return nullptr;
        }
        widget = widget->parentWidget();
    }
    return nullptr;
}
} // namespace Fooyin

#include "moc_toolbarmanager.cpp"
