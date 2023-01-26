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

#include "gui/guisettings.h"
#include "gui/widgets/dummy.h"
#include "gui/widgets/menuheader.h"
#include "gui/widgets/overlayfilter.h"
#include "gui/widgets/splitterwidget.h"
#include "widgetfactory.h"
#include "widgetprovider.h"

#include <core/actions/actioncontainer.h>
#include <core/actions/actionmanager.h>
#include <core/constants.h>
#include <utils/enumhelper.h>
#include <utils/paths.h>

#include <QCoreApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

namespace Gui::Widgets {
struct EditableLayout::Private
{
    Core::ActionManager* actionManager;
    Core::SettingsManager* settings;
    Widgets::WidgetFactory* widgetFactory;
    Widgets::WidgetProvider* widgetProvider;

    Core::ActionContainer* menu;
    QHBoxLayout* box;
    OverlayFilter* overlay;
    SplitterWidget* splitter;
    int menuLevels{2};
    bool layoutEditing{false};

    QFile layoutFile;

    explicit Private(Core::SettingsManager* settings, Core::ActionManager* actionManager, WidgetFactory* widgetFactory,
                     WidgetProvider* widgetProvider, QWidget* parent)
        : actionManager{actionManager}
        , settings{settings}
        , widgetFactory{widgetFactory}
        , widgetProvider{widgetProvider}
        , menu{actionManager->createMenu(Core::Constants::ContextMenus::Layout)}
        , box{new QHBoxLayout(parent)}
        , overlay{new OverlayFilter(parent)}
        , layoutFile{Utils::configPath() + "/layout.fyl"}
    { }

    Core::ActionContainer* createNewMenu(FyWidget* parent, const QString& title) const
    {
        auto id       = parent->id().append(title);
        auto* newMenu = actionManager->createMenu(id);
        newMenu->menu()->setTitle(title);

        return newMenu;
    }

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
};

EditableLayout::EditableLayout(Core::SettingsManager* settings, Core::ActionManager* actionManager,
                               WidgetFactory* widgetFactory, WidgetProvider* widgetProvider, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(settings, actionManager, widgetFactory, widgetProvider, this)}
{
    setObjectName("EditableLayout");

    p->menu->appendGroup(Core::Constants::Groups::Two);
    p->menu->appendGroup(Core::Constants::Groups::Three);

    p->box->setContentsMargins(5, 5, 5, 5);
}

EditableLayout::~EditableLayout() = default;

void EditableLayout::initialise()
{
    connect(p->menu, &Core::ActionContainer::aboutToHide, this, &EditableLayout::hideOverlay);
    p->settings->subscribe<Settings::LayoutEditing>(this, [this](bool enabled) {
        p->layoutEditing = enabled;
    });

    const bool loaded = loadLayout();
    if(!loaded) {
        p->splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget("SplitterVertical"));
        if(p->splitter) {
            p->splitter->setParent(this);
            p->box->addWidget(p->splitter);
        }
        else {
            p->splitter = new SplitterWidget(p->actionManager, p->widgetProvider, p->settings, this);
        }
    }
    if(!p->splitter->hasChildren()) {
        p->settings->set<Settings::LayoutEditing>(true);
    }
    qApp->installEventFilter(this);
}

void EditableLayout::setupWidgetMenu(Core::ActionContainer* menu, FyWidget* parent, bool replace)
{
    if(!menu->isEmpty()) {
        return;
    }
    auto* splitter = qobject_cast<SplitterWidget*>(parent);
    if(!splitter || replace) {
        splitter = qobject_cast<SplitterWidget*>(parent->findParent());
    }
    auto widgets = p->widgetFactory->registeredWidgets();
    for(const auto& widget : widgets) {
        auto* parentMenu = menu;
        for(const auto& subMenu : widget.second.subMenus) {
            const Utils::Id id = Utils::Id{menu->id()}.append(subMenu);
            auto* childMenu    = p->actionManager->actionContainer(id);
            if(!childMenu) {
                childMenu = p->actionManager->createMenu(id);
                childMenu->menu()->setTitle(subMenu);
                parentMenu->addMenu(childMenu);
            }
            parentMenu = childMenu;
        }
        auto* addWidget = new QAction(widget.second.name, parentMenu);
        QAction::connect(addWidget, &QAction::triggered, this, [this, parent, replace, widget, splitter] {
            FyWidget* newWidget = p->widgetProvider->createWidget(widget.first);
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

void EditableLayout::setupContextMenu(FyWidget* widget, Core::ActionContainer* menu)
{
    if(!widget || !menu) {
        return;
    }
    auto* currentWidget = widget;
    auto level          = p->menuLevels;
    while(level > 0) {
        if(!currentWidget) {
            break;
        }
        menu->addAction(new MenuHeaderAction(currentWidget->name(), menu));
        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<SplitterWidget*>(currentWidget->findParent());

        if(auto* splitter = qobject_cast<SplitterWidget*>(currentWidget)) {
            auto* addMenu = p->createNewMenu(splitter, tr("&Add"));
            setupWidgetMenu(addMenu, splitter);
            menu->addMenu(addMenu);

            // Only non-root splitters are removable
            if(parent) {
                auto* changeMenu = p->createNewMenu(splitter, tr("&Replace"));
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
            auto* changeMenu = p->createNewMenu(currentWidget, tr("&Replace"));
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
    if(!p->layoutEditing) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::RightButton && p->menu->isHidden()) {
            p->menu->clear();

            const QPoint pos = mouseEvent->position().toPoint();
            QWidget* widget  = parentWidget()->childAt(pos);
            FyWidget* child  = p->splitterChild(widget);

            if(child) {
                if(qobject_cast<Dummy*>(child)) {
                    setupContextMenu(child->findParent(), p->menu);
                }
                else {
                    setupContextMenu(child, p->menu);
                }
                showOverlay(child);
                p->menu->menu()->exec(mapToGlobal(pos));
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditableLayout::changeLayout(const QByteArray& layout)
{
    // Delete all current widgets
    // TODO: Look into caching previous layout widgets
    p->splitter->deleteLater();
    const bool success = loadLayout(layout);
    if(success && p->splitter->hasChildren()) {
        p->settings->set<Settings::LayoutEditing>(false);
    }
    else {
        p->settings->set<Settings::LayoutEditing>(true);
    }
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonArray array;

    p->splitter->saveLayout(array);

    root["Layout"] = array;

    const auto json = QJsonDocument(root).toJson();

    if(!p->layoutFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Couldn't open layout file.";
        return;
    }

    p->layoutFile.write(json);
    p->layoutFile.close();
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
                    if(auto* splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget(name))) {
                        p->splitter  = splitter;
                        auto options = widget.value(name).toObject();
                        p->splitter->loadLayout(options);
                        p->box->addWidget(p->splitter);
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
    if(!p->layoutFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Couldn't open layout file.";
        return false;
    }

    const QByteArray layout = p->layoutFile.readAll();
    p->layoutFile.close();

    return loadLayout(layout);
}

void EditableLayout::showOverlay(FyWidget* widget)
{
    p->overlay->setGeometry(p->widgetGeometry(widget));
    p->overlay->raise();
    p->overlay->show();
}

void EditableLayout::hideOverlay()
{
    p->overlay->hide();
}
} // namespace Gui::Widgets
