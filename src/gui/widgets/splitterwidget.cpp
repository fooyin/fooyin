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

#include "dummy.h"
#include "splitter.h"
#include "utils/enumhelper.h"
#include "utils/widgetprovider.h"

#include <QMenu>

SplitterWidget::SplitterWidget(Qt::Orientation type, WidgetProvider* widgetProvider, QWidget* parent)
    : Widget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_splitter(new Splitter(type))
    , m_widgetProvider(widgetProvider)
{
    setObjectName(QString("%1 Splitter").arg(type == Qt::Horizontal ? "Horizontal" : "Vertical"));

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_splitter);

    auto* dummy = new Dummy(this);
    addToSplitter(Widgets::WidgetType::Dummy, dummy);
}

SplitterWidget::~SplitterWidget() = default;

Qt::Orientation SplitterWidget::orientation() const
{
    return m_splitter->orientation();
}

void SplitterWidget::setOrientation(Qt::Orientation orientation)
{
    m_splitter->setOrientation(orientation);
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

void SplitterWidget::addToSplitter(Widgets::WidgetType type, QWidget* newWidget)
{
    int index = static_cast<int>(m_children.count());
    int dumIndex = findIndex(Widgets::WidgetType::Dummy);
    if(m_children.size() == 1 && dumIndex != -1) {
        index = placeholderIndex();
        m_splitter->widget(index)->deleteLater();
        m_children.remove(dumIndex);
    }
    m_children.append(SplitterEntry{type, newWidget});
    return m_splitter->insertWidget(index, newWidget);
}

void SplitterWidget::removeWidget(QWidget* widget)
{
    int index = findIndex(widget);
    if(index != -1) {
        widget->deleteLater();
        m_children.remove(index);
    }
    if(m_children.isEmpty()) {
        auto* dummy = new Dummy(this);
        m_splitter->addWidget(dummy);
        m_children.append(SplitterEntry{Widgets::WidgetType::Dummy, dummy});
    }
}

int SplitterWidget::findIndex(QWidget* widgetToFind)
{
    for(int i = 0; i < m_children.size(); ++i) {
        QWidget* widget = m_children.value(i).widget;
        if(widget == widgetToFind) {
            return i;
        }
    }
    return -1;
}

int SplitterWidget::findIndex(Widgets::WidgetType typeToFind)
{
    for(int i = 0; i < m_children.size(); ++i) {
        Widgets::WidgetType type = m_children.value(i).type;
        if(type == typeToFind) {
            return i;
        }
    }
    return -1;
}

QList<SplitterEntry> SplitterWidget::children()
{
    return m_children;
}

void SplitterWidget::layoutEditingMenu(QMenu* menu)
{
    auto* addMenu = new QMenu("Add", this);
    auto* changeSplitter = new QAction("Change Splitter", this);

    QAction::connect(changeSplitter, &QAction::triggered, this, [=] {
        setOrientation(m_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
        setObjectName(QString("%1 Splitter").arg(EnumHelper::toString(m_splitter->orientation())));
    });

    m_widgetProvider->addMenuActions(addMenu, this);

    menu->addAction(changeSplitter);
    menu->addMenu(addMenu);
}

int SplitterWidget::placeholderIndex() const
{
    for(int i = 0; i < m_splitter->count(); ++i) {
        auto* dummy = qobject_cast<Dummy*>(m_splitter->widget(i));
        if(dummy) {
            return i;
        }
    }
    return 0;
}
