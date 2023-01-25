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
#include "gui/widgetprovider.h"
#include "splitter.h"

#include <core/actions/actioncontainer.h>
#include <core/actions/actionmanager.h>
#include <core/plugins/pluginmanager.h>
#include <utils/enumhelper.h>

#include <QJsonObject>
#include <QMenu>

namespace Gui::Widgets {
SplitterWidget::SplitterWidget(Core::ActionManager* actionManager, Widgets::WidgetProvider* widgetProvider,
                               Core::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_actionManager{actionManager}
    , m_widgetProvider{widgetProvider}
    , m_layout{new QHBoxLayout(this)}
    , m_splitter{new Splitter(Qt::Vertical, settings, this)}
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
    const int index = static_cast<int>(m_children.count());
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
    const int index = findIndex(oldWidget);
    replaceWidget(index, newWidget);
}

void SplitterWidget::removeWidget(FyWidget* widget)
{
    const int index = findIndex(widget);
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

void SplitterWidget::layoutEditingMenu(Core::ActionContainer* menu)
{
    menu->addAction(m_changeSplitter);
}

void SplitterWidget::saveLayout(QJsonArray& array)
{
    QJsonArray childern;
    for(const auto& widget : children()) {
        widget->saveLayout(childern);
    }
    const QString state = QString::fromUtf8(saveState().toBase64());

    QJsonObject options;
    options["State"]    = state;
    options["Children"] = childern;

    QJsonObject splitter;
    splitter[name()] = options;
    array.append(splitter);
}

void SplitterWidget::loadLayout(QJsonObject& object)
{
    const auto state    = QByteArray::fromBase64(object["State"].toString().toUtf8());
    const auto children = object["Children"].toArray();

    for(const auto& widget : children) {
        const QJsonObject object = widget.toObject();
        if(!object.isEmpty()) {
            const auto name = object.constBegin().key();
            if(auto* childWidget = m_widgetProvider->createWidget(name)) {
                addWidget(childWidget);
                auto widgetObject = object.value(name).toObject();
                childWidget->loadLayout(widgetObject);
            }
        }
        else {
            auto* childWidget = m_widgetProvider->createWidget(widget.toString());
            addWidget(childWidget);
        }
    }
    restoreState(state);
}
} // namespace Gui::Widgets
