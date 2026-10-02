/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
WidgetContainer::WidgetContainer(WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_widgetProvider{widgetProvider}
    , m_settings{settings}
{ }

FyWidget* WidgetContainer::widgetAtPosition(const QPoint& /*pos*/) const
{
    return nullptr;
}

QRect WidgetContainer::widgetGeometry(FyWidget* widget) const
{
    if(!widget) {
        return {};
    }

    return {widget->mapTo(const_cast<WidgetContainer*>(this), QPoint{}), widget->size()}; // NOLINT
}

int WidgetContainer::fullWidgetCount() const
{
    return widgetCount();
}

Qt::Orientation WidgetContainer::orientation() const
{
    return Qt::Horizontal;
}

void WidgetContainer::removeWidget(int index)
{
    if(auto* widget = takeWidget(index)) {
        widget->deleteLater();
    }
}

void WidgetContainer::replaceWidget(int index, FyWidget* newWidget)
{
    if(auto* previous = exchangeWidget(index, newWidget)) {
        previous->deleteLater();
    }
}

FyWidget* WidgetContainer::exchangeWidget(int index, FyWidget* newWidget)
{
    auto* previous = widgetAtIndex(index);
    if(!previous || !newWidget || previous == newWidget || newWidget == this || newWidget->isAncestorOf(this)) {
        return nullptr;
    }

    if(auto* parent = qobject_cast<WidgetContainer*>(newWidget->findParent());
       parent && parent->widgetIndex(newWidget->id()) >= 0) {
        return nullptr;
    }

    const auto state  = saveEditingState();
    const bool hidden = previous->isHidden();
    if(auto* detached = exchangeWidgetImpl(index, newWidget)) {
        newWidget->setVisible(!hidden);
        restoreEditingState(state);
        return detached;
    }
    return nullptr;
}

QByteArray WidgetContainer::saveState() const
{
    return {};
}

bool WidgetContainer::restoreState(const QByteArray& /*state*/)
{
    return true;
}

QJsonObject WidgetContainer::saveChildState(int /*index*/) const
{
    return {};
}

void WidgetContainer::restoreChildState(int /*index*/, const QJsonObject& /*state*/) { }

QJsonObject WidgetContainer::saveEditingState() const
{
    QJsonArray children;
    const auto count = widgets().size();
    for(int i{0}; std::cmp_less(i, count); ++i) {
        children.append(saveChildState(i));
    }
    return {{"State"_L1, QString::fromUtf8(saveState().toBase64())}, {"Children"_L1, children}};
}

void WidgetContainer::restoreEditingState(const QJsonObject& state)
{
    restoreState(QByteArray::fromBase64(state.value("State"_L1).toString().toUtf8()));

    const auto children = state.value("Children"_L1).toArray();
    const auto count    = widgets().size();
    for(int i{0}; i < children.size() && std::cmp_less(i, count); ++i) {
        restoreChildState(i, children.at(i).toObject());
    }
}

void WidgetContainer::saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot)
{
    FyWidget::saveCopyLayoutData(layout, context, isRoot);

    const auto childWidgets = widgets();
    if(childWidgets.empty()) {
        return;
    }

    QJsonArray children;
    for(FyWidget* widget : childWidgets) {
        if(widget) {
            widget->saveCopyLayout(children, context, false);
        }
    }

    layout["Widgets"_L1] = children;
}

void WidgetContainer::loadWidgets(const QJsonArray& widgets)
{
    std::vector<FyWidget*> addedWidgets;

    for(const auto& widget : widgets) {
        if(!widget.isObject()) {
            continue;
        }
        const QJsonObject widgetObject = widget.toObject();

        const auto widgetName = widgetObject.constBegin().key();
        const auto childValue = widgetObject.value(widgetName);

        bool currentIsMissing{false};
        FyWidget* childWidget{nullptr};
        if(!m_widgetProvider->widgetExists(widgetName)) {
            currentIsMissing = true;
            childWidget      = new Dummy(widgetName, m_settings, this);
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
                        const QJsonObject missingData = dummy->missingLayoutData();
                        childWidget->deleteLater();
                        childWidget = m_widgetProvider->createWidget(missingName);

                        if(childWidget && !missingData.empty()) {
                            childWidget->loadLayout(missingData);
                        }
                    }
                }
            }

            if(childWidget) {
                addWidget(childWidget);
                addedWidgets.emplace_back(childWidget);
            }
        }
    }

    for(const auto& childWidget : addedWidgets) {
        childWidget->finalise();
    }
}
} // namespace Fooyin

#include "gui/moc_widgetcontainer.cpp"
