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

#include "searchcontroller.h"

#include <gui/fywidget.h>
#include <gui/layout/editablelayout.h>
#include <gui/widgetfilter.h>
#include <gui/widgets/overlaywidget.h>

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <ranges>
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
    bool isConnected(const Id& sourceId, const Id& widgetId);
    bool isConnectedToOther(const Id& sourceId, const Id& widgetId);

    void addOrRemoveConnection(const Id& sourceId, FyWidget* widget);
    void updateWidgetOverlay(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay);
    void setupWidgetOverlay(const Id& sourceId, FyWidget* widget);
    void createControlDialog(const Id& sourceId);

    void retrieveWidgets();
    void setupWidgetConnections(const Id& sourceId);
    void removeWidget(const Id& widgetId, QObject* object);

    void setSearch(const Id& id, const QString& Search);

    SearchController* m_self;
    QPointer<EditableLayout> m_editableLayout;

    QPointer<WidgetFilter> m_filter;

    QPointer<OverlayWidget> m_controlDialog;
    std::unordered_map<Id, QPointer<OverlayWidget>, Id::IdHash> m_overlays;
    QColor m_connectedColour{Qt::green};
    QColor m_disconnectedColour{Qt::red};
    QColor m_unavailableColour{Qt::gray};

    QPointer<QPushButton> m_clearAll;
    QPointer<QPushButton> m_finishEditing;

    std::unordered_map<Id, FyWidget*, Id::IdHash> m_searchableWidgets;
    std::unordered_map<Id, std::set<Id>, Id::IdHash> m_connections;
    std::unordered_map<FyWidget*, std::function<void(const QString&)>> m_setFuncs;
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
    for(auto& widget : m_overlays | std::views::values) {
        if(widget) {
            widget->deleteLater();
        }
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

void SearchControllerPrivate::addOrRemoveConnection(const Id& sourceId, FyWidget* widget)
{
    const Id id = widget->id();

    if(isConnected(sourceId, id)) {
        m_connections[sourceId].erase(id);
        widget->searchEvent({});
    }
    else {
        m_connections[sourceId].emplace(id);
    }

    for(const auto& [widgetId, overlay] : m_overlays) {
        if(overlay && m_searchableWidgets.contains(widgetId)) {
            updateWidgetOverlay(sourceId, m_searchableWidgets.at(widgetId), overlay);
        }
    }
    updateDialog(sourceId);
    Q_EMIT m_self->connectionChanged(sourceId);
}

void SearchControllerPrivate::updateWidgetOverlay(const Id& sourceId, FyWidget* widget, OverlayWidget* overlay)
{
    const Id widgetId = widget->id();

    const bool connected        = isConnected(sourceId, widgetId);
    const bool connectedToOther = isConnectedToOther(sourceId, widgetId);
    const bool exclusiveWidget  = widget->hasFeature(FyWidget::ExclusiveSearch);
    const bool exclusiveSource  = hasExclusiveConnection(sourceId);
    const bool hasAnyConnection = hasConnections(sourceId);

    const bool isUnavailable
        = !connected && (connectedToOther || exclusiveSource || (exclusiveWidget && hasAnyConnection));
    overlay->button()->setDisabled(isUnavailable);

    if(isUnavailable) {
        overlay->setButtonText(SearchController::tr("Unavailable"));
        overlay->setColour(m_unavailableColour);
    }
    else {
        overlay->setButtonText(connected ? SearchController::tr("Disconnect") : SearchController::tr("Connect"));
        overlay->setColour(connected ? m_connectedColour : m_disconnectedColour);
    }
}

void SearchControllerPrivate::setupWidgetOverlay(const Id& sourceId, FyWidget* widget)
{
    const auto overlayFlags = OverlayWidget::Resize | OverlayWidget::Button;
    auto* overlay           = new OverlayWidget(overlayFlags, widget);
    m_overlays.emplace(widget->id(), overlay);
    QObject::connect(overlay->button(), &QPushButton::clicked, m_self,
                     [this, sourceId, widget]() { addOrRemoveConnection(sourceId, widget); });

    updateWidgetOverlay(sourceId, widget, overlay);
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
        const auto connections = m_connections[sourceId];
        m_connections[sourceId].clear();
        for(const auto& [id, widget] : m_searchableWidgets) {
            if(connections.contains(id)) {
                widget->searchEvent({});
            }
            if(m_overlays.contains(id) && m_overlays.at(id)) {
                updateWidgetOverlay(sourceId, widget, m_overlays.at(id));
            }
        }
        updateDialog(sourceId);
        Q_EMIT m_self->connectionChanged(sourceId);
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

    std::erase_if(m_searchableWidgets, [this, &widgets](const auto& entry) {
        const bool removed = std::ranges::none_of(widgets, [&entry](const FyWidget* widget) {
            return widget == entry.second && widget->id() == entry.first;
        });
        if(removed) {
            QObject::disconnect(entry.second, nullptr, m_self, nullptr);
        }
        return removed;
    });

    for(FyWidget* widget : widgets) {
        const Id widgetId = widget->id();

        if(!m_searchableWidgets.contains(widgetId) || m_searchableWidgets.at(widgetId) != widget) {
            QObject::connect(widget, &QObject::destroyed, m_self,
                             [this, widgetId](QObject* object) { removeWidget(widgetId, object); });
            if(widget->hasFeature(FyWidget::ExclusiveSearch)) {
                QObject::connect(widget, &FyWidget::changeSearch, m_self,
                                 [this, widgetId, widget](const QString& search) {
                                     if(!m_editableLayout || m_editableLayout->findWidget(widgetId) != widget) {
                                         return;
                                     }
                                     const Id sourceId = exclusiveConnection(widgetId);
                                     if(sourceId.isValid()) {
                                         setSearch(sourceId, search);
                                     }
                                 });
            }
        }
        m_searchableWidgets[widgetId] = widget;
    }
}

void SearchControllerPrivate::setupWidgetConnections(const Id& sourceId)
{
    m_self->loadWidgets();
    clearOverlays();

    m_filter = new WidgetFilter(m_editableLayout, m_self);
    m_filter->start();

    createControlDialog(sourceId);

    for(const auto& [_, widget] : m_searchableWidgets) {
        setupWidgetOverlay(sourceId, widget);
    }

    m_controlDialog->show();
}

void SearchControllerPrivate::removeWidget(const Id& widgetId, QObject* object)
{
    if(m_searchableWidgets.contains(widgetId) && m_searchableWidgets.at(widgetId) != object) {
        return;
    }

    m_searchableWidgets.erase(widgetId);
    for(const auto& [sourceId, connections] : m_connections) {
        if(connections.contains(widgetId)) {
            Q_EMIT m_self->connectionChanged(sourceId);
        }
    }
}

void SearchControllerPrivate::setSearch(const Id& id, const QString& Search)
{
    for(const auto& [source, func] : m_setFuncs) {
        if(source->id() == id && m_editableLayout && m_editableLayout->findWidget(id) == source) {
            func(Search);
            return;
        }
    }
}

SearchController::SearchController(EditableLayout* editableLayout, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<SearchControllerPrivate>(this, editableLayout)}
{
    QObject::connect(editableLayout, &EditableLayout::widgetsChanged, this, &SearchController::loadWidgets,
                     Qt::QueuedConnection);
}

SearchController::~SearchController() = default;

void SearchController::loadWidgets()
{
    if(!p->m_editableLayout) {
        return;
    }
    p->retrieveWidgets();
    // Search bars may have been shown before their replacement targets were discovered
    for(const auto& sourceId : p->m_connections | std::views::keys) {
        Q_EMIT connectionChanged(sourceId);
    }
}

void SearchController::setupWidgetConnections(const Id& id)
{
    p->setupWidgetConnections(id);
}

void SearchController::registerSetFunction(FyWidget* source, const std::function<void(const QString&)>& func)
{
    p->m_setFuncs.insert_or_assign(source, func);
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
            auto* widget = p->m_searchableWidgets.at(widgetId);
            if(p->m_editableLayout && p->m_editableLayout->findWidget(widgetId) == widget) {
                widgets.emplace_back(widget);
            }
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
    Q_EMIT connectionChanged(id);
}

void SearchController::removeConnectedWidgets(FyWidget* source)
{
    const Id id = source->id();
    p->m_setFuncs.erase(source);
    if(std::ranges::any_of(p->m_setFuncs, [&id](const auto& entry) { return entry.first->id() == id; })) {
        return;
    }
    p->m_connections.erase(id);
}

void SearchController::changeSearch(const Id& id, const SearchRequest& request)
{
    if(!p->m_connections.contains(id)) {
        return;
    }

    const auto connections = p->m_connections.at(id);
    for(const auto& widgetId : connections) {
        if(p->m_searchableWidgets.contains(widgetId)) {
            auto* widget = p->m_searchableWidgets.at(widgetId);
            if(p->m_editableLayout && p->m_editableLayout->findWidget(widgetId) == widget) {
                widget->searchEvent(request);
            }
        }
    }
}
} // namespace Fooyin

#include "moc_searchcontroller.cpp"
