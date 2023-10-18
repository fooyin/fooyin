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

#include "quicksetup/quicksetupdialog.h"
#include "widgets/dummy.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/splitterwidget.h>
#include <gui/widgetcontainer.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/menuheader.h>
#include <utils/overlayfilter.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

namespace Fy::Gui::Widgets {
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

struct EditableLayout::Private
{
    EditableLayout* self;

    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settings;
    Widgets::WidgetProvider* widgetProvider;
    LayoutProvider* layoutProvider;

    Utils::ActionContainer* menu;
    QHBoxLayout* box;
    Utils::OverlayFilter* overlay;
    SplitterWidget* splitter;
    bool layoutEditing;

    Private(EditableLayout* self, Utils::ActionManager* actionManager, WidgetProvider* widgetProvider,
            LayoutProvider* layoutProvider, Utils::SettingsManager* settings)
        : self{self}
        , actionManager{actionManager}
        , settings{settings}
        , widgetProvider{widgetProvider}
        , layoutProvider{layoutProvider}
        , menu{actionManager->createMenu(Constants::Menus::Context::Layout)}
        , box{new QHBoxLayout(self)}
        , overlay{new Utils::OverlayFilter(self)}
        , splitter{nullptr}
        , layoutEditing{false}
    { }
};

EditableLayout::EditableLayout(Utils::ActionManager* actionManager, WidgetProvider* widgetProvider,
                               LayoutProvider* layoutProvider, Utils::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider, layoutProvider, settings)}
{
    setObjectName("EditableLayout");

    p->menu->appendGroup(Constants::Groups::Two);
    p->menu->appendGroup(Constants::Groups::Three);

    p->box->setContentsMargins(5, 5, 5, 5);
}

EditableLayout::~EditableLayout() = default;

void EditableLayout::initialise()
{
    connect(p->menu, &Utils::ActionContainer::aboutToHide, this, &EditableLayout::hideOverlay);
    p->settings->subscribe<Settings::LayoutEditing>(this, [this](bool enabled) {
        p->layoutEditing = enabled;
    });

    const bool loaded = loadLayout();
    if(!loaded) {
        p->splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget("SplitterVertical"));
        p->splitter->setParent(this);
        p->box->addWidget(p->splitter);
    }
    if(p->splitter && p->splitter->childCount() < 1) {
        p->settings->set<Settings::LayoutEditing>(true);
    }
    qApp->installEventFilter(this);
}

Utils::ActionContainer* EditableLayout::createNewMenu(FyWidget* parent, const QString& title) const
{
    auto id       = parent->id().append(title);
    auto* newMenu = p->actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);

    return newMenu;
}

void EditableLayout::setupReplaceWidgetMenu(Utils::ActionContainer* menu, FyWidget* current)
{
    if(!menu->isEmpty()) {
        return;
    }

    auto* parent = qobject_cast<WidgetContainer*>(current->findParent());
    if(!parent) {
        return;
    }

    p->widgetProvider->setupWidgetMenu(menu, [parent, current](FyWidget* newWidget) {
        parent->replaceWidget(current, newWidget);
    });
}

void EditableLayout::setupContextMenu(FyWidget* widget, Utils::ActionContainer* menu)
{
    if(!widget || !menu) {
        return;
    }

    FyWidget* currentWidget = widget;
    int level               = p->settings->value<Settings::EditingMenuLevels>();

    while(level > 0 && currentWidget) {
        menu->addAction(new Utils::MenuHeaderAction(currentWidget->name(), menu));
        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());
        if(parent) {
            auto* changeMenu = createNewMenu(currentWidget, tr("&Replace"));
            setupReplaceWidgetMenu(changeMenu, currentWidget);
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
            FyWidget* child  = splitterChild(widget);

            if(child) {
                if(qobject_cast<Dummy*>(child)) {
                    setupContextMenu(child->findParent(), p->menu);
                }
                else {
                    setupContextMenu(child, p->menu);
                }
                showOverlay(child);
                p->menu->menu()->popup(mapToGlobal(pos));
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditableLayout::changeLayout(const Layout& layout)
{
    // Delete all current widgets
    delete p->splitter;
    const bool success = loadLayout(layout.json);
    if(success && p->splitter->childCount() > 0) {
        p->settings->set<Settings::LayoutEditing>(false);
    }
    else {
        p->settings->set<Settings::LayoutEditing>(true);
    }
}

void EditableLayout::saveLayout()
{
    p->layoutProvider->saveCurrentLayout(currentLayout());
}

QByteArray EditableLayout::currentLayout()
{
    QJsonObject root;
    QJsonArray array;

    p->splitter->saveLayout(array);

    root["Layout"] = array;

    return QJsonDocument(root).toJson();
}

bool EditableLayout::loadLayout(const QByteArray& layout)
{
    const auto jsonDoc = QJsonDocument::fromJson(layout);
    if(jsonDoc.isNull() || jsonDoc.isEmpty()) {
        return false;
    }

    const QJsonObject json = jsonDoc.object();
    if(!json.contains("Layout") || !json["Layout"].isArray()) {
        return false;
    }

    const QJsonArray layoutArray = json["Layout"].toArray();
    if(layoutArray.isEmpty() || layoutArray.size() != 1) {
        return false;
    }

    const QJsonObject widget = layoutArray[0].toObject();
    if(widget.isEmpty()) {
        return false;
    }

    const auto name = widget.constBegin().key();
    if(auto* splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget(name))) {
        p->splitter               = splitter;
        const QJsonObject options = widget.value(name).toObject();
        p->splitter->loadLayout(options);
        p->box->addWidget(p->splitter);
        return true;
    }

    return false;
}

bool EditableLayout::loadLayout()
{
    const Layout layout = p->layoutProvider->currentLayout();
    return loadLayout(layout.json);
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

void EditableLayout::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(p->layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    connect(quickSetup, &QuickSetupDialog::layoutChanged, this, &Widgets::EditableLayout::changeLayout);
    quickSetup->show();
}
} // namespace Fy::Gui::Widgets
