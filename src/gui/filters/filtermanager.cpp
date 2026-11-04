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

#include "filtermanager.h"

#include "filtercontroller.h"
#include "filterwidget.h"
#include "tracklistwidget.h"

#include <core/library/musiclibrary.h>
#include <gui/layout/editablelayout.h>
#include <gui/widgetfilter.h>
#include <gui/widgets/overlaywidget.h>
#include <utils/async.h>
#include <utils/crypto.h>

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <ranges>

namespace Fooyin::Filters {
namespace {
QColor generateRandomUniqueColor(const Id& id)
{
    auto uniqueColour = QColor::fromRgb(static_cast<QRgb>(id.id()));
    uniqueColour.setAlpha(80);
    return uniqueColour;
}

void updateOverlayLabel(FilterWidget* filter, OverlayWidget* overlay)
{
    if(!filter || !overlay) {
        return;
    }

    overlay->label()->setText(QString::number(filter->index() + 1));
}
} // namespace

class FilterManagerPrivate
{
public:
    FilterManagerPrivate(FilterManager* self, FilterController* controller, EditableLayout* editableLayout);

    void enterGroupMode() const;
    void exitGroupMode();

    void clearOverlays();
    void hideAndDeselectOverlay(FyWidget* widget) const;

    void updateDialog() const;

    void addOrRemoveFilter(FilterWidget* widget, const QColor& colour) const;
    void addOrRemoveViewer(TrackListWidget* widget, const QColor& colour) const;
    void setupOverlayButtons(const Id& group, const QColor& colour);
    OverlayWidget* setupWidgetOverlay(FilterWidget* widget, const QColor& colour);
    OverlayWidget* setupWidgetOverlay(TrackListWidget* widget, const QColor& colour);

    void setupOverlays();

    void createControlDialog();

    FilterManager* m_self;

    FilterController* m_controller;
    EditableLayout* m_editableLayout;
    QPointer<OverlayWidget> m_controlDialog;
    std::unordered_map<Id, OverlayWidget*, Id::IdHash> m_overlays;

    QPointer<WidgetFilter> m_widgetFilter;

    Id m_selectedGroup;
    QColor m_ungroupedColour{Qt::red};

    QPointer<QPushButton> m_addGroup;
    QPointer<QPushButton> m_clearGroups;
    QPointer<QPushButton> m_goBack;
    QPointer<QPushButton> m_finishEditing;
};

FilterManagerPrivate::FilterManagerPrivate(FilterManager* self, FilterController* controller,
                                           EditableLayout* editableLayout)
    : m_self{self}
    , m_controller{controller}
    , m_editableLayout{editableLayout}
{
    m_ungroupedColour.setAlpha(20);
}

void FilterManagerPrivate::enterGroupMode() const
{
    m_clearGroups->setText(FilterManager::tr("Clear Group"));

    m_addGroup->hide();
    m_finishEditing->hide();
    m_goBack->show();
}

void FilterManagerPrivate::exitGroupMode()
{
    m_selectedGroup = {};
    m_clearGroups->setText(FilterManager::tr("Clear Groups"));

    m_goBack->hide();
    m_addGroup->show();
    m_finishEditing->show();

    m_controlDialog->adjustSize();
}

void FilterManagerPrivate::clearOverlays()
{
    for(auto* overlay : m_overlays | std::views::values) {
        overlay->deleteLater();
    }
    m_overlays.clear();
}

void FilterManagerPrivate::hideAndDeselectOverlay(FyWidget* widget) const
{
    const Id id = widget->id();
    if(m_overlays.contains(id)) {
        auto* overlay = m_overlays.at(id);
        overlay->button()->hide();
        overlay->deselect();

        const bool ungrouped = m_controller->filterIsUngrouped(id) || m_controller->viewerIsUngrouped(id);
        overlay->setOption(OverlayWidget::Selectable, !ungrouped);
    }
}

void FilterManagerPrivate::updateDialog() const
{
    m_addGroup->setEnabled(m_controller->haveUngroupedFilters());
    m_clearGroups->setEnabled(!m_controller->filterGroups().empty());
}

void FilterManagerPrivate::addOrRemoveFilter(FilterWidget* widget, const QColor& colour) const
{
    const Id id = widget->id();

    if(!m_overlays.contains(id)) {
        return;
    }

    auto* overlay = m_overlays.at(id);

    const Id groupId         = widget->group();
    const bool addingToGroup = m_controller->filterIsUngrouped(id);

    if(m_controller->removeFilter(widget)) {
        if(addingToGroup) {
            m_controller->addFilterToGroup(widget, m_selectedGroup);

            overlay->label()->setText(QString::number(widget->index() + 1));
            overlay->button()->setText(FilterManager::tr("Remove"));
            overlay->setColour(colour);
            overlay->select();

            if(const auto group = m_controller->groupById(m_selectedGroup)) {
                for(const FyWidget* filter : group.value().filters) {
                    if(m_overlays.contains(filter->id())) {
                        m_overlays.at(filter->id())->connectOverlay(overlay);
                    }
                }
                for(const FyWidget* viewer : group->viewers) {
                    if(m_overlays.contains(viewer->id())) {
                        m_overlays.at(viewer->id())->connectOverlay(overlay);
                    }
                }
            }
        }
        else {
            m_controller->addFilterToGroup(widget, {});
            overlay->label()->setText(FilterManager::tr("Ungrouped"));
            overlay->button()->setText(FilterManager::tr("Add"));
            overlay->setColour(m_ungroupedColour);
            overlay->setOption(OverlayWidget::Selectable, false);

            if(const auto group = m_controller->groupById(groupId)) {
                for(FilterWidget* filter : group.value().filters) {
                    if(m_overlays.contains(filter->id())) {
                        auto* filterOverlay = m_overlays.at(filter->id());
                        overlay->disconnectOverlay(filterOverlay);
                        updateOverlayLabel(filter, filterOverlay);
                    }
                }
                for(const FyWidget* viewer : group->viewers) {
                    if(m_overlays.contains(viewer->id())) {
                        overlay->disconnectOverlay(m_overlays.at(viewer->id()));
                    }
                }
            }
        }
    }

    updateDialog();
}

void FilterManagerPrivate::addOrRemoveViewer(TrackListWidget* widget, const QColor& colour) const
{
    const Id id = widget->id();
    if(!m_overlays.contains(id)) {
        return;
    }

    auto* overlay            = m_overlays.at(id);
    const Id groupId         = widget->group();
    const bool addingToGroup = m_controller->viewerIsUngrouped(id);

    if(m_controller->removeViewer(widget)) {
        if(addingToGroup) {
            m_controller->addViewerToGroup(widget, m_selectedGroup);
            overlay->label()->setText(FilterManager::tr("Viewer"));
            overlay->button()->setText(FilterManager::tr("Remove"));
            overlay->setColour(colour);
            overlay->select();

            if(const auto group = m_controller->groupById(m_selectedGroup)) {
                for(const FyWidget* member : group->filters) {
                    if(m_overlays.contains(member->id())) {
                        m_overlays.at(member->id())->connectOverlay(overlay);
                    }
                }
                for(const FyWidget* member : group->viewers) {
                    if(m_overlays.contains(member->id())) {
                        m_overlays.at(member->id())->connectOverlay(overlay);
                    }
                }
            }
        }
        else {
            m_controller->addViewerToGroup(widget, {});
            overlay->label()->setText(FilterManager::tr("Ungrouped"));
            overlay->button()->setText(FilterManager::tr("Add"));
            overlay->setColour(m_ungroupedColour);
            overlay->setOption(OverlayWidget::Selectable, false);

            if(const auto group = m_controller->groupById(groupId)) {
                for(const FyWidget* member : group->filters) {
                    if(m_overlays.contains(member->id())) {
                        overlay->disconnectOverlay(m_overlays.at(member->id()));
                    }
                }
                for(const FyWidget* member : group->viewers) {
                    if(m_overlays.contains(member->id())) {
                        overlay->disconnectOverlay(m_overlays.at(member->id()));
                    }
                }
            }
        }
    }

    updateDialog();
}

void FilterManagerPrivate::setupOverlayButtons(const Id& group, const QColor& colour)
{
    auto setupFilterOverlayButtons = [this, &colour](const Id& id, FilterWidget* widget) {
        if(!m_overlays.contains(id)) {
            return;
        }

        OverlayWidget* overlay = m_overlays.at(id);
        overlay->button()->setText(FilterManager::tr(m_controller->filterIsUngrouped(id) ? "Add" : "Remove"));
        overlay->button()->show();
        overlay->button()->disconnect(m_self);
        QObject::connect(overlay->button(), &QPushButton::clicked, m_self,
                         [this, widget, colour]() { addOrRemoveFilter(widget, colour); });
    };
    auto setupViewerOverlayButtons = [this, &colour](const Id& id, TrackListWidget* widget) {
        if(!m_overlays.contains(id)) {
            return;
        }

        OverlayWidget* overlay = m_overlays.at(id);
        overlay->button()->setText(FilterManager::tr(m_controller->viewerIsUngrouped(id) ? "Add" : "Remove"));
        overlay->button()->show();
        overlay->button()->disconnect(m_self);
        QObject::connect(overlay->button(), &QPushButton::clicked, m_self,
                         [this, widget, colour]() { addOrRemoveViewer(widget, colour); });
    };

    if(group.isValid()) {
        const auto groups        = m_controller->filterGroups();
        const auto& groupWidgets = groups.at(group).filters;
        const auto& groupViewers = groups.at(group).viewers;

        for(FilterWidget* widget : groupWidgets) {
            setupFilterOverlayButtons(widget->id(), widget);
        }
        for(TrackListWidget* widget : groupViewers) {
            setupViewerOverlayButtons(widget->id(), widget);
        }
    }

    const auto ungrouped = m_controller->ungroupedFilters();
    for(const auto& [id, widget] : ungrouped) {
        setupFilterOverlayButtons(id, widget);
    }

    const auto ungroupedViewers = m_controller->ungroupedViewers();
    for(const auto& [id, widget] : ungroupedViewers) {
        setupViewerOverlayButtons(id, widget);
    }
}

OverlayWidget* FilterManagerPrivate::setupWidgetOverlay(FilterWidget* widget, const QColor& colour)
{
    const Id widgetId           = widget->id();
    constexpr auto overlayFlags = OverlayWidget::Label | OverlayWidget::Button | OverlayWidget::Resize;
    auto* overlay               = m_overlays.emplace(widgetId, new OverlayWidget(overlayFlags, widget)).first->second;

    overlay->button()->hide();
    overlay->setColour(colour);

    if(widget->group().isValid()) {
        overlay->label()->setText(QString::number(widget->index() + 1));
        overlay->setOption(OverlayWidget::Selectable);
    }
    else {
        overlay->label()->setText(FilterManager::tr("Ungrouped"));
    }

    QObject::connect(overlay, &OverlayWidget::clicked, m_self, [this, widget, overlay]() {
        m_selectedGroup = widget->group();
        setupOverlayButtons(m_selectedGroup, overlay->colour());
        enterGroupMode();
    });

    overlay->resize(widget->size());
    overlay->show();

    return overlay;
}

OverlayWidget* FilterManagerPrivate::setupWidgetOverlay(TrackListWidget* widget, const QColor& colour)
{
    const Id widgetId           = widget->id();
    constexpr auto overlayFlags = OverlayWidget::Label | OverlayWidget::Button | OverlayWidget::Resize;
    auto* overlay               = m_overlays.emplace(widgetId, new OverlayWidget(overlayFlags, widget)).first->second;

    const bool groupIsValid = widget->group().isValid();

    overlay->button()->hide();
    overlay->setColour(colour);
    overlay->label()->setText(groupIsValid ? FilterManager::tr("Viewer") : FilterManager::tr("Ungrouped"));
    overlay->setOption(OverlayWidget::Selectable, groupIsValid);

    QObject::connect(overlay, &OverlayWidget::clicked, m_self, [this, widget, overlay]() {
        m_selectedGroup = widget->group();
        setupOverlayButtons(m_selectedGroup, overlay->colour());
        enterGroupMode();
    });

    overlay->resize(widget->size());
    overlay->show();

    return overlay;
}

void FilterManagerPrivate::setupOverlays()
{
    const auto groups = m_controller->filterGroups();

    for(const auto& group : groups | std::views::values) {
        if(!group.filters.empty() || !group.viewers.empty()) {
            const QColor groupColour = generateRandomUniqueColor(group.id);
            std::vector<OverlayWidget*> groupOverlays;

            std::ranges::transform(
                group.filters, std::back_inserter(groupOverlays),
                [this, &groupColour](FilterWidget* widget) { return setupWidgetOverlay(widget, groupColour); });
            std::ranges::transform(
                group.viewers, std::back_inserter(groupOverlays),
                [this, &groupColour](TrackListWidget* widget) { return setupWidgetOverlay(widget, groupColour); });

            for(auto it1 = groupOverlays.begin(); it1 != groupOverlays.end(); ++it1) {
                for(auto it2 = std::next(it1); it2 != groupOverlays.end(); ++it2) {
                    (*it1)->connectOverlay(*it2);
                }
            }
        }
    }

    const auto ungrouped = m_controller->ungroupedFilters();

    std::ranges::for_each(ungrouped | std::views::values,
                          [this](FilterWidget* widget) { setupWidgetOverlay(widget, m_ungroupedColour); });
    std::ranges::for_each(m_controller->ungroupedViewers() | std::views::values,
                          [this](TrackListWidget* widget) { setupWidgetOverlay(widget, m_ungroupedColour); });
}

void FilterManagerPrivate::createControlDialog()
{
    m_controlDialog = new OverlayWidget(OverlayWidget::Static, m_editableLayout);

    auto highlight = m_controlDialog->palette().color(QPalette::Highlight);
    highlight.setAlpha(160);
    m_controlDialog->setColour(highlight);

    auto* effect = new QGraphicsDropShadowEffect(m_controlDialog);
    effect->setBlurRadius(30);
    effect->setColor(Qt::black);
    effect->setOffset(10, 10);

    m_controlDialog->setGraphicsEffect(effect);

    m_addGroup = new QPushButton(FilterManager::tr("Add New Group"), m_controlDialog);
    m_controlDialog->addWidget(m_addGroup);
    QObject::connect(m_addGroup, &QPushButton::clicked, m_self, [this]() {
        const auto newGroup = Id{Utils::generateUniqueHash()};
        m_selectedGroup     = newGroup;
        setupOverlayButtons({}, generateRandomUniqueColor(newGroup));
        enterGroupMode();
    });

    m_clearGroups = new QPushButton(FilterManager::tr("Clear Groups"), m_controlDialog);
    m_controlDialog->addWidget(m_clearGroups);
    QObject::connect(m_clearGroups, &QPushButton::clicked, m_self, [this]() {
        const auto groups = m_controller->filterGroups();

        if(m_selectedGroup.isValid()) {
            const auto& group  = groups.at(m_selectedGroup);
            const auto filters = group.filters;
            std::ranges::for_each(filters,
                                  [this](FilterWidget* widget) { addOrRemoveFilter(widget, m_ungroupedColour); });
            const auto viewers = group.viewers;
            std::ranges::for_each(viewers,
                                  [this](TrackListWidget* widget) { addOrRemoveViewer(widget, m_ungroupedColour); });
        }
        else {
            for(const auto& group : groups | std::views::values) {
                const auto filters = group.filters;
                std::ranges::for_each(filters,
                                      [this](FilterWidget* widget) { addOrRemoveFilter(widget, m_ungroupedColour); });
                const auto viewers = group.viewers;
                std::ranges::for_each(
                    viewers, [this](TrackListWidget* widget) { addOrRemoveViewer(widget, m_ungroupedColour); });
            }
        }
    });

    m_goBack = new QPushButton(FilterManager::tr("Back"), m_controlDialog);
    m_controlDialog->addWidget(m_goBack);
    m_goBack->hide();
    QObject::connect(m_goBack, &QPushButton::clicked, m_self, [this]() {
        const auto groups = m_controller->filterGroups();
        if(m_selectedGroup.isValid() && groups.contains(m_selectedGroup)) {
            const auto& group = groups.at(m_selectedGroup);
            std::ranges::for_each(group.filters, [this](FilterWidget* filter) { hideAndDeselectOverlay(filter); });
            std::ranges::for_each(group.viewers, [this](TrackListWidget* viewer) { hideAndDeselectOverlay(viewer); });
        }

        const auto ungrouped = m_controller->ungroupedFilters();
        for(auto* filter : ungrouped | std::views::values) {
            hideAndDeselectOverlay(filter);
        }
        for(auto* viewer : m_controller->ungroupedViewers() | std::views::values) {
            hideAndDeselectOverlay(viewer);
        }
        exitGroupMode();
    });

    m_finishEditing = new QPushButton(FilterManager::tr("Finish"), m_controlDialog);
    m_controlDialog->addWidget(m_finishEditing);
    QObject::connect(m_finishEditing, &QPushButton::clicked, m_self, [this]() {
        m_widgetFilter->stop();
        m_widgetFilter->deleteLater();
        m_controlDialog->deleteLater();
        clearOverlays();
    });

    QObject::connect(m_widgetFilter, &WidgetFilter::filterFinished, m_self, [this]() {
        if(m_goBack && !m_goBack->isHidden()) {
            QMetaObject::invokeMethod(m_goBack, "clicked", Q_ARG(bool, false));
        }
        else {
            QMetaObject::invokeMethod(m_finishEditing, "clicked", Q_ARG(bool, false));
        }
    });

    m_controlDialog->move(m_editableLayout->width() - 160, m_editableLayout->height() - 160);
}

FilterManager::FilterManager(FilterController* controller, EditableLayout* editableLayout, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<FilterManagerPrivate>(this, controller, editableLayout)}
{ }

FilterManager::~FilterManager() = default;

void FilterManager::setupWidgetConnections()
{
    p->m_widgetFilter = new WidgetFilter(p->m_editableLayout, this);
    p->m_widgetFilter->start();

    p->clearOverlays();
    p->setupOverlays();

    p->createControlDialog();
    p->updateDialog();

    p->m_controlDialog->show();
}
} // namespace Fooyin::Filters

#include "moc_filtermanager.cpp"
