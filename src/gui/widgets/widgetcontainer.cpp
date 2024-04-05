/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgetcontainer.h>

#include "widgets/dummy.h"

#include <gui/widgetprovider.h>

#include <QJsonArray>
#include <QJsonObject>

namespace Fooyin {
WidgetContainer::WidgetContainer(WidgetProvider* widgetProvider, QWidget* parent)
    : FyWidget{parent}
    , m_widgetProvider{widgetProvider}
{ }

Qt::Orientation WidgetContainer::orientation() const
{
    return Qt::Horizontal;
}

QByteArray WidgetContainer::saveState() const
{
    return {};
}

bool WidgetContainer::restoreState(const QByteArray& /*state*/)
{
    return true;
}

void WidgetContainer::loadWidgets(const QJsonArray& widgets)
{
    for(const auto& widget : widgets) {
        if(!widget.isObject()) {
            continue;
        }
        const QJsonObject widgetObject = widget.toObject();

        const auto widgetName = widgetObject.constBegin().key();
        const auto childValue = widgetObject.value(widgetName);

        bool currentIsMissing{false};
        FyWidget* childWidget{nullptr};
        if(!m_widgetProvider->canCreateWidget(widgetName)) {
            currentIsMissing = true;
            childWidget      = new Dummy(widgetName, this);
        }
        else {
            childWidget = m_widgetProvider->createWidget(widgetName);
        }

        if(childWidget) {
            if(childValue.isObject()) {
                childWidget->loadLayout(childValue.toObject());
            }

            if(!currentIsMissing) {
                if(auto* dummy = qobject_cast<Dummy*>(childWidget)) {
                    const QString missingName = dummy->missingName();

                    if(!missingName.isEmpty() && m_widgetProvider->canCreateWidget(missingName)) {
                        childWidget->deleteLater();
                        childWidget = m_widgetProvider->createWidget(missingName);

                        if(childValue.isObject()) {
                            childWidget->loadLayout(childValue.toObject());
                        }
                    }
                }
            }

            addWidget(childWidget);
            childWidget->finalise();
        }
    }
}
} // namespace Fooyin

#include "gui/moc_widgetcontainer.cpp"
