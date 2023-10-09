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

#include "splitterwidget.h"

#include "dummy.h"
#include "gui/widgetfactory.h"
#include "splitter.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/enumhelper.h>

#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>

namespace Fy::Gui::Widgets {
Utils::ActionContainer* createNewMenu(Utils::ActionManager* actionManager, FyWidget* parent, const QString& title)
{
    auto id       = parent->id().append(title);
    auto* newMenu = actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);
    return newMenu;
}

SplitterWidget::SplitterWidget(Utils::ActionManager* actionManager, Widgets::WidgetFactory* widgetFactory,
                               Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_actionManager{actionManager}
    , m_widgetFactory{widgetFactory}
    , m_layout{new QHBoxLayout(this)}
    , m_splitter{new Splitter(Qt::Vertical, settings, this)}
    , m_dummy{new Dummy(this)}
    , m_widgetCount{0}
    , m_isRoot{false}
{
    setObjectName(SplitterWidget::name());

    if(!qobject_cast<SplitterWidget*>(findParent())) {
        m_isRoot = true;
    }

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_splitter);

    m_splitter->addWidget(m_dummy);
}

Qt::Orientation SplitterWidget::orientation() const
{
    return m_splitter->orientation();
}

void SplitterWidget::setOrientation(Qt::Orientation orientation)
{
    m_splitter->setOrientation(orientation);
    setObjectName(SplitterWidget::name());
}

QByteArray SplitterWidget::saveState() const
{
    return m_splitter->saveState();
}

bool SplitterWidget::restoreState(const QByteArray& state)
{
    return m_splitter->restoreState(state);
}

QWidget* SplitterWidget::widgetAtIndex(int index) const
{
    return m_splitter->widget(index);
}

bool SplitterWidget::hasChildren()
{
    return !m_children.isEmpty();
}

void SplitterWidget::addWidget(QWidget* newWidget)
{
    auto* widget = qobject_cast<FyWidget*>(newWidget);
    if(!widget) {
        return;
    }

    const auto* newSplitter = qobject_cast<SplitterWidget*>(newWidget);

    if((m_isRoot && newSplitter) || !newSplitter) {
        ++m_widgetCount;
    }
    if(m_widgetCount > 0) {
        m_dummy->hide();
    }

    const int index = static_cast<int>(m_children.count());
    m_children.append(widget);
    return m_splitter->insertWidget(index, widget);
}

void SplitterWidget::insertWidget(int index, FyWidget* widget)
{
    if(!widget) {
        return;
    }
    m_children.insert(index, widget);
    m_splitter->insertWidget(index, widget);
}

void SplitterWidget::setupAddWidgetMenu(Utils::ActionContainer* menu)
{
    if(!menu->isEmpty()) {
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
        auto* addWidgetAction = new QAction(widget.second.name, parentMenu);
        QObject::connect(addWidgetAction, &QAction::triggered, this, [this, widget] {
            FyWidget* newWidget = m_widgetFactory->make(widget.first);
            addWidget(newWidget);
        });
        parentMenu->addAction(addWidgetAction);
    }
}

void SplitterWidget::replaceWidget(int index, FyWidget* widget)
{
    if(!widget || m_children.isEmpty()) {
        return;
    }
    FyWidget* oldWidget = m_children.takeAt(index);
    oldWidget->deleteLater();
    m_children.insert(index, widget);
    m_splitter->insertWidget(index, widget);
}

void SplitterWidget::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    if(!oldWidget || !newWidget || m_children.isEmpty()) {
        return;
    }
    const int index = findIndex(oldWidget);
    replaceWidget(index, newWidget);
}

void SplitterWidget::removeWidget(FyWidget* widget)
{
    const int index = findIndex(widget);
    if(index != -1) {
        const auto* removeSplitter = qobject_cast<SplitterWidget*>(widget);
        if((m_isRoot && removeSplitter) || !removeSplitter) {
            --m_widgetCount;
        }
        widget->deleteLater();
        m_children.remove(index);
    }
    if(m_widgetCount < 2) {
        m_dummy->show();
    }
}

int SplitterWidget::findIndex(FyWidget* widgetToFind)
{
    for(int i = 0; i < m_children.size(); ++i) {
        FyWidget* widget = m_children.value(i);
        if(widget == widgetToFind) {
            return i;
        }
    }
    return -1;
}

QString SplitterWidget::name() const
{
    return QString("%1 Splitter").arg(orientation() == Qt::Horizontal ? "Horizontal" : "Vertical");
}

QString SplitterWidget::layoutName() const
{
    return QString("Splitter%1").arg(orientation() == Qt::Horizontal ? "Horizontal" : "Vertical");
}

void SplitterWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    QAction* changeSplitter = new QAction("Change Splitter", this);
    QAction::connect(changeSplitter, &QAction::triggered, this, [this] {
        setOrientation(m_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
        setObjectName(QString("%1 Splitter").arg(Utils::EnumHelper::toString(m_splitter->orientation())));
    });
    menu->addAction(changeSplitter);

    auto* addMenu = createNewMenu(m_actionManager, this, tr("&Add"));
    setupAddWidgetMenu(addMenu);
    menu->addMenu(addMenu);
}

void SplitterWidget::saveLayout(QJsonArray& array)
{
    QJsonArray children;
    for(const auto& widget : m_children) {
        widget->saveLayout(children);
    }
    const QString state = QString::fromUtf8(saveState().toBase64());

    QJsonObject options;
    options["State"]    = state;
    options["Children"] = children;

    QJsonObject splitter;
    splitter[layoutName()] = options;
    array.append(splitter);
}

void SplitterWidget::loadLayout(const QJsonObject& object)
{
    const auto state    = QByteArray::fromBase64(object["State"].toString().toUtf8());
    const auto children = object["Children"].toArray();

    for(const auto& widget : children) {
        const QJsonObject jsonObject = widget.toObject();
        if(!jsonObject.isEmpty()) {
            const auto widgetName = jsonObject.constBegin().key();
            if(auto* childWidget = m_widgetFactory->make(widgetName)) {
                addWidget(childWidget);
                const QJsonObject widgetObject = jsonObject.value(widgetName).toObject();
                childWidget->loadLayout(widgetObject);
            }
        }
        else {
            auto* childWidget = m_widgetFactory->make(widget.toString());
            addWidget(childWidget);
        }
    }
    restoreState(state);
}
} // namespace Fy::Gui::Widgets
