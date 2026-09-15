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

#include <gui/widgetcontainer.h>
#include <gui/widgets/singletabbedwidget.h>

#include <QPointer>

class QVBoxLayout;

namespace Fooyin {
class LibraryFilter;
class MusicLibrary;
class SettingsManager;

namespace Filters {
class LibraryFilterRegistry;

class FYGUI_EXPORT LibraryFilterTabs : public WidgetContainer
{
    Q_OBJECT

public:
    LibraryFilterTabs(LibraryFilterRegistry* registry, MusicLibrary* library, WidgetProvider* widgetProvider,
                      SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void setupTabs();
    int addFilter(int id);
    void removeFilter(int id);

    int addNewTab(const QString& name);
    int addNewTab(const QString& name, const QIcon& icon);

    [[nodiscard]] bool canAddWidget() const override;
    [[nodiscard]] bool canMoveWidget(int index, int newIndex) const override;
    [[nodiscard]] int widgetIndex(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtId(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtIndex(int index) const override;
    [[nodiscard]] FyWidget* widgetAtPosition(const QPoint& pos) const override;
    [[nodiscard]] QRect widgetGeometry(FyWidget* widget) const override;
    [[nodiscard]] int widgetCount() const override;
    [[nodiscard]] WidgetList widgets() const override;

    int addWidget(FyWidget* widget) override;
    void insertWidget(int index, FyWidget* widget) override;
    void removeWidget(int index) override;
    void replaceWidget(int index, FyWidget* newWidget) override;
    void moveWidget(int index, int newIndex) override;

private:
    void filterChanged(const LibraryFilter& filter);
    void activateCurrent();
    void tabMoved(int from, int to);
    void tabRenamed(int index, const QString& text);
    void showContextMenu(const QPoint& pos);

    LibraryFilterRegistry* m_registry;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QVBoxLayout* m_layout;
    SingleTabbedWidget* m_tabs;
    QPointer<FyWidget> m_tabsWidget;

    QString m_allLibraryName;
    int m_allLibraryIndex;
};
} // namespace Filters
} // namespace Fooyin
