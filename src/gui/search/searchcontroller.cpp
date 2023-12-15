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

#include <gui/editablelayout.h>
#include <gui/fywidget.h>
#include <gui/widgetfilter.h>
#include <utils/widgets/overlaywidget.h>

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct SearchController::Private
{
    SearchController* self;
    EditableLayout* editableLayout;

    QPointer<WidgetFilter> filter;

    QPointer<OverlayWidget> controlDialog;
    std::unordered_map<Id, OverlayWidget*, Id::IdHash> overlays;
    QColor connectedColour{Qt::green};
    QColor disconnectedColour{Qt::red};

    QPointer<QPushButton> clearAll;
    QPointer<QPushButton> finishEditing;

    std::unordered_map<Id, FyWidget*, Id::IdHash> searchableWidgets;
    std::unordered_map<Id, std::set<Id>, Id::IdHash> connections;

    explicit Private(SearchController* self, EditableLayout* editableLayout)
        : self{self}
        , editableLayout{editableLayout}
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

    void updateDialog(const Id& sourceId) const
    {
        clearAll->setEnabled(connections.contains(sourceId) && !connections.at(sourceId).empty());
    }

    bool isConnected(const Id& sourceId, const Id& widgetId)
    {
        return connections.contains(sourceId) && connections.at(sourceId).contains(widgetId);
    }

    bool isConnectedToOther(const Id& sourceId, const Id& widgetId)
    {
        return std::ranges::any_of(connections, [sourceId, widgetId](const auto& connection) {
            return (connection.first != sourceId && connection.second.contains(widgetId));
        });
    }

    void addOrRemoveConnection(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay)
    {
        const Id id = widget->id();

        if(isConnected(sourceId, id)) {
            connections[sourceId].erase(id);
            overlay->button()->setText(tr("Connect"));
            overlay->setColour(disconnectedColour);
        }
        else {
            connections[sourceId].emplace(id);
            overlay->button()->setText(tr("Disconnect"));
            overlay->setColour(connectedColour);
        }

        updateDialog(sourceId);
    }

    void setupWidgetOverlay(const Id& sourceId, FyWidget* widget)
    {
        const Id widgetId = widget->id();

        const bool connectedToOther = isConnectedToOther(sourceId, widgetId);
        constexpr auto overlayFlags = OverlayWidget::Button | OverlayWidget::Resize;

        auto* overlay = overlays.emplace(widgetId, new OverlayWidget(overlayFlags, widget)).first->second;

        if(connectedToOther) {
            auto* conectedLabel = new QLabel(tr("Connected to another widget"), overlay);
            overlay->addWidget(conectedLabel);
        }
        else {
            const bool connected = connections.contains(sourceId) && connections.at(sourceId).contains(widgetId);
            overlay->setColour(connected ? connectedColour : disconnectedColour);

            overlay->button()->setText(tr(connected ? "Disconnect" : "Connect"));

            QObject::connect(overlay->button(), &QPushButton::clicked, self,
                             [this, sourceId, widget, overlay]() { addOrRemoveConnection(sourceId, widget, overlay); });
        }
        overlay->resize(widget->size());

        overlay->show();
    }

    void createControlDialog(const Id& sourceId)
    {
        controlDialog = new OverlayWidget(OverlayWidget::Static, editableLayout);

        auto* effect = new QGraphicsDropShadowEffect();
        effect->setBlurRadius(30);
        effect->setColor(Qt::black);
        effect->setOffset(1, 1);

        controlDialog->setGraphicsEffect(effect);

        clearAll = new QPushButton(tr("Clear All"), controlDialog);
        controlDialog->addWidget(clearAll);
        QObject::connect(clearAll, &QPushButton::clicked, self, [this, sourceId]() {
            for(const auto& [id, widget] : searchableWidgets) {
                if(isConnected(sourceId, id) && overlays.contains(id)) {
                    addOrRemoveConnection(sourceId, widget, overlays.at(id));
                }
            }
        });

        finishEditing = new QPushButton(tr("Finish"), controlDialog);
        controlDialog->addWidget(finishEditing);
        QObject::connect(finishEditing, &QPushButton::clicked, self, [this]() {
            filter->stop();
            filter->deleteLater();
            controlDialog->deleteLater();
            clearOverlays();
        });
        QObject::connect(filter, &WidgetFilter::filterFinished, self, [this]() {
            if(finishEditing) {
                QMetaObject::invokeMethod(finishEditing, "clicked", Q_ARG(bool, false));
            }
        });

        updateDialog(sourceId);

        controlDialog->move(editableLayout->width() - 160, editableLayout->height() - 140);
        controlDialog->resize(120, 80);
    }

    void setupWidgetConnections(const Id& sourceId)
    {
        clearOverlays();

        const auto widgets = editableLayout->findWidgetsByFeatures(FyWidget::Search);

        if(widgets.empty()) {
            return;
        }

        filter = new WidgetFilter(self);
        filter->start();

        createControlDialog(sourceId);

        std::ranges::for_each(widgets, [this, &sourceId](FyWidget* widget) {
            searchableWidgets.emplace(widget->id(), widget);
            setupWidgetOverlay(sourceId, widget);
        });

        controlDialog->show();
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
