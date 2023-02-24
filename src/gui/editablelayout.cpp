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

#include "editablelayout.h"

#include "gui/widgets/dummy.h"
#include "gui/widgets/splitterwidget.h"
#include "guiconstants.h"
#include "guisettings.h"
#include "layoutprovider.h"
#include "widgetfactory.h"
#include "widgetprovider.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/enumhelper.h>
#include <utils/menuheader.h>
#include <utils/overlayfilter.h>
#include <utils/paths.h>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

namespace Gui::Widgets {
QRect widgetGeometry(FyWidget* widget)
{
    const int w = widget->width();
    const int h = widget->height();

    int x = widget->x();
    int y = widget->y();

    while((widget = widget->findParent())) {
        x += widget->x();
        y += widget->y();
    }
    return {x, y, w, h};
}

FyWidget* splitterChild(QWidget* widget)
{
    if(!widget) {
        return {};
    }
    QWidget* child = widget;

    while(!qobject_cast<FyWidget*>(child)) {
        child = child->parentWidget();
        if(!child) {
            return {};
        }
    }
    return qobject_cast<FyWidget*>(child);
}

EditableLayout::EditableLayout(Core::SettingsManager* settings, Utils::ActionManager* actionManager,
                               WidgetFactory* widgetFactory, WidgetProvider* widgetProvider,
                               LayoutProvider* layoutProvider, QWidget* parent)
    : QWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_widgetFactory{widgetFactory}
    , m_widgetProvider{widgetProvider}
    , m_layoutProvider{layoutProvider}
    , m_menu{actionManager->createMenu(Constants::Menus::Context::Layout)}
    , m_box{new QHBoxLayout(this)}
    , m_overlay{new Utils::OverlayFilter(this)}
    , m_menuLevels{2}
    , m_layoutEditing{false}
{
    setObjectName("EditableLayout");

    m_menu->appendGroup(Constants::Groups::Two);
    m_menu->appendGroup(Constants::Groups::Three);

    m_box->setContentsMargins(5, 5, 5, 5);
}

void EditableLayout::initialise()
{
    connect(m_menu, &Utils::ActionContainer::aboutToHide, this, &EditableLayout::hideOverlay);
    m_settings->subscribe<Settings::LayoutEditing>(this, [this](bool enabled) {
        m_layoutEditing = enabled;
    });

    const bool loaded = loadLayout();
    if(!loaded) {
        m_splitter = qobject_cast<SplitterWidget*>(m_widgetProvider->createWidget("SplitterVertical"));
        m_splitter->setParent(this);
        m_box->addWidget(m_splitter);
    }
    if(m_splitter && !m_splitter->hasChildren()) {
        m_settings->set<Settings::LayoutEditing>(true);
    }
    qApp->installEventFilter(this);
}

Utils::ActionContainer* EditableLayout::createNewMenu(FyWidget* parent, const QString& title) const
{
    auto id       = parent->id().append(title);
    auto* newMenu = m_actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);

    return newMenu;
}

void EditableLayout::setupWidgetMenu(Utils::ActionContainer* menu, FyWidget* parent, bool replace)
{
    if(!menu->isEmpty()) {
        return;
    }
    auto* splitter = qobject_cast<SplitterWidget*>(parent);
    if(!splitter || replace) {
        splitter = qobject_cast<SplitterWidget*>(parent->findParent());
    }
    auto widgets = m_widgetFactory->registeredWidgets();
    for(const auto& widget : widgets) {
        auto* parentMenu = menu;
        for(const auto& subMenu : widget.second.subMenus) {
            const Utils::Id id = Utils::Id{menu->id()}.append(subMenu);
            auto* childMenu    = m_actionManager->actionContainer(id);
            if(!childMenu) {
                childMenu = m_actionManager->createMenu(id);
                childMenu->menu()->setTitle(subMenu);
                parentMenu->addMenu(childMenu);
            }
            parentMenu = childMenu;
        }
        auto* addWidget = new QAction(widget.second.name, parentMenu);
        QAction::connect(addWidget, &QAction::triggered, this, [this, parent, replace, widget, splitter] {
            FyWidget* newWidget = m_widgetProvider->createWidget(widget.first);
            if(replace) {
                splitter->replaceWidget(parent, newWidget);
            }
            else {
                splitter->addWidget(newWidget);
            }
        });
        parentMenu->addAction(addWidget);
    }
}

void EditableLayout::setupContextMenu(FyWidget* widget, Utils::ActionContainer* menu)
{
    if(!widget || !menu) {
        return;
    }
    auto* currentWidget = widget;
    auto level          = m_menuLevels;
    while(level > 0) {
        if(!currentWidget) {
            break;
        }
        menu->addAction(new Utils::MenuHeaderAction(currentWidget->name(), menu));
        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<SplitterWidget*>(currentWidget->findParent());

        if(auto* splitter = qobject_cast<SplitterWidget*>(currentWidget)) {
            auto* addMenu = createNewMenu(splitter, tr("&Add"));
            setupWidgetMenu(addMenu, splitter);
            menu->addMenu(addMenu);

            // Only non-root splitters are removable
            if(parent) {
                auto* changeMenu = createNewMenu(splitter, tr("&Replace"));
                setupWidgetMenu(changeMenu, splitter, true);
                menu->addMenu(changeMenu);

                auto* remove = new QAction("Remove", menu);
                QAction::connect(remove, &QAction::triggered, parent, [parent, splitter] {
                    parent->removeWidget(splitter);
                });
                menu->addAction(remove);
            }
        }
        else {
            auto* changeMenu = createNewMenu(currentWidget, tr("&Replace"));
            setupWidgetMenu(changeMenu, currentWidget, true);
            menu->addMenu(changeMenu);

            auto* remove = new QAction("Remove", menu);
            QAction::connect(remove, &QAction::triggered, parent, [parent, widget] {
                parent->removeWidget(widget);
            });
            menu->addAction(remove);
        }
        currentWidget = parent;
        --level;
    }
}

bool EditableLayout::eventFilter(QObject* watched, QEvent* event)
{
    if(!m_layoutEditing) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::RightButton && m_menu->isHidden()) {
            m_menu->clear();

            const QPoint pos = mouseEvent->position().toPoint();
            QWidget* widget  = parentWidget()->childAt(pos);
            FyWidget* child  = splitterChild(widget);

            if(child) {
                if(qobject_cast<Dummy*>(child)) {
                    setupContextMenu(child->findParent(), m_menu);
                }
                else {
                    setupContextMenu(child, m_menu);
                }
                showOverlay(child);
                m_menu->menu()->popup(mapToGlobal(pos));
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditableLayout::changeLayout(const Layout& layout)
{
    // Delete all current widgets
    // TODO: Look into caching previous layout widgets
    delete m_splitter;
    const bool success = loadLayout(layout.json);
    if(success && m_splitter->hasChildren()) {
        m_settings->set<Settings::LayoutEditing>(false);
    }
    else {
        m_settings->set<Settings::LayoutEditing>(true);
    }
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonArray array;

    m_splitter->saveLayout(array);

    root["Layout"] = array;

    const auto json = QJsonDocument(root).toJson();
    m_layoutProvider->saveCurrentLayout(json);
}

bool EditableLayout::loadLayout(const QByteArray& layout)
{
    const auto jsonDoc = QJsonDocument::fromJson(layout);
    if(!jsonDoc.isNull() && !jsonDoc.isEmpty()) {
        QJsonObject json = jsonDoc.object();
        if(json.contains("Layout") && json["Layout"].isArray()) {
            auto layout = json["Layout"].toArray();
            if(!layout.isEmpty() && layout.size() == 1) {
                const auto first = layout.constBegin();
                auto widget      = first->toObject();
                if(!widget.isEmpty()) {
                    const auto name = widget.constBegin().key();
                    if(auto* splitter = qobject_cast<SplitterWidget*>(m_widgetProvider->createWidget(name))) {
                        m_splitter   = splitter;
                        auto options = widget.value(name).toObject();
                        m_splitter->loadLayout(options);
                        m_box->addWidget(m_splitter);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool EditableLayout::loadLayout()
{
    const Layout layout = m_layoutProvider->currentLayout();
    return loadLayout(layout.json);
}

void EditableLayout::showOverlay(FyWidget* widget)
{
    m_overlay->setGeometry(widgetGeometry(widget));
    m_overlay->raise();
    m_overlay->show();
}

void EditableLayout::hideOverlay()
{
    m_overlay->hide();
}
} // namespace Gui::Widgets
