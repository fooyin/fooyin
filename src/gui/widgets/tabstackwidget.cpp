/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
#include <utils/enum.h>
#include <utils/widgets/editabletabbar.h>
#include <utils/widgets/editabletabwidget.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct TabStackWidget::Private
{
    TabStackWidget* self;

    ActionManager* actionManager;
    WidgetProvider* widgetProvider;

    std::vector<FyWidget*> widgets;
    EditableTabWidget* tabs;

    Private(TabStackWidget* self, ActionManager* actionManager, WidgetProvider* widgetProvider)
        : self{self}
        , actionManager{actionManager}
        , widgetProvider{widgetProvider}
        , tabs{new EditableTabWidget(self)}
    { }

    int indexOfWidget(FyWidget* widget)
    {
        auto it = std::ranges::find(std::as_const(widgets), widget);
        if(it != widgets.cend()) {
            return static_cast<int>(std::distance(widgets.cbegin(), it));
        }
        return -1;
    }

    void changeTabPosition(QTabWidget::TabPosition position) const
    {
        tabs->setTabPosition(position);
        tabs->adjustSize();
    };
};

TabStackWidget::TabStackWidget(ActionManager* actionManager, WidgetProvider* widgetProvider, QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider)}
{
    QObject::setObjectName(TabStackWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->tabs);

    QObject::connect(p->tabs->tabBar(), &QTabBar::tabMoved, this, [this](int from, int to) {
        if(from >= 0 && from < static_cast<int>(p->widgets.size())) {
            auto* widget = p->widgets.at(from);
            p->widgets.erase(p->widgets.begin() + from);
            p->widgets.insert(p->widgets.begin() + to, widget);
        }
    });
}

TabStackWidget::~TabStackWidget() = default;

QString TabStackWidget::name() const
{
    return QStringLiteral("Tab Stack");
}

QString TabStackWidget::layoutName() const
{
    return QStringLiteral("TabStack");
}

void TabStackWidget::layoutEditingMenu(ActionContainer* menu)
{
    const QString addTitle{tr("&Add")};
    auto addMenuId = id().append(addTitle);

    auto* addMenu = p->actionManager->createMenu(addMenuId);
    addMenu->menu()->setTitle(addTitle);

    p->widgetProvider->setupWidgetMenu(addMenu, [this](FyWidget* newWidget) { addWidget(newWidget); });
    menu->addMenu(addMenu);
}

void TabStackWidget::saveLayout(QJsonArray& array)
{
    QJsonArray widgets;
    for(FyWidget* widget : p->widgets) {
        widget->saveLayout(widgets);
    }

    QString state;
    for(int i{0}; i < p->tabs->count(); ++i) {
        if(!state.isEmpty()) {
            state.append(Constants::Separator);
        }
        state.append(p->tabs->tabText(i));
    }

    QJsonObject options;
    options["Children"_L1] = widgets;
    options["State"_L1]    = state;
    options["Position"_L1] = Utils::Enum::toString(p->tabs->tabPosition());

    QJsonObject tabStack;
    tabStack[layoutName()] = options;
    array.append(tabStack);
}

void TabStackWidget::loadLayout(const QJsonObject& object)
{
    const auto position = Utils::Enum::fromString<QTabWidget::TabPosition>(object.value("Position"_L1).toString());
    if(position) {
        p->tabs->setTabPosition(*position);
    }

    const auto widgets = object.value("Children"_L1).toArray();

    WidgetContainer::loadWidgets(widgets);

    const auto state         = object.value("State"_L1).toString();
    const QStringList titles = state.split(Constants::Separator);

    for(int i{0}; const QString& title : titles) {
        if(i < p->tabs->count()) {
            p->tabs->setTabText(i++, title);
        }
    }
}

void TabStackWidget::addWidget(FyWidget* widget)
{
    const int index = p->tabs->addTab(widget, widget->name());
    p->widgets.insert(p->widgets.begin() + index, widget);
}

void TabStackWidget::removeWidget(FyWidget* widget)
{
    const int index = p->indexOfWidget(widget);
    if(index >= 0) {
        p->tabs->removeTab(index);
        p->widgets.erase(p->widgets.begin() + index);
    }
}

void TabStackWidget::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    const int index = p->indexOfWidget(oldWidget);
    if(index >= 0) {
        p->tabs->removeTab(index);
        p->tabs->insertTab(index, newWidget, newWidget->name());

        p->widgets.erase(p->widgets.begin() + index);
        p->widgets.insert(p->widgets.begin() + index, newWidget);

        p->tabs->setCurrentIndex(index);
    }
}

void TabStackWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QPoint point = p->tabs->tabBar()->mapFrom(this, event->pos());
    const int index    = p->tabs->tabBar()->tabAt(point);

    if(index < 0 || index >= p->tabs->count()) {
        FyWidget::contextMenuEvent(event);
        return;
    }

    auto* widget = p->widgets.at(index);

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

    const auto tabPos = p->tabs->tabPosition();

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

    QObject::connect(north, &QAction::triggered, this, [this]() { p->changeTabPosition(QTabWidget::North); });
    QObject::connect(east, &QAction::triggered, this, [this]() { p->changeTabPosition(QTabWidget::East); });
    QObject::connect(south, &QAction::triggered, this, [this]() { p->changeTabPosition(QTabWidget::South); });
    QObject::connect(west, &QAction::triggered, this, [this]() { p->changeTabPosition(QTabWidget::West); });

    positionGroup->addAction(north);
    positionGroup->addAction(east);
    positionGroup->addAction(south);
    positionGroup->addAction(west);

    posMenu->addAction(north);
    posMenu->addAction(east);
    posMenu->addAction(south);
    posMenu->addAction(west);

    auto* rename = new QAction(tr("&Rename"), menu);
    QObject::connect(rename, &QAction::triggered, p->tabs->editableTabBar(), &EditableTabBar::showEditor);

    auto* remove = new QAction(tr("Re&move"), menu);
    QObject::connect(remove, &QAction::triggered, this, [this, widget]() { removeWidget(widget); });

    menu->addMenu(posMenu);
    menu->addSeparator();
    menu->addAction(rename);
    menu->addAction(remove);

    menu->popup(p->tabs->tabBar()->mapToGlobal(point));
}
} // namespace Fooyin

#include "moc_tabstackwidget.cpp"
