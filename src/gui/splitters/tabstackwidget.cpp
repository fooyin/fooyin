/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "tabstackwidget.h"

#include <core/constants.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/editabletabbar.h>
#include <gui/widgets/editabletabwidget.h>
#include <utils/enum.h>
#include <utils/helpers.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
TabStackWidget::TabStackWidget(WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, settings, parent}
    , m_tabs{new EditableTabWidget(this)}
{
    QObject::setObjectName(TabStackWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_tabs);

    m_tabs->editableTabBar()->setEditTitle(tr("Playlist"));

    QObject::connect(m_tabs, &EditableTabWidget::tabBarClicked, this, [this](int index) {
        if(index != m_tabs->editableTabBar()->currentIndex()) {
            m_tabs->editableTabBar()->closeEditor();
        }
    });
    QObject::connect(m_tabs->tabBar(), &QTabBar::tabMoved, this, [this](int from, int to) {
        if(from >= 0 && from < static_cast<int>(m_widgets.size())) {
            auto* widget = m_widgets.at(from);
            m_widgets.erase(m_widgets.begin() + from);
            m_widgets.insert(m_widgets.begin() + to, widget);
        }
    });
}

QString TabStackWidget::name() const
{
    return tr("Tab Stack");
}

QString TabStackWidget::layoutName() const
{
    return u"TabStack"_s;
}

void TabStackWidget::saveLayoutData(QJsonObject& layout)
{
    QJsonArray widgets;
    for(FyWidget* widget : m_widgets) {
        widget->saveLayout(widgets);
    }

    QString state;
    for(int i{0}; i < m_tabs->count(); ++i) {
        if(!state.isEmpty()) {
            state.append(QLatin1String{Constants::UnitSeparator});
        }
        state.append(m_tabs->tabText(i));
    }

    layout["Position"_L1] = Utils::Enum::toString(m_tabs->tabPosition());
    layout["State"_L1]    = state;
    layout["Widgets"_L1]  = widgets;
}

void TabStackWidget::loadLayoutData(const QJsonObject& layout)
{
    if(const auto position = Utils::Enum::fromString<QTabWidget::TabPosition>(layout.value("Position"_L1).toString())) {
        changeTabPosition(position.value());
    }

    if(layout.contains("Widgets"_L1)) {
        const auto widgets = layout.value("Widgets"_L1).toArray();
        WidgetContainer::loadWidgets(widgets);
    }

    if(layout.contains("State"_L1)) {
        const auto state         = layout.value("State"_L1).toString();
        const QStringList titles = state.split(QLatin1String{Constants::UnitSeparator});

        for(int i{0}; const QString& title : titles) {
            if(i < m_tabs->count()) {
                m_tabs->setTabText(i++, title);
            }
        }
    }
}

Qt::Orientation TabStackWidget::orientation() const
{
    const auto position = m_tabs->tabPosition();

    if(position == QTabWidget::North || position == QTabWidget::South) {
        return Qt::Vertical;
    }

    return Qt::Horizontal;
}

bool TabStackWidget::canAddWidget() const
{
    return true;
}

bool TabStackWidget::canMoveWidget(int index, int newIndex) const
{
    const auto count = static_cast<int>(m_widgets.size());

    if(index < 0 || index >= count) {
        return false;
    }

    if(newIndex < 0 || newIndex > count) {
        return false;
    }

    if(index == newIndex || (index == count - 1 && newIndex == index + 1)) {
        return false;
    }

    return true;
}

int TabStackWidget::widgetIndex(const Id& id) const
{
    if(!id.isValid()) {
        return -1;
    }

    auto it = std::ranges::find_if(m_widgets, [id](FyWidget* widget) { return widget->id() == id; });
    if(it != m_widgets.cend()) {
        return static_cast<int>(std::distance(m_widgets.cbegin(), it));
    }

    return -1;
}

FyWidget* TabStackWidget::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    const auto widgetIt = std::ranges::find_if(m_widgets, [id](FyWidget* widget) { return widget->id() == id; });
    if(widgetIt != m_widgets.cend()) {
        return *widgetIt;
    }

    return nullptr;
}

FyWidget* TabStackWidget::widgetAtIndex(int index) const
{
    if(index < 0 || std::cmp_greater_equal(index, m_widgets.size())) {
        return nullptr;
    }

    return m_widgets.at(index);
}

int TabStackWidget::widgetCount() const
{
    return static_cast<int>(m_widgets.size());
}

WidgetList TabStackWidget::widgets() const
{
    return m_widgets;
}

int TabStackWidget::addWidget(FyWidget* widget)
{
    const int index = m_tabs->count();
    insertWidget(index, widget);
    return index;
}

void TabStackWidget::insertWidget(int index, FyWidget* widget)
{
    if(index < 0 || index > m_tabs->count()) {
        return;
    }

    const int insertionIndex = m_tabs->insertTab(index, widget, widget->name());
    m_widgets.insert(m_widgets.begin() + insertionIndex, widget);
}

void TabStackWidget::removeWidget(int index)
{
    if(index < 0 || index > m_tabs->count()) {
        return;
    }

    m_tabs->removeTab(index);
    m_widgets.at(index)->deleteLater();
    m_widgets.erase(m_widgets.begin() + index);
}

void TabStackWidget::replaceWidget(int index, FyWidget* newWidget)
{
    if(index < 0 || std::cmp_greater_equal(index, m_widgets.size())) {
        return;
    }

    m_tabs->removeTab(index);
    m_tabs->insertTab(index, newWidget, newWidget->name());

    m_widgets.at(index)->deleteLater();
    m_widgets.erase(m_widgets.begin() + index);
    m_widgets.insert(m_widgets.begin() + index, newWidget);

    m_tabs->setCurrentIndex(index);
}

void TabStackWidget::moveWidget(int index, int newIndex)
{
    auto* widget = m_widgets.at(index);
    Utils::move(m_widgets, index, newIndex);
    m_tabs->removeTab(index);
    m_tabs->insertTab(newIndex, widget, widget->name());
}

void TabStackWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QPoint point = m_tabs->tabBar()->mapFrom(this, event->pos());
    const int index    = m_tabs->tabBar()->tabAt(point);

    if(index < 0 || index >= m_tabs->count()) {
        FyWidget::contextMenuEvent(event);
        return;
    }

    auto* posMenu = new QMenu(tr("&Position"), menu);

    auto* positionGroup = new QActionGroup(menu);

    auto* north = new QAction(tr("&North"), posMenu);
    auto* east  = new QAction(tr("&East"), posMenu);
    auto* south = new QAction(tr("&South"), posMenu);
    auto* west  = new QAction(tr("&West"), posMenu);

    north->setCheckable(true);
    east->setCheckable(true);
    south->setCheckable(true);
    west->setCheckable(true);

    const auto tabPos = m_tabs->tabPosition();

    switch(tabPos) {
        case(QTabWidget::North):
            north->setChecked(true);
            break;
        case(QTabWidget::East):
            east->setChecked(true);
            break;
        case(QTabWidget::South):
            south->setChecked(true);
            break;
        case(QTabWidget::West):
            west->setChecked(true);
            break;
    }

    QObject::connect(north, &QAction::triggered, this, [this]() { changeTabPosition(QTabWidget::North); });
    QObject::connect(east, &QAction::triggered, this, [this]() { changeTabPosition(QTabWidget::East); });
    QObject::connect(south, &QAction::triggered, this, [this]() { changeTabPosition(QTabWidget::South); });
    QObject::connect(west, &QAction::triggered, this, [this]() { changeTabPosition(QTabWidget::West); });

    positionGroup->addAction(north);
    positionGroup->addAction(east);
    positionGroup->addAction(south);
    positionGroup->addAction(west);

    posMenu->addAction(north);
    posMenu->addAction(east);
    posMenu->addAction(south);
    posMenu->addAction(west);

    auto* rename = new QAction(tr("&Rename"), menu);
    QObject::connect(rename, &QAction::triggered, m_tabs->editableTabBar(), &EditableTabBar::showEditor);

    auto* remove = new QAction(tr("Re&move"), menu);
    QObject::connect(remove, &QAction::triggered, this, [this, index]() { removeWidget(index); });

    menu->addMenu(posMenu);
    menu->addSeparator();
    menu->addAction(rename);
    menu->addAction(remove);

    menu->popup(m_tabs->tabBar()->mapToGlobal(point));
}

int TabStackWidget::indexOfWidget(FyWidget* widget) const
{
    auto it = std::ranges::find(m_widgets, widget);
    if(it != m_widgets.cend()) {
        return static_cast<int>(std::distance(m_widgets.cbegin(), it));
    }
    return -1;
}

int TabStackWidget::indexOfWidget(const Id& id) const
{
    const auto widgetIt = std::ranges::find_if(m_widgets, [id](FyWidget* widget) { return widget->id() == id; });
    if(widgetIt != m_widgets.cend()) {
        return static_cast<int>(std::distance(m_widgets.cbegin(), widgetIt));
    }
    return -1;
}

void TabStackWidget::changeTabPosition(QTabWidget::TabPosition position) const
{
    if(m_tabs->tabPosition() == position) {
        return;
    }

    switch(position) {
        case(QTabWidget::North):
        case(QTabWidget::South):
            m_tabs->editableTabBar()->setEditMode(EditableTabBar::EditMode::Inline);
            break;
        case(QTabWidget::West):
        case(QTabWidget::East):
            m_tabs->editableTabBar()->setEditMode(EditableTabBar::EditMode::Dialog);
            break;
    }

    m_tabs->setTabPosition(position);
    m_tabs->adjustSize();
}
} // namespace Fooyin

#include "moc_tabstackwidget.cpp"
