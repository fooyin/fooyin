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
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

using namespace Qt::Literals::StringLiterals;

namespace {
QRect widgetGeometry(Fooyin::FyWidget* widget)
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

Fooyin::FyWidget* splitterChild(QWidget* widget)
{
    if(!widget) {
        return {};
    }
    QWidget* child = widget;

    while(!qobject_cast<Fooyin::FyWidget*>(child)) {
        child = child->parentWidget();
        if(!child) {
            return {};
        }
    }
    return qobject_cast<Fooyin::FyWidget*>(child);
}
} // namespace

namespace Fooyin {
struct EditableLayout::Private
{
    EditableLayout* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    WidgetProvider* widgetProvider;
    LayoutProvider* layoutProvider;

    ActionContainer* menu;
    QHBoxLayout* box;
    OverlayFilter* overlay;
    SplitterWidget* splitter{nullptr};
    bool layoutEditing{false};

    Private(EditableLayout* self, ActionManager* actionManager, WidgetProvider* widgetProvider,
            LayoutProvider* layoutProvider, SettingsManager* settings)
        : self{self}
        , actionManager{actionManager}
        , settings{settings}
        , widgetProvider{widgetProvider}
        , layoutProvider{layoutProvider}
        , menu{actionManager->createMenu(Constants::Menus::Context::Layout)}
        , box{new QHBoxLayout(self)}
        , overlay{new OverlayFilter(self)}
    {
        box->setContentsMargins(5, 5, 5, 5);
    }

    void changeEditingState(bool editing)
    {
        layoutEditing = editing;
        if(editing) {
            qApp->installEventFilter(self);
        }
        else {
            qApp->removeEventFilter(self);
        }
    }
};

EditableLayout::EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                               LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider, layoutProvider, settings)}
{
    setObjectName("EditableLayout");
}

EditableLayout::~EditableLayout() = default;

void EditableLayout::initialise()
{
    if(!loadLayout()) {
        p->splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget(u"SplitterVertical"_s));
        p->splitter->setParent(this);
        p->box->addWidget(p->splitter);
    }

    p->settings->subscribe<Settings::Gui::LayoutEditing>(this,
                                                         [this](bool enabled) { p->changeEditingState(enabled); });

    if(p->splitter && p->splitter->childCount() < 1) {
        p->settings->set<Settings::Gui::LayoutEditing>(true);
    }

    QObject::connect(p->menu->menu(), &QMenu::aboutToHide, this, &EditableLayout::hideOverlay);
}

ActionContainer* EditableLayout::createNewMenu(FyWidget* parent, const QString& title) const
{
    auto id       = parent->id().append(title);
    auto* newMenu = p->actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);

    return newMenu;
}

void EditableLayout::setupReplaceWidgetMenu(ActionContainer* menu, FyWidget* current)
{
    if(!menu->isEmpty()) {
        return;
    }

    auto* parent = qobject_cast<WidgetContainer*>(current->findParent());
    if(!parent) {
        return;
    }

    p->widgetProvider->setupWidgetMenu(
        menu, [parent, current](FyWidget* newWidget) { parent->replaceWidget(current, newWidget); });
}

void EditableLayout::setupContextMenu(FyWidget* widget, ActionContainer* menu)
{
    if(!widget || !menu) {
        return;
    }

    FyWidget* currentWidget = widget;
    int level               = p->settings->value<Settings::Gui::EditingMenuLevels>();

    while(level > 0 && currentWidget) {
        menu->addAction(new MenuHeaderAction(currentWidget->name(), menu));
        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());
        if(parent) {
            auto* changeMenu = createNewMenu(currentWidget, tr("&Replace"));
            setupReplaceWidgetMenu(changeMenu, currentWidget);
            menu->addMenu(changeMenu);

            auto* remove = new QAction(tr("Remove"), menu);
            QObject::connect(remove, &QAction::triggered, parent,
                             [parent, currentWidget] { parent->removeWidget(currentWidget); });
            menu->addAction(remove);
        }
        currentWidget = parent;
        --level;
    }
}

bool EditableLayout::eventFilter(QObject* watched, QEvent* event)
{
    if(!p->layoutEditing || event->type() != QEvent::MouseButtonPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if(mouseEvent->button() != Qt::RightButton || !p->menu->isHidden()) {
        return QWidget::eventFilter(watched, event);
    }

    p->menu->clear();

    const QPoint pos = mouseEvent->position().toPoint();
    QWidget* widget  = parentWidget()->childAt(pos);
    FyWidget* child  = splitterChild(widget);

    if(!child) {
        return QWidget::eventFilter(watched, event);
    }

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

void EditableLayout::changeLayout(const Layout& layout)
{
    // Delete all current widgets
    delete p->splitter;
    const bool success = loadLayout(layout);
    if(success && p->splitter->childCount() > 0) {
        p->settings->set<Settings::Gui::LayoutEditing>(false);
    }
    else {
        p->settings->set<Settings::Gui::LayoutEditing>(true);
    }
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonArray array;

    p->splitter->saveLayout(array);

    root["Default"_L1] = array;

    const QByteArray json = QJsonDocument(root).toJson();

    if(auto layout = p->layoutProvider->readLayout(json)) {
        p->layoutProvider->changeLayout(layout.value());
    }
}

bool EditableLayout::loadLayout(const Layout& layout)
{
    if(layout.json.isEmpty()) {
        return false;
    }

    const auto name = layout.json.constBegin().key();
    if(auto* splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget(name))) {
        p->splitter               = splitter;
        const QJsonObject options = layout.json.constBegin()->toObject();
        p->splitter->loadLayout(options);
        p->box->addWidget(p->splitter);
        return true;
    }

    return false;
}

bool EditableLayout::loadLayout()
{
    const Layout layout = p->layoutProvider->currentLayout();
    return loadLayout(layout);
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
    QObject::connect(quickSetup, &QuickSetupDialog::layoutChanged, this, &EditableLayout::changeLayout);
    quickSetup->show();
}
} // namespace Fooyin

#include "moc_editablelayout.cpp"
