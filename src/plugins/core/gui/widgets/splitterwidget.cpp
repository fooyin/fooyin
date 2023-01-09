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

#include "splitterwidget.h"

#include "core/actions/actioncontainer.h"
#include "core/actions/actionmanager.h"
#include "core/widgets/widgetprovider.h"
#include "dummy.h"
#include "splitter.h"

#include <QJsonObject>
#include <QMenu>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

namespace Core::Widgets {
SplitterWidget::SplitterWidget(QWidget* parent)
    : FyWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_splitter{new Splitter(Qt::Vertical)}
    , m_actionManager{PluginSystem::object<ActionManager>()}
    , m_widgetProvider{PluginSystem::object<Widgets::WidgetProvider>()}
    , m_dummy{new Dummy(this)}
{
    setObjectName(SplitterWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_splitter);

    m_splitter->addWidget(m_dummy);

    setupActions();
}

void SplitterWidget::setupActions()
{
    m_changeSplitter = new QAction("Change Splitter", this);

    QAction::connect(m_changeSplitter, &QAction::triggered, this, [this] {
        setOrientation(m_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
        setObjectName(QString("%1 Splitter").arg(Utils::EnumHelper::toString(m_splitter->orientation())));
    });
}

SplitterWidget::~SplitterWidget() = default;

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

QWidget* SplitterWidget::widget(int index) const
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
    if(m_children.isEmpty()) {
        m_dummy->hide();
    }
    int index = static_cast<int>(m_children.count());
    m_children.append(widget);
    return m_splitter->insertWidget(index, widget);
}

void SplitterWidget::insertWidget(int index, FyWidget* widget)
{
    if(!widget) {
        return;
    }
    if(m_children.isEmpty()) {
        m_dummy->hide();
    }
    m_children.insert(index, widget);
    m_splitter->insertWidget(index, widget);
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
    int index = findIndex(oldWidget);
    replaceWidget(index, newWidget);
}

void SplitterWidget::removeWidget(FyWidget* widget)
{
    int index = findIndex(widget);
    if(index != -1) {
        widget->deleteLater();
        m_children.remove(index);
    }
    if(m_children.isEmpty()) {
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

QList<FyWidget*> SplitterWidget::children()
{
    return m_children;
}

QString SplitterWidget::name() const
{
    return QString("%1 Splitter").arg(orientation() == Qt::Horizontal ? "Horizontal" : "Vertical");
}

void SplitterWidget::layoutEditingMenu(ActionContainer* menu)
{
    menu->addAction(m_changeSplitter);
}

void SplitterWidget::saveSplitter(QJsonObject& object, QJsonArray& splitterArray)
{
    QJsonArray array;

    for(const auto& widget : children()) {
        auto* childSplitter = qobject_cast<SplitterWidget*>(widget);
        if(childSplitter) {
            childSplitter->saveSplitter(object, array);
        }
        else {
            widget->saveLayout(array);
        }
    }
    QString state = QString::fromUtf8(saveState().toBase64());

    QJsonObject children;
    children["Type"] = Utils::EnumHelper::toString(orientation());
    children["State"] = state;
    children["Children"] = array;

    if(!findParent()) {
        object["Splitter"] = children;
    }
    else {
        QJsonObject object;
        object["Splitter"] = children;
        splitterArray.append(object);
    }
}

void SplitterWidget::loadSplitter(const QJsonArray& array, SplitterWidget* splitter)
{
    for(const auto& widget : array) {
        QJsonObject object = widget.toObject();
        if(!object.isEmpty()) {
            if(object.contains("Splitter")) {
                QJsonObject childSplitterObject = object["Splitter"].toObject();
                auto type = Utils::EnumHelper::fromString<Qt::Orientation>(childSplitterObject["Type"].toString());

                QJsonArray splitterArray = childSplitterObject["Children"].toArray();
                QByteArray splitterState = QByteArray::fromBase64(childSplitterObject["State"].toString().toUtf8());

                auto* childSplitter = Widgets::WidgetProvider::createSplitter(type, this);
                splitter->addWidget(childSplitter);
                loadSplitter(splitterArray, childSplitter);
                childSplitter->restoreState(splitterState);
            }
            else {
                const auto jObject = object.constBegin().key();
                auto* childWidget = m_widgetProvider->createWidget(jObject);
                splitter->addWidget(childWidget);
                auto widgetObject = object.value(jObject).toObject();
                childWidget->loadLayout(widgetObject);
            }
        }
        else {
            auto* childWidget = m_widgetProvider->createWidget(widget.toString());
            splitter->addWidget(childWidget);
        }
    }
}
}; // namespace Core::Widgets
