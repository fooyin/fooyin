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

#include "libraryfiltertabs.h"

#include "filterconstants.h"

#include <core/library/libraryfilter.h>
#include <core/library/libraryfilterregistry.h>
#include <core/library/musiclibrary.h>
#include <gui/widgets/editabletabbar.h>
#include <utils/itemregistry.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
LibraryFilterTabs::LibraryFilterTabs(LibraryFilterRegistry* registry, MusicLibrary* library,
                                     WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, settings, parent}
    , m_registry{registry}
    , m_library{library}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
    , m_tabs{new SingleTabbedWidget(this)}
    , m_allLibraryName{tr("All")}
    , m_allLibraryIndex{0}
{
    QObject::setObjectName(LibraryFilterTabs::name());

    m_layout->setContentsMargins({});
    m_layout->setAlignment(Qt::AlignTop);
    m_layout->addWidget(m_tabs);

    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    m_tabs->tabBar()->setEditTitle(tr("Library filter"));
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(m_tabs, &SingleTabbedWidget::currentChanged, this, &LibraryFilterTabs::activateCurrent);
    QObject::connect(m_tabs, &SingleTabbedWidget::tabMoved, this, &LibraryFilterTabs::tabMoved);
    QObject::connect(m_tabs->tabBar(), &EditableTabBar::tabTextChanged, this, &LibraryFilterTabs::tabRenamed);
    QObject::connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, &LibraryFilterTabs::showContextMenu);
    QObject::connect(m_registry, &RegistryBase::itemAdded, this, [this]() { setupTabs(); });
    QObject::connect(m_registry, &LibraryFilterRegistry::libraryFilterChanged, this, &LibraryFilterTabs::filterChanged);
    QObject::connect(m_registry, &RegistryBase::itemRemoved, this, [this](int id) {
        auto activeFilters = m_library->activeLibraryFilters();
        const auto active  = std::ranges::find(activeFilters, id, &LibraryFilter::id);
        if(active != activeFilters.end()) {
            activeFilters.erase(active);
            m_library->setActiveLibraryFilters(std::move(activeFilters));
        }
        setupTabs();
    });
    QObject::connect(m_library, &MusicLibrary::activeLibraryFiltersChanged, this, [this]() { setupTabs(); });
}

QString LibraryFilterTabs::name() const
{
    return tr("Saved Filter Tabs");
}

QString LibraryFilterTabs::layoutName() const
{
    return u"SavedFilterTabs"_s;
}

void LibraryFilterTabs::saveLayoutData(QJsonObject& layout)
{
    layout["AllLibraryName"_L1]  = m_allLibraryName;
    layout["AllLibraryIndex"_L1] = m_allLibraryIndex;
    layout["TabPosition"_L1]     = static_cast<int>(m_tabs->tabPosition());

    if(m_tabsWidget) {
        QJsonArray children;
        m_tabsWidget->saveLayout(children);
        layout["Widgets"_L1] = children;
    }
}

void LibraryFilterTabs::loadLayoutData(const QJsonObject& layout)
{
    if(const QString name = layout.value("AllLibraryName"_L1).toString().trimmed(); !name.isEmpty()) {
        m_allLibraryName = name;
    }
    if(layout.contains("AllLibraryIndex"_L1)) {
        m_allLibraryIndex = layout.value("AllLibraryIndex"_L1).toInt();
    }
    if(layout.contains("TabPosition"_L1)) {
        const auto position = static_cast<SingleTabbedWidget::TabPosition>(layout.value("TabPosition"_L1).toInt());
        m_tabs->setTabPosition(position);
    }

    if(layout.contains("Widgets"_L1)) {
        WidgetContainer::loadWidgets(layout.value("Widgets"_L1).toArray());
    }
}

void LibraryFilterTabs::finalise()
{
    setupTabs();
}

void LibraryFilterTabs::setupTabs()
{
    const QSignalBlocker blocker{m_tabs};
    while(m_tabs->count() > 0) {
        m_tabs->removeTab(0);
    }

    const auto filters = m_registry->items();
    std::vector<int> enabledFilterIds;
    enabledFilterIds.reserve(filters.size());
    for(const LibraryFilter& filter : filters) {
        if(filter.enabled) {
            enabledFilterIds.push_back(filter.id);
        }
    }

    m_allLibraryIndex = std::clamp(m_allLibraryIndex, 0, static_cast<int>(enabledFilterIds.size()));
    for(int index{0}; std::cmp_less_equal(index, enabledFilterIds.size()); ++index) {
        if(index == m_allLibraryIndex) {
            const int allLibraryIndex = addNewTab(m_allLibraryName);
            m_tabs->tabBar()->setTabData(allLibraryIndex, -1);
        }

        if(std::cmp_less(index, enabledFilterIds.size())) {
            addFilter(enabledFilterIds.at(index));
        }
    }

    int activeIndex{m_allLibraryIndex};
    const LibraryFilterList activeFilters = m_library->activeLibraryFilters();
    if(activeFilters.size() == 1) {
        for(int index{0}; index < m_tabs->count(); ++index) {
            if(m_tabs->tabBar()->tabData(index).toInt() == activeFilters.front().id) {
                activeIndex = index;
                break;
            }
        }
    }

    m_tabs->setCurrentIndex(activeIndex);
    // Workaround for issue where QTabBar is scrolled to the right when initialised, hiding tabs before current.
    m_tabs->tabBar()->adjustSize();
}

int LibraryFilterTabs::addFilter(int id)
{
    const auto filter = m_registry->itemById(id);
    if(!filter || !filter->enabled) {
        return -1;
    }

    const int index = addNewTab(filter->name);
    if(index >= 0) {
        m_tabs->tabBar()->setTabData(index, QVariant::fromValue(filter->id));
        const LibraryFilterList activeFilters = m_library->activeLibraryFilters();
        if(activeFilters.size() == 1 && activeFilters.front().id == filter->id) {
            m_tabs->setCurrentIndex(index);
        }
    }

    return index;
}

void LibraryFilterTabs::removeFilter(int id)
{
    for(int i{0}; i < m_tabs->count(); ++i) {
        if(m_tabs->tabBar()->tabData(i).toInt() == id) {
            m_tabs->removeTab(i);
        }
    }
}

int LibraryFilterTabs::addNewTab(const QString& name)
{
    return addNewTab(name, {});
}

int LibraryFilterTabs::addNewTab(const QString& name, const QIcon& icon)
{
    if(name.isEmpty()) {
        return -1;
    }

    const int index = m_tabs->addTab(icon, name);

    return index;
}

bool LibraryFilterTabs::canAddWidget() const
{
    return !m_tabsWidget;
}

bool LibraryFilterTabs::canMoveWidget(int /*index*/, int /*newIndex*/) const
{
    return false;
}

int LibraryFilterTabs::widgetIndex(const Id& id) const
{
    if(!id.isValid() || !m_tabsWidget) {
        return -1;
    }

    if(m_tabsWidget->id() == id) {
        return 0;
    }

    return -1;
}

FyWidget* LibraryFilterTabs::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    if(!m_tabsWidget || m_tabsWidget->id() != id) {
        return nullptr;
    }

    return m_tabsWidget;
}

FyWidget* LibraryFilterTabs::widgetAtIndex(int index) const
{
    if(index != 0) {
        return nullptr;
    }

    return m_tabsWidget;
}

FyWidget* LibraryFilterTabs::widgetAtPosition(const QPoint& pos) const
{
    const QRect tabBarRect{m_tabs->tabBar()->mapTo(this, QPoint{}), m_tabs->tabBar()->size()};
    if(tabBarRect.contains(pos)) {
        return const_cast<LibraryFilterTabs*>(this); // NOLINT
    }

    if(!m_tabsWidget) {
        return nullptr;
    }

    const QRect tabsRect{m_tabs->pos(), m_tabs->size()};
    if(!tabsRect.contains(pos)) {
        return nullptr;
    }

    return m_tabsWidget;
}

QRect LibraryFilterTabs::widgetGeometry(FyWidget* widget) const
{
    if(widget == this) {
        return {QPoint{}, size()};
    }
    if(widget != m_tabsWidget) {
        return {};
    }

    return {m_tabsWidget->mapTo(this, QPoint{}), m_tabsWidget->size()};
}

int LibraryFilterTabs::widgetCount() const
{
    return m_tabsWidget ? 1 : 0;
}

WidgetList LibraryFilterTabs::widgets() const
{
    if(!m_tabsWidget) {
        return {};
    }

    return {m_tabsWidget};
}

int LibraryFilterTabs::addWidget(FyWidget* widget)
{
    m_tabsWidget = widget;
    m_tabs->setWidget(m_tabsWidget);

    return 0;
}

void LibraryFilterTabs::insertWidget(int index, FyWidget* widget)
{
    if(m_tabsWidget) {
        return;
    }

    if(index != 0) {
        return;
    }

    addWidget(widget);
}

void LibraryFilterTabs::removeWidget(int index)
{
    if(index == 0 && m_tabsWidget) {
        m_tabs->setWidget(nullptr);
        m_tabsWidget = nullptr;
    }
}

void LibraryFilterTabs::replaceWidget(int index, FyWidget* newWidget)
{
    if(index != 0) {
        return;
    }

    m_tabsWidget = newWidget;
    m_tabs->setWidget(m_tabsWidget);
}

void LibraryFilterTabs::moveWidget(int /*index*/, int /*newIndex*/) { }

void LibraryFilterTabs::filterChanged(const LibraryFilter& filter)
{
    auto activeFilters = m_library->activeLibraryFilters();
    const auto active  = std::ranges::find(activeFilters, filter.id, &LibraryFilter::id);
    if(active != activeFilters.end()) {
        if(filter.enabled) {
            *active = filter;
        }
        else {
            activeFilters.erase(active);
        }
        m_library->setActiveLibraryFilters(std::move(activeFilters));
    }

    setupTabs();
}

void LibraryFilterTabs::tabMoved(int /*from*/, int to)
{
    const int movedId = m_tabs->tabBar()->tabData(to).toInt();

    std::vector<int> visibleFilterIds;
    for(int index{0}; index < m_tabs->count(); ++index) {
        const int id = m_tabs->tabBar()->tabData(index).toInt();
        if(id < 0) {
            m_allLibraryIndex = index;
        }
        else {
            visibleFilterIds.push_back(id);
        }
    }

    std::vector<int> currentVisibleFilterIds;
    auto filters = m_registry->items();
    for(const LibraryFilter& filter : filters) {
        if(filter.enabled) {
            currentVisibleFilterIds.push_back(filter.id);
        }
    }
    if(visibleFilterIds == currentVisibleFilterIds) {
        return;
    }

    const auto movedPosition = std::ranges::find(visibleFilterIds, movedId);
    if(movedId < 0 || movedPosition == visibleFilterIds.end()) {
        return;
    }

    const auto moved = std::ranges::find(filters, movedId, &LibraryFilter::id);
    if(moved == filters.end()) {
        return;
    }

    LibraryFilter movedFilter{*moved};
    filters.erase(moved);

    auto insertion = filters.end();
    if(movedPosition + 1 != visibleFilterIds.end()) {
        const int nextId = *(movedPosition + 1);
        insertion        = std::ranges::find(filters, nextId, &LibraryFilter::id);
    }
    else if(movedPosition != visibleFilterIds.begin()) {
        const int previousId = *(movedPosition - 1);
        const auto previous  = std::ranges::find(filters, previousId, &LibraryFilter::id);
        if(previous != filters.end()) {
            insertion = previous + 1;
        }
    }
    filters.insert(insertion, std::move(movedFilter));

    for(int index{0}; std::cmp_less(index, filters.size()); ++index) {
        LibraryFilter& filter = filters.at(index);
        if(filter.index != index) {
            filter.index = index;
            m_registry->changeItem(filter);
        }
    }
}

void LibraryFilterTabs::tabRenamed(int index, const QString& text)
{
    const int id = m_tabs->tabBar()->tabData(index).toInt();
    if(id < 0) {
        m_allLibraryName = text;
        return;
    }

    if(auto filter = m_registry->itemById(id)) {
        filter->name = text;
        if(!m_registry->changeItem(*filter)) {
            setupTabs();
        }
    }
}

void LibraryFilterTabs::activateCurrent()
{
    const int index = m_tabs->currentIndex();
    const int id    = m_tabs->tabBar()->tabData(index).toInt();
    if(id < 0) {
        m_library->clearActiveLibraryFilters();
        return;
    }

    if(const auto preset = m_registry->itemById(id)) {
        m_library->setActiveLibraryFilters({*preset});
    }
}

void LibraryFilterTabs::showContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const int index = m_tabs->tabBar()->tabAt(pos);
    if(index >= 0) {
        auto* rename = menu->addAction(tr("Rename tab"));
        QObject::connect(rename, &QAction::triggered, m_tabs->tabBar(),
                         [tabBar = m_tabs->tabBar(), index]() { tabBar->showEditor(index); });
    }

    menu->addSeparator();

    auto* positionMenu  = menu->addMenu(tr("Tabs position"));
    auto* positionGroup = new QActionGroup(positionMenu);

    const auto addPositionAction = [this, positionMenu, positionGroup](const QString& text,
                                                                       SingleTabbedWidget::TabPosition position) {
        auto* action = positionMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(m_tabs->tabPosition() == position);
        positionGroup->addAction(action);
        QObject::connect(action, &QAction::triggered, this, [this, position]() { m_tabs->setTabPosition(position); });
    };

    addPositionAction(tr("Top"), SingleTabbedWidget::TabPosition::Top);
    addPositionAction(tr("Bottom"), SingleTabbedWidget::TabPosition::Bottom);

    menu->addSeparator();

    auto* manage = menu->addAction(tr("Manage library filters…"));
    QObject::connect(manage, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Id{Constants::Page::Filters}); });

    menu->popup(m_tabs->tabBar()->mapToGlobal(pos));
}
} // namespace Fooyin::Filters

#include "moc_libraryfiltertabs.cpp"
