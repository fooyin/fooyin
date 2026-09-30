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

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSizePolicy>

#include <vector>

class QMainWindow;
class QMenu;
class QMenuBar;
class QAction;
class QToolBar;
class QWidget;

namespace Fooyin {
class ActionManager;
class Command;
class FyWidget;
class SettingsManager;
class WidgetProvider;

class ToolbarManager : public QObject
{
    Q_OBJECT

public:
    ToolbarManager(QMainWindow* window, QMenuBar* menuBar, ActionManager* actionManager, WidgetProvider* widgetProvider,
                   SettingsManager* settings, QObject* parent = nullptr);
    ~ToolbarManager() override;

    [[nodiscard]] QJsonArray saveLayout() const;
    [[nodiscard]] QByteArray saveState() const;
    void loadLayout(const QJsonArray& toolbars, const QByteArray& state);
    void clear();

    [[nodiscard]] std::vector<FyWidget*> widgets() const;
    void setMenuVisible(bool visible);

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ToolbarItem
    {
        QString key;
        QPointer<QAction> action;
        QPointer<FyWidget> widget;
        QJsonObject savedWidget;
    };

    struct ToolbarEntry
    {
        QString id;
        QPointer<QToolBar> toolbar;
        std::vector<ToolbarItem> items;
    };

    void setEditing(bool editing);
    void setMenuInToolbar(bool inToolbar);
    void setToolbarsLocked(bool locked);

    void updateMenuVisibility();

    void addToolbar(const QString& key, const QString& id = {}, const QJsonObject& layout = {});
    QToolBar* createToolbar(const QString& title, const QString& id);

    bool insertWidget(QToolBar* toolbar, const QString& key, QAction* before = nullptr, const QJsonObject& layout = {});
    void insertMissingWidget(QToolBar* toolbar, const QString& key, QAction* before, const QJsonObject& layout);
    void insertSeparator(QToolBar* toolbar, QAction* before);

    void removeItem(QToolBar* toolbar, QAction* action);
    void updateAllowedAreas(ToolbarEntry* entry);
    void removeToolbar(QToolBar* toolbar);

    void showContextMenu(QToolBar* toolbar, const QPoint& toolbarPos, const QPoint& globalPos, QWidget* source);
    static QAction* contextAction(QToolBar* toolbar, const ToolbarEntry& entry, const QPoint& toolbarPos,
                                  QWidget* source);
    FyWidget* populateItemContextMenu(QMenu* menu, QToolBar* toolbar, const QPoint& toolbarPos, QWidget* source);
    void populateToolbarsMenu(QMenu* menu);
    void populateInsertWidgetMenu(QMenu* menu, QToolBar* toolbar, QAction* before);

    ToolbarEntry* entryFor(QToolBar* toolbar);
    [[nodiscard]] const ToolbarEntry* entryFor(const QToolBar* toolbar) const;
    [[nodiscard]] QToolBar* toolbarFor(QObject* object) const;

    QMainWindow* m_window;
    QMenuBar* m_menuBar;
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;
    QToolBar* m_menuToolbar;
    QAction* m_menuAction;
    QAction* m_menuBarMovableAction;
    QAction* m_lockToolbarsAction;
    Command* m_menuBarMovableCommand;
    Command* m_lockToolbarsCommand;
    QMenu* m_toolbarsMenu;
    std::vector<ToolbarEntry> m_toolbars;
    QSizePolicy m_menuBarSizePolicy;
    bool m_menuBarNative;
    bool m_menuInToolbar;
    bool m_menuVisible;
    bool m_toolbarsLocked;
    bool m_editing;
};
} // namespace Fooyin
