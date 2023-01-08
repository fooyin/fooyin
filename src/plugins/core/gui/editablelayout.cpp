/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/actions/actioncontainer.h"
#include "core/actions/actionmanager.h"
#include "core/constants.h"
#include "core/gui/widgets/dummy.h"
#include "core/gui/widgets/menuheader.h"
#include "core/gui/widgets/overlayfilter.h"
#include "core/gui/widgets/splitterwidget.h"
#include "core/settings/settings.h"
#include "core/widgets/widgetfactory.h"
#include "core/widgets/widgetprovider.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

namespace Core::Widgets {
struct EditableLayout::Private
{
    using MenuMap = std::unordered_map<Utils::Id, ActionContainer*, Utils::Id::IdHash>;

    QHBoxLayout* box;
    Settings* settings;
    bool layoutEditing{false};
    OverlayFilter* overlay;
    ActionManager* actionManager;
    SplitterWidget* splitter;
    ActionContainer* menu;
    int menuLevels{2};
    MenuMap addMenus;
    Widgets::WidgetFactory* widgetFactory;
    Widgets::WidgetProvider* widgetProvider;

    explicit Private(QWidget* parent)
        : box{new QHBoxLayout(parent)}
        , settings{PluginSystem::object<Settings>()}
        , overlay{new OverlayFilter(parent)}
        , actionManager{PluginSystem::object<ActionManager>()}
        , widgetFactory{PluginSystem::object<Widgets::WidgetFactory>()}
        , widgetProvider{PluginSystem::object<Widgets::WidgetProvider>()}
    { }
};

EditableLayout::EditableLayout(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName("EditableLayout");

    p->menu = p->actionManager->createMenu(Core::Constants::ContextMenus::Layout);

    p->menu->appendGroup(Core::Constants::Groups::Two);
    p->menu->appendGroup(Core::Constants::Groups::Three);

    p->box->setContentsMargins(5, 5, 5, 5);

    connect(p->menu, &ActionContainer::aboutToHide, this, &EditableLayout::hideOverlay);
    connect(p->settings, &Settings::layoutEditingChanged, this, [this](bool enabled) {
        p->layoutEditing = enabled;
    });

    bool loaded = loadLayout();
    if(!loaded) {
        p->splitter = Widgets::WidgetProvider::createSplitter(Qt::Vertical, this);
        p->box->addWidget(p->splitter);
    }
    if(!p->splitter->hasChildren()) {
        p->settings->set(Settings::Setting::LayoutEditing, true);
    }
    qApp->installEventFilter(this);
}

ActionContainer* EditableLayout::createAddMenu(SplitterWidget* parent)
{
    auto id = parent->id();
    auto* menu = p->actionManager->createMenu(id);
    p->addMenus.emplace(id, menu);
    menu->menu()->setTitle(tr("&Add"));

    return menu;
}

void EditableLayout::setupAddMenu(ActionContainer* menu, SplitterWidget* parent)
{
    if(!menu->isEmpty()) {
        return;
    }
    auto widgets = p->widgetFactory->registeredWidgets();
    for(const auto& widget : widgets) {
        auto* parentMenu = menu;
        for(const auto& subMenu : widget.second.subMenus) {
            Utils::Id id = Utils::Id{menu->id()}.append(subMenu);
            auto* childMenu = p->actionManager->actionContainer(id);
            if(!childMenu) {
                childMenu = p->actionManager->createMenu(id);
                childMenu->menu()->setTitle(subMenu);
                parentMenu->addMenu(childMenu);
            }
            parentMenu = childMenu;
        }
        auto* addWidget = new QAction(widget.first, parentMenu);
        QAction::connect(addWidget, &QAction::triggered, this, [this, widget, parent] {
            p->widgetProvider->createWidget(widget.first, parent);
        });
        parentMenu->addAction(addWidget);
    }
}

void EditableLayout::changeLayout(const QByteArray& layout)
{
    // Delete all current widgets
    // TODO: Look into caching previous layout widgets
    p->splitter->deleteLater();
    bool success = loadLayout(layout);
    if(success && p->splitter->hasChildren()) {
        p->settings->set(Settings::Setting::LayoutEditing, false);
    }
    else {
        p->settings->set(Settings::Setting::LayoutEditing, true);
    }
}

EditableLayout::~EditableLayout() = default;

FyWidget* EditableLayout::splitterChild(QWidget* widget)
{
    if(!widget) {
        return {};
    }

    QWidget* child = widget;

    while(!qobject_cast<FyWidget*>(child) || qobject_cast<Dummy*>(child)) {
        child = child->parentWidget();
        if(!child) {
            return {};
        }
    }

    if(child) {
        return qobject_cast<FyWidget*>(child);
    }

    return {};
}

void EditableLayout::addParentContext(FyWidget* widget, ActionContainer* menu)
{
    auto* parent = qobject_cast<SplitterWidget*>(widget->findParent());
    if(parent) {
        if(qobject_cast<SplitterWidget*>(widget)) {
            auto* addMenu = createAddMenu(parent);
            setupAddMenu(addMenu, parent);
            menu->addMenu(addMenu);
        }
        auto* remove = new QAction("Remove", menu);
        QAction::connect(remove, &QAction::triggered, parent, [parent, widget] {
            parent->removeWidget(widget);
        });
        menu->addAction(remove);
    }
    auto level = p->menuLevels;
    while(level > 1) {
        if(!parent) {
            break;
        }
        menu->addAction(new MenuHeaderAction(parent->name(), menu));
        parent->layoutEditingMenu(menu);
        auto* addMenu = createAddMenu(parent);
        setupAddMenu(addMenu, parent);
        menu->addMenu(addMenu);
        parent = qobject_cast<SplitterWidget*>(parent->findParent());
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
            QWidget* widget = parentWidget()->childAt(pos);
            FyWidget* child = splitterChild(widget);

            if(child) {
                p->menu->addAction(new MenuHeaderAction(child->objectName(), p->menu));
                child->layoutEditingMenu(p->menu);
                addParentContext(child, p->menu);
            }
            if(child && !p->menu->isEmpty()) {
                showOverlay(child);
                p->menu->menu()->exec(mapToGlobal(pos));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect EditableLayout::widgetGeometry(FyWidget* widget)
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

void EditableLayout::showOverlay(FyWidget* widget)
{
    p->overlay->setGeometry(widgetGeometry(widget));
    p->overlay->raise();
    p->overlay->show();
}

void EditableLayout::hideOverlay()
{
    p->overlay->hide();
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonObject object;
    QJsonArray array;

    p->splitter->saveSplitter(object, array);

    root["Layout"] = object;

    QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact).toBase64());

    p->settings->set(Settings::Setting::Layout, json);
}

bool EditableLayout::loadLayout(const QByteArray& layout)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(layout);
    if(!jsonDoc.isNull() && !jsonDoc.isEmpty()) {
        QJsonObject json = jsonDoc.object();
        if(json.contains("Layout") && json["Layout"].isObject()) {
            QJsonObject object = json["Layout"].toObject();
            if(object.contains("Splitter") && object["Splitter"].isObject()) {
                QJsonObject splitterObject = object["Splitter"].toObject();

                auto type = Utils::EnumHelper::fromString<Qt::Orientation>(splitterObject["Type"].toString());
                QJsonArray splitterChildren = splitterObject["Children"].toArray();
                auto state = QByteArray::fromBase64(splitterObject["State"].toString().toUtf8());

                p->splitter = Widgets::WidgetProvider::createSplitter(type, this);
                p->box->addWidget(p->splitter);

                p->splitter->loadSplitter(splitterChildren, p->splitter);
                p->splitter->restoreState(state);
            }
            return true;
        }
    }
    return false;
}

bool EditableLayout::loadLayout()
{
    auto layout = QByteArray::fromBase64(p->settings->value(Settings::Setting::Layout).toByteArray());
    return loadLayout(layout);
}
}; // namespace Core::Widgets
