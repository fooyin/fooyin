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

#include <QContextMenuEvent>
#include <QInputDialog>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Gui::Widgets {
struct TabStackWidget::Private
{
    Utils::ActionManager* actionManager;
    WidgetProvider* widgetProvider;

    std::vector<FyWidget*> widgets;
    QTabWidget* tabs;

    Private(TabStackWidget* self, Utils::ActionManager* actionManager, WidgetProvider* widgetProvider)
        : actionManager{actionManager}
        , widgetProvider{widgetProvider}
        , tabs{new QTabWidget(self)}
    {
        tabs->setMovable(true);
    }

    int indexOfWidget(FyWidget* widget)
    {
        auto it = std::ranges::find(std::as_const(widgets), widget);
        if(it != widgets.cend()) {
            return static_cast<int>(std::distance(widgets.cbegin(), it));
        }
        return -1;
    }
};

TabStackWidget::TabStackWidget(Utils::ActionManager* actionManager, WidgetProvider* widgetProvider, QWidget* parent)
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

void TabStackWidget::layoutEditingMenu(Utils::ActionContainer* menu)
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
            state.append(Core::Constants::Separator);
        }
        state.append(p->tabs->tabText(i));
    }

    QJsonObject options;
    options["Children"_L1] = widgets;
    options["State"_L1]    = state;

    QJsonObject tabStack;
    tabStack[layoutName()] = options;
    array.append(tabStack);
}

void TabStackWidget::loadLayout(const QJsonObject& object)
{
    const auto widgets = object.value("Children"_L1).toArray();

    WidgetContainer::loadWidgets(widgets);

    const auto state         = object.value("State"_L1).toString();
    const QStringList titles = state.split(Core::Constants::Separator);

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

    const QPoint point = event->pos();
    const int index    = p->tabs->tabBar()->tabAt(point);

    if(index < 0 || index >= p->tabs->count()) {
        FyWidget::contextMenuEvent(event);
        return;
    }

    auto* widget = p->widgets.at(index);

    auto* rename = new QAction(u"Rename"_s, menu);
    QObject::connect(rename, &QAction::triggered, this, [this, index]() {
        bool success{false};
        const QString text = QInputDialog::getText(this, tr("Rename"), tr("Tab Name:"), QLineEdit::Normal,
                                                   p->tabs->tabText(index), &success);

        if(success && !text.isEmpty()) {
            p->tabs->setTabText(index, text);
        }
    });

    auto* remove = new QAction(u"Remove"_s, menu);
    QObject::connect(remove, &QAction::triggered, this, [this, widget]() { removeWidget(widget); });

    menu->addAction(rename);
    menu->addSeparator();
    menu->addAction(remove);

    menu->popup(mapToGlobal(point));
}
} // namespace Fy::Gui::Widgets
