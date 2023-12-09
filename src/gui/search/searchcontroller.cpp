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

#include <gui/searchcontroller.h>

#include "editablelayout.h"
#include "search/widgetfilter.h"

#include <gui/fywidget.h>
#include <utils/widgets/overlaywidget.h>

#include <QCoroCore>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct SearchController::Private
{
    SearchController* self;
    EditableLayout* editableLayout;

    WidgetFilter* filter;

    std::unordered_map<Id, OverlayWidget*, Id::IdHash> overlays;
    QColor connectedColour{Qt::green};
    QColor disconnectedColour{Qt::red};

    std::unordered_map<Id, FyWidget*, Id::IdHash> searchableWidgets;
    std::unordered_map<Id, std::set<Id>, Id::IdHash> connections;

    explicit Private(SearchController* self, EditableLayout* editableLayout)
        : self{self}
        , editableLayout{editableLayout}
        , filter{new WidgetFilter(self)}
    {
        connectedColour.setAlpha(80);
        disconnectedColour.setAlpha(80);
    }

    void clearOverlays()
    {
        for(auto& [id, widget] : overlays) {
            widget->deleteLater();
        }
        overlays.clear();
    }

    void handleWidgetOverlay(const Id& sourceId, FyWidget* widget)
    {
        const Id widgetId = widget->id();

        const bool connectedToOther = isConnectedToOther(sourceId, widget->id());

        auto* overlay
            = overlays
                  .emplace(widgetId, new OverlayWidget(connectedToOther ? OverlayWidget::Label : OverlayWidget::Button,
                                                       editableLayout))
                  .first->second;

        if(connectedToOther) {
            overlay->setText(u"Connected to another widget"_s);
        }
        else {
            const bool connected = connections.contains(sourceId) && connections.at(sourceId).contains(widgetId);
            overlay->setColour(connected ? connectedColour : disconnectedColour);
            overlay->setButtonText(connected ? u"Disconnect"_s : u"Connect"_s);
        }
        overlay->setGeometry(widget->widgetGeometry());

        QObject::connect(overlay, &OverlayWidget::buttonClicked, self,
                         [this, sourceId, widget, overlay]() { handleConnectionChanged(sourceId, widget, overlay); });

        overlay->show();
    }

    void handleConnectionChanged(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay)
    {
        const Id id = widget->id();

        if(connections.contains(sourceId) && connections.at(sourceId).contains(id)) {
            connections[sourceId].erase(id);
            searchableWidgets.erase(id);
            overlay->setButtonText(u"Connect"_s);
            overlay->setColour(disconnectedColour);
        }
        else {
            connections[sourceId].emplace(id);
            searchableWidgets.emplace(id, widget);
            overlay->setButtonText(u"Disconnect"_s);
            overlay->setColour(connectedColour);
        }
    }

    bool isConnectedToOther(const Id& sourceId, const Id& widgetId)
    {
        return std::ranges::any_of(connections, [sourceId, widgetId](const auto& connection) {
            return (connection.first != sourceId && connection.second.contains(widgetId));
        });
    }

    QCoro::Task<void> setupWidgetConnections(Id id)
    {
        clearOverlays();

        const auto widgets = editableLayout->findWidgetsByFeatures(FyWidget::Search);

        if(!widgets.empty()) {
            filter->start();

            for(const auto& widget : widgets) {
                handleWidgetOverlay(id, widget);
            }

            co_await qCoro(filter, &WidgetFilter::filterFinished);

            filter->stop();
            clearOverlays();
        }
    }
};

SearchController::SearchController(EditableLayout* editableLayout, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, editableLayout)}
{ }

SearchController::~SearchController() = default;

void SearchController::setupWidgetConnections(const Id& id)
{
    p->setupWidgetConnections(id);
}

IdSet SearchController::connectedWidgets(const Id& id)
{
    if(!p->connections.contains(id)) {
        return {};
    }

    return p->connections.at(id);
}

void SearchController::setConnectedWidgets(const Id& id, const IdSet& widgets)
{
    p->connections[id] = widgets;
}

void SearchController::removeConnectedWidgets(const Id& id)
{
    p->connections.erase(id);
}

void SearchController::changeSearch(const Id& id, const QString& search)
{
    if(!p->connections.contains(id)) {
        return;
    }

    for(const auto& widgetId : p->connections[id]) {
        if(p->searchableWidgets.contains(widgetId)) {
            if(auto* widget = p->searchableWidgets.at(widgetId)) {
                widget->searchEvent(search);
            }
        }
        else if(auto* widget = p->editableLayout->findWidget(widgetId)) {
            p->searchableWidgets.emplace(widgetId, widget);
            widget->searchEvent(search);
        }
    }
}
} // namespace Fooyin

#include "gui/moc_searchcontroller.cpp"
