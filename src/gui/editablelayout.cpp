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
#include "widgetprovider.h"
#include "widgets/dummy.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/splitterwidget.h>
#include <gui/widgetfactory.h>
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

EditableLayout::EditableLayout(Utils::SettingsManager* settings, Utils::ActionManager* actionManager,
                               WidgetFactory* widgetFactory, LayoutProvider* layoutProvider, QWidget* parent)
    : QWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_widgetFactory{widgetFactory}
    , m_widgetProvider{m_widgetFactory}
    , m_layoutProvider{layoutProvider}
    , m_menu{actionManager->createMenu(Constants::Menus::Context::Layout)}
    , m_box{new QHBoxLayout(this)}
    , m_overlay{new Utils::OverlayFilter(this)}
    , m_splitter{nullptr}
    , m_layoutEditing{false}
{
    setObjectName("EditableLayout");

    m_menu->appendGroup(Constants::Groups::Two);
    m_menu->appendGroup(Constants::Groups::Three);

    m_box->setContentsMargins(5, 5, 5, 5);

    m_widgetFactory->registerClass<Gui::Widgets::VerticalSplitterWidget>(
        "SplitterVertical",
        [this]() {
            return new Gui::Widgets::VerticalSplitterWidget(m_actionManager, m_widgetFactory, m_settings);
        },
        "Vertical Splitter", {"Splitter"});

    m_widgetFactory->registerClass<Gui::Widgets::HorizontalSplitterWidget>(
        "SplitterHorizontal",
        [this]() {
            return new Gui::Widgets::HorizontalSplitterWidget(m_actionManager, m_widgetFactory, m_settings);
        },
        "Horizontal Splitter", {"Splitter"});
}

void EditableLayout::initialise()
{
    connect(m_menu, &Utils::ActionContainer::aboutToHide, this, &EditableLayout::hideOverlay);
    m_settings->subscribe<Settings::LayoutEditing>(this, [this](bool enabled) {
        m_layoutEditing = enabled;
    });

    const bool loaded = loadLayout();
    if(!loaded) {
        m_splitter = qobject_cast<SplitterWidget*>(m_widgetProvider.createWidget("SplitterVertical"));
        m_splitter->setParent(this);
        m_box->addWidget(m_splitter);
    }
    if(m_splitter && m_splitter->childCount() < 1) {
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

void EditableLayout::setupReplaceWidgetMenu(Utils::ActionContainer* menu, FyWidget* current)
{
    if(!menu->isEmpty()) {
        return;
    }

    auto* splitter = qobject_cast<SplitterWidget*>(current->findParent());
    if(!splitter) {
        return;
    }

    const auto widgets = m_widgetFactory->registeredWidgets();
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
        QObject::connect(addWidget, &QAction::triggered, this, [this, current, widget, splitter] {
            FyWidget* newWidget = m_widgetProvider.createWidget(widget.first);
            splitter->replaceWidget(current, newWidget);
        });
        parentMenu->addAction(addWidget);
    }
}

void EditableLayout::setupContextMenu(FyWidget* widget, Utils::ActionContainer* menu)
{
    if(!widget || !menu) {
        return;
    }

    FyWidget* currentWidget = widget;
    int level               = m_settings->value<Settings::EditingMenuLevels>();

    while(level > 0 && currentWidget) {
        menu->addAction(new Utils::MenuHeaderAction(currentWidget->name(), menu));
        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<SplitterWidget*>(currentWidget->findParent());
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
    delete m_splitter;
    const bool success = loadLayout(layout.json);
    if(success && m_splitter->childCount() > 0) {
        m_settings->set<Settings::LayoutEditing>(false);
    }
    else {
        m_settings->set<Settings::LayoutEditing>(true);
    }
}

void EditableLayout::saveLayout()
{
    m_layoutProvider->saveCurrentLayout(currentLayout());
}

QByteArray EditableLayout::currentLayout()
{
    QJsonObject root;
    QJsonArray array;

    m_splitter->saveLayout(array);

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
    if(auto* splitter = qobject_cast<SplitterWidget*>(m_widgetProvider.createWidget(name))) {
        m_splitter                = splitter;
        const QJsonObject options = widget.value(name).toObject();
        m_splitter->loadLayout(options);
        m_box->addWidget(m_splitter);
        return true;
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

void EditableLayout::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(m_layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    connect(quickSetup, &QuickSetupDialog::layoutChanged, this, &Widgets::EditableLayout::changeLayout);
    quickSetup->show();
}
} // namespace Fy::Gui::Widgets
