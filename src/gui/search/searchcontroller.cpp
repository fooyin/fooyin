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

#include "searchcontroller.h"

#include <gui/editablelayout.h>
#include <gui/fywidget.h>
#include <gui/widgetfilter.h>
#include <gui/widgets/overlaywidget.h>

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <set>

namespace Fooyin {
class SearchControllerPrivate
{
public:
    explicit SearchControllerPrivate(SearchController* self, EditableLayout* editableLayout);

    void clearOverlays();

    void updateDialog(const Id& sourceId) const;

    bool hasConnections(const Id& sourceId);
    bool hasExclusiveConnection(const Id& sourceId);
    Id exclusiveConnection(const Id& widgetId);
    bool isConnected(const Id& widgetId);
    bool isConnected(const Id& sourceId, const Id& widgetId);
    bool isConnectedToOther(const Id& sourceId, const Id& widgetId);

    void addOrRemoveConnection(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay);
    void setupWidgetOverlay(const Id& sourceId, FyWidget* widget);
    void createControlDialog(const Id& sourceId);

    void retrieveWidgets();
    void setupWidgetConnections(const Id& sourceId);
    void removeWidget(const Id& widgetId);

    void setSearch(const Id& id, const QString& Search);

    SearchController* m_self;
    EditableLayout* m_editableLayout;

    QPointer<WidgetFilter> m_filter;

    QPointer<OverlayWidget> m_controlDialog;
    std::unordered_map<Id, OverlayWidget*, Id::IdHash> m_overlays;
    QColor m_connectedColour{Qt::green};
    QColor m_disconnectedColour{Qt::red};
    QColor m_unavailableColour{Qt::gray};

    QPointer<QPushButton> m_clearAll;
    QPointer<QPushButton> m_finishEditing;

    std::unordered_map<Id, FyWidget*, Id::IdHash> m_searchableWidgets;
    std::unordered_map<Id, std::set<Id>, Id::IdHash> m_connections;
    std::unordered_map<Id, std::function<void(const QString&)>, Id::IdHash> m_setFuncs;
};

SearchControllerPrivate::SearchControllerPrivate(SearchController* self, EditableLayout* editableLayout)
    : m_self{self}
    , m_editableLayout{editableLayout}
{
    m_connectedColour.setAlpha(80);
    m_disconnectedColour.setAlpha(80);
    m_unavailableColour.setAlpha(80);
}

void SearchControllerPrivate::clearOverlays()
{
    for(auto& [id, widget] : m_overlays) {
        widget->deleteLater();
    }
    m_overlays.clear();
}

void SearchControllerPrivate::updateDialog(const Id& sourceId) const
{
    m_clearAll->setEnabled(m_connections.contains(sourceId) && !m_connections.at(sourceId).empty());
}

bool SearchControllerPrivate::hasConnections(const Id& sourceId)
{
    return m_connections.contains(sourceId) && !m_connections.at(sourceId).empty();
}

bool SearchControllerPrivate::hasExclusiveConnection(const Id& sourceId)
{
    if(!m_connections.contains(sourceId)) {
        return false;
    }

    const auto connections = m_connections.at(sourceId);
    return std::ranges::any_of(connections, [this](const auto& id) {
        return m_searchableWidgets.contains(id) && m_searchableWidgets.at(id)->hasFeature(FyWidget::ExclusiveSearch);
    });
}

Id SearchControllerPrivate::exclusiveConnection(const Id& widgetId)
{
    for(const auto& [sourceId, widgetIds] : m_connections) {
        if(std::ranges::any_of(widgetIds, [widgetId](const Id& id) { return widgetId == id; })) {
            return sourceId;
        }
    }
    return {};
}

bool SearchControllerPrivate::isConnected(const Id& widgetId)
{
    return std::ranges::any_of(m_connections,
                               [widgetId](const auto& connection) { return connection.second.contains(widgetId); });
}

bool SearchControllerPrivate::isConnected(const Id& sourceId, const Id& widgetId)
{
    return m_connections.contains(sourceId) && m_connections.at(sourceId).contains(widgetId);
}

bool SearchControllerPrivate::isConnectedToOther(const Id& sourceId, const Id& widgetId)
{
    return std::ranges::any_of(m_connections, [sourceId, widgetId](const auto& connection) {
        return (connection.first != sourceId && connection.second.contains(widgetId));
    });
}

void SearchControllerPrivate::addOrRemoveConnection(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay)
{
    const Id id = widget->id();

    const auto updateOverlay
        = [](OverlayWidget* olWidget, const QString& text, const QColor& color, bool enabled = true) {
              olWidget->setButtonText(text);
              olWidget->setColour(color);
              olWidget->button()->setEnabled(enabled);
          };

    const auto updateAllOverlays = [this, &sourceId, widget, &id, updateOverlay]() {
        const bool isExclusive = widget->hasFeature(FyWidget::ExclusiveSearch) && isConnected(id);

        for(const auto& [otherId, otherOverlay] : m_overlays) {
            if(otherId == id) {
                continue;
            }

            if(isExclusive) {
                updateOverlay(otherOverlay, SearchController::tr("Unavailable"), m_unavailableColour, false);
                continue;
            }

            if(m_searchableWidgets.contains(otherId)) {
                const auto* otherWidget = m_searchableWidgets.at(otherId);
                if((otherWidget->hasFeature(FyWidget::ExclusiveSearch)
                    && (isConnected(otherId) || hasConnections(sourceId)))) {
                    updateOverlay(otherOverlay, SearchController::tr("Unavailable"), m_unavailableColour, false);
                    continue;
                }
            }

            const bool otherConnected = isConnected(sourceId, otherId);
            updateOverlay(otherOverlay,
                          otherConnected ? SearchController::tr("Disconnect") : SearchController::tr("Connect"),
                          otherConnected ? m_connectedColour : m_disconnectedColour, true);
        }
    };

    if(isConnected(sourceId, id)) {
        widget->searchEvent({});
        m_connections[sourceId].erase(id);
        updateOverlay(overlay, SearchController::tr("Connect"), m_disconnectedColour);
    }
    else {
        m_connections[sourceId].emplace(id);
        updateOverlay(overlay, SearchController::tr("Disconnect"), m_connectedColour);
    }

    updateAllOverlays();
    updateDialog(sourceId);
    emit m_self->connectionChanged(sourceId);
}

void SearchControllerPrivate::setupWidgetOverlay(const Id& sourceId, FyWidget* widget)
{
    const Id widgetId = widget->id();

    const bool connected        = isConnected(sourceId, widgetId);
    const bool connectedToOther = isConnectedToOther(sourceId, widgetId);
    const bool exclusiveWidget  = widget->hasFeature(FyWidget::ExclusiveSearch);
    const bool exclusiveSource  = hasExclusiveConnection(sourceId);
    const bool hasAnyConnection = hasConnections(sourceId);

    const bool isUnavailable
        = connectedToOther || (!connected && (exclusiveSource || (exclusiveWidget && hasAnyConnection)));

    const auto overlayFlags = OverlayWidget::Resize | OverlayWidget::Button;
    auto* overlay           = m_overlays.emplace(widgetId, new OverlayWidget(overlayFlags, widget)).first->second;
    overlay->button()->setDisabled(isUnavailable);
    QObject::connect(overlay->button(), &QPushButton::clicked, m_self,
                     [this, sourceId, widget, overlay]() { addOrRemoveConnection(sourceId, widget, overlay); });

    if(isUnavailable) {
        overlay->setButtonText(SearchController::tr("Unavailable"));
        overlay->setColour(m_unavailableColour);
    }
    else {
        overlay->setButtonText(connected ? SearchController::tr("Disconnect") : SearchController::tr("Connect"));
        overlay->setColour(connected ? m_connectedColour : m_disconnectedColour);
    }

    overlay->resize(widget->size());
    overlay->show();
}

void SearchControllerPrivate::createControlDialog(const Id& sourceId)
{
    m_controlDialog = new OverlayWidget(OverlayWidget::Static, m_editableLayout);

    auto* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(30);
    effect->setColor(Qt::black);
    effect->setOffset(1, 1);

    m_controlDialog->setGraphicsEffect(effect);

    m_clearAll = new QPushButton(SearchController::tr("Clear All"), m_controlDialog);
    m_controlDialog->addWidget(m_clearAll);
    QObject::connect(m_clearAll, &QPushButton::clicked, m_self, [this, sourceId]() {
        for(const auto& [id, widget] : m_searchableWidgets) {
            if(isConnected(sourceId, id) && m_overlays.contains(id)) {
                addOrRemoveConnection(sourceId, widget, m_overlays.at(id));
            }
        }
    });

    m_finishEditing = new QPushButton(SearchController::tr("Finish"), m_controlDialog);
    m_controlDialog->addWidget(m_finishEditing);
    QObject::connect(m_finishEditing, &QPushButton::clicked, m_self, [this]() {
        m_filter->stop();
        m_filter->deleteLater();
        m_controlDialog->deleteLater();
        clearOverlays();
    });
    QObject::connect(m_filter, &WidgetFilter::filterFinished, m_self, [this]() {
        if(m_finishEditing) {
            QMetaObject::invokeMethod(m_finishEditing, "clicked", Q_ARG(bool, false));
        }
    });

    updateDialog(sourceId);

    m_controlDialog->move(m_editableLayout->width() - 160, m_editableLayout->height() - 140);
    m_controlDialog->resize(120, 80);
}

void SearchControllerPrivate::retrieveWidgets()
{
    const auto widgets = m_editableLayout->findWidgetsByFeatures(FyWidget::Search | FyWidget::ExclusiveSearch);

    if(widgets.empty()) {
        return;
    }

    for(FyWidget* widget : widgets) {
        const Id widgetId = widget->id();

        if(!m_searchableWidgets.contains(widgetId)) {
            QObject::connect(widget, &QObject::destroyed, m_self, [this, widgetId]() { removeWidget(widgetId); });
            if(widget->hasFeature(FyWidget::ExclusiveSearch)) {
                QObject::connect(widget, &FyWidget::changeSearch, m_self, [this, widgetId](const QString& search) {
                    const Id sourceId = exclusiveConnection(widgetId);
                    if(sourceId.isValid()) {
                        setSearch(sourceId, search);
                    }
                });
            }
        }
        m_searchableWidgets.emplace(widgetId, widget);
    }
}

void SearchControllerPrivate::setupWidgetConnections(const Id& sourceId)
{
    clearOverlays();

    if(m_searchableWidgets.empty()) {
        return;
    }

    m_filter = new WidgetFilter(m_editableLayout, m_self);
    m_filter->start();

    createControlDialog(sourceId);

    for(const auto& [_, widget] : m_searchableWidgets) {
        setupWidgetOverlay(sourceId, widget);
    }

    m_controlDialog->show();
}

void SearchControllerPrivate::removeWidget(const Id& widgetId)
{
    m_searchableWidgets.erase(widgetId);
    m_connections.erase(widgetId);
}

void SearchControllerPrivate::setSearch(const Id& id, const QString& Search)
{
    if(m_setFuncs.contains(id)) {
        m_setFuncs.at(id)(Search);
    }
}

SearchController::SearchController(EditableLayout* editableLayout, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<SearchControllerPrivate>(this, editableLayout)}
{ }

SearchController::~SearchController() = default;

void SearchController::loadWidgets()
{
    p->retrieveWidgets();
}

void SearchController::setupWidgetConnections(const Id& id)
{
    p->setupWidgetConnections(id);
}

void SearchController::registerSetFunction(const Id& id, const std::function<void(const QString&)>& func)
{
    p->m_setFuncs.emplace(id, func);
}

WidgetList SearchController::connectedWidgets(const Id& id)
{
    if(!p->m_connections.contains(id)) {
        return {};
    }

    WidgetList widgets;

    const auto connections = p->m_connections.at(id);
    for(const auto& widgetId : connections) {
        if(p->m_searchableWidgets.contains(widgetId)) {
            widgets.emplace_back(p->m_searchableWidgets.at(widgetId));
        }
    }

    return widgets;
}

IdSet SearchController::connectedWidgetIds(const Id& id)
{
    if(!p->m_connections.contains(id)) {
        return {};
    }

    return p->m_connections.at(id);
}

void SearchController::setConnectedWidgets(const Id& id, const IdSet& widgets)
{
    p->m_connections[id] = widgets;
}

void SearchController::removeConnectedWidgets(const Id& id)
{
    p->m_connections.erase(id);
}

void SearchController::changeSearch(const Id& id, const QString& search)
{
    if(!p->m_connections.contains(id)) {
        return;
    }

    const auto connections = p->m_connections.at(id);
    for(const auto& widgetId : connections) {
        if(p->m_searchableWidgets.contains(widgetId)) {
            auto* widget = p->m_searchableWidgets.at(widgetId);
            widget->searchEvent(search);
        }
    }
}
} // namespace Fooyin

#include "moc_searchcontroller.cpp"
