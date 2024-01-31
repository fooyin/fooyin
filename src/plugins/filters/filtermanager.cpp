/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/musiclibrary.h>
#include <gui/editablelayout.h>
#include <gui/widgetfilter.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/widgets/overlaywidget.h>

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <ranges>

namespace {
QColor generateRandomUniqueColor(const Fooyin::Id& id)
{
    auto uniqueColour = QColor::fromRgb(static_cast<QRgb>(id.id()));
    uniqueColour.setAlpha(80);
    return uniqueColour;
}

void updateOverlayLabel(Fooyin::Filters::FilterWidget* filter, Fooyin::OverlayWidget* overlay)
{
    if(!filter || !overlay) {
        return;
    }

    overlay->label()->setText(QString::number(filter->index() + 1));
}
} // namespace

namespace Fooyin::Filters {
struct FilterManager::Private
{
    FilterManager* self;

    FilterController* controller;
    EditableLayout* editableLayout;
    QPointer<OverlayWidget> controlDialog;
    std::unordered_map<Id, OverlayWidget*, Id::IdHash> overlays;

    QPointer<WidgetFilter> widgetFilter;

    Id selectedGroup;
    QColor ungroupedColour{Qt::red};

    QPointer<QPushButton> addGroup;
    QPointer<QPushButton> clearGroups;
    QPointer<QPushButton> goBack;
    QPointer<QPushButton> finishEditing;

    explicit Private(FilterManager* self_, FilterController* controller_, EditableLayout* editableLayout_)
        : self{self_}
        , controller{controller_}
        , editableLayout{editableLayout_}
    {
        ungroupedColour.setAlpha(20);
    }

    void enterGroupMode()
    {
        clearGroups->setText(tr("Clear Group"));

        addGroup->hide();
        finishEditing->hide();
        goBack->show();
    }

    void exitGroupMode()
    {
        selectedGroup = {};
        clearGroups->setText(tr("Clear Groups"));

        goBack->hide();
        addGroup->show();
        finishEditing->show();

        controlDialog->adjustSize();
    }

    void clearOverlays()
    {
        for(auto* overlay : overlays | std::views::values) {
            overlay->deleteLater();
        }
        overlays.clear();
    }

    void hideAndDeselectOverlay(FilterWidget* filter) const
    {
        const Id id = filter->id();
        if(overlays.contains(id)) {
            auto* overlay = overlays.at(id);
            overlay->button()->hide();
            overlay->deselect();
            overlay->setOption(OverlayWidget::Selectable, !controller->filterIsUngrouped(id));
        }
    }

    void updateDialog() const
    {
        addGroup->setEnabled(controller->haveUngroupedFilters());
        clearGroups->setEnabled(!controller->filterGroups().empty());
    }

    void addOrRemoveFilter(FilterWidget* widget, const QColor& colour) const
    {
        const Id id = widget->id();

        if(!overlays.contains(id)) {
            return;
        }

        auto* overlay = overlays.at(id);

        const Id groupId         = widget->group();
        const bool addingToGroup = controller->filterIsUngrouped(id);

        if(controller->removeFilter(widget)) {
            if(addingToGroup) {
                controller->addFilterToGroup(widget, selectedGroup);

                overlay->label()->setText(QString::number(widget->index() + 1));
                overlay->button()->setText(tr("Remove"));
                overlay->setColour(colour);
                overlay->select();

                if(const auto group = controller->groupById(selectedGroup)) {
                    for(FilterWidget* filter : group.value().filters) {
                        if(overlays.contains(filter->id())) {
                            overlays.at(filter->id())->connectOverlay(overlay);
                        }
                    }
                }
            }
            else {
                controller->addFilterToGroup(widget, {});
                overlay->label()->setText(tr("Ungrouped"));
                overlay->button()->setText(tr("Add"));
                overlay->setColour(ungroupedColour);
                overlay->setOption(OverlayWidget::Selectable, false);

                if(const auto group = controller->groupById(groupId)) {
                    for(FilterWidget* filter : group.value().filters) {
                        if(overlays.contains(filter->id())) {
                            auto* filterOverlay = overlays.at(filter->id());
                            overlay->disconnectOverlay(filterOverlay);
                            updateOverlayLabel(filter, filterOverlay);
                        }
                    }
                }
            }
        }

        updateDialog();
    }

    void setupOverlayButtons(const Id& group, const QColor& colour)
    {
        auto setupOverlayButtons = [this, &colour](const Id& id, FilterWidget* widget) {
            if(!overlays.contains(id)) {
                return;
            }

            OverlayWidget* overlay = overlays.at(id);

            overlay->button()->setText(tr(controller->filterIsUngrouped(id) ? "Add" : "Remove"));
            overlay->button()->show();

            overlay->button()->disconnect(self);
            QObject::connect(overlay->button(), &QPushButton::clicked, self,
                             [this, widget, colour]() { addOrRemoveFilter(widget, colour); });
        };

        if(group.isValid()) {
            const auto groups        = controller->filterGroups();
            const auto& groupWidgets = groups.at(group).filters;

            for(FilterWidget* widget : groupWidgets) {
                setupOverlayButtons(widget->id(), widget);
            }
        }

        const auto ungrouped = controller->ungroupedFilters();
        for(const auto& [id, widget] : ungrouped) {
            setupOverlayButtons(id, widget);
        }
    }

    OverlayWidget* setupWidgetOverlay(FilterWidget* widget, const QColor& colour)
    {
        const Id widgetId           = widget->id();
        constexpr auto overlayFlags = OverlayWidget::Label | OverlayWidget::Button | OverlayWidget::Resize;

        auto* overlay = overlays.emplace(widgetId, new OverlayWidget(overlayFlags, widget)).first->second;

        overlay->button()->hide();
        overlay->setColour(colour);

        if(widget->group().isValid()) {
            overlay->label()->setText(QString::number(widget->index() + 1));
            overlay->setOption(OverlayWidget::Selectable);
        }
        else {
            overlay->label()->setText(tr("Ungrouped"));
        }

        QObject::connect(overlay, &OverlayWidget::clicked, self, [this, widget, overlay]() {
            selectedGroup = widget->group();
            setupOverlayButtons(selectedGroup, overlay->colour());
            enterGroupMode();
        });

        overlay->resize(widget->size());
        overlay->show();

        return overlay;
    }

    void setupOverlays()
    {
        const auto groups = controller->filterGroups();

        for(const auto& group : groups | std::views::values) {
            if(!group.filters.empty()) {
                const QColor groupColour = generateRandomUniqueColor(group.id);
                std::vector<OverlayWidget*> groupOverlays;

                std::ranges::transform(
                    group.filters, std::back_inserter(groupOverlays),
                    [this, &groupColour](FilterWidget* widget) { return setupWidgetOverlay(widget, groupColour); });

                for(auto it1 = groupOverlays.begin(); it1 != groupOverlays.end(); ++it1) {
                    for(auto it2 = std::next(it1); it2 != groupOverlays.end(); ++it2) {
                        (*it1)->connectOverlay(*it2);
                    }
                }
            }
        }

        const auto ungrouped = controller->ungroupedFilters();

        std::ranges::for_each(ungrouped | std::views::values,
                              [this](FilterWidget* widget) { setupWidgetOverlay(widget, ungroupedColour); });
    }

    void createControlDialog()
    {
        controlDialog = new OverlayWidget(OverlayWidget::Static, editableLayout);

        auto* effect = new QGraphicsDropShadowEffect();
        effect->setBlurRadius(30);
        effect->setColor(Qt::black);
        effect->setOffset(1, 1);

        controlDialog->setGraphicsEffect(effect);

        addGroup = new QPushButton(tr("Add New Group"), controlDialog);
        controlDialog->addWidget(addGroup);
        QObject::connect(addGroup, &QPushButton::clicked, self, [this]() {
            const auto newGroup = Id{Utils::generateUniqueHash()};
            selectedGroup       = newGroup;
            setupOverlayButtons({}, generateRandomUniqueColor(newGroup));
            enterGroupMode();
        });

        clearGroups = new QPushButton(tr("Clear Groups"), controlDialog);
        controlDialog->addWidget(clearGroups);
        QObject::connect(clearGroups, &QPushButton::clicked, self, [this]() {
            const auto groups = controller->filterGroups();

            if(selectedGroup.isValid()) {
                const auto filters = groups.at(selectedGroup).filters;
                std::ranges::for_each(filters,
                                      [this](FilterWidget* widget) { addOrRemoveFilter(widget, ungroupedColour); });
            }
            else {
                for(const auto& group : groups | std::views::values) {
                    const auto filters = group.filters;
                    std::ranges::for_each(filters,
                                          [this](FilterWidget* widget) { addOrRemoveFilter(widget, ungroupedColour); });
                }
            }
        });

        goBack = new QPushButton(tr("Back"), controlDialog);
        controlDialog->addWidget(goBack);
        goBack->hide();
        QObject::connect(goBack, &QPushButton::clicked, self, [this]() {
            const auto groups = controller->filterGroups();
            if(selectedGroup.isValid() && groups.contains(selectedGroup)) {
                const auto& group = groups.at(selectedGroup);
                std::ranges::for_each(group.filters, [this](FilterWidget* filter) { hideAndDeselectOverlay(filter); });
            }

            const auto ungrouped = controller->ungroupedFilters();
            for(auto* filter : ungrouped | std::views::values) {
                hideAndDeselectOverlay(filter);
            }
            exitGroupMode();
        });

        finishEditing = new QPushButton(tr("Finish"), controlDialog);
        controlDialog->addWidget(finishEditing);
        QObject::connect(finishEditing, &QPushButton::clicked, self, [this]() {
            widgetFilter->stop();
            widgetFilter->deleteLater();
            controlDialog->deleteLater();
            clearOverlays();
        });

        QObject::connect(widgetFilter, &WidgetFilter::filterFinished, self, [this]() {
            if(goBack && !goBack->isHidden()) {
                QMetaObject::invokeMethod(goBack, "clicked", Q_ARG(bool, false));
            }
            else {
                QMetaObject::invokeMethod(finishEditing, "clicked", Q_ARG(bool, false));
            }
        });

        controlDialog->move(editableLayout->width() - 160, editableLayout->height() - 160);
    }
};

FilterManager::FilterManager(FilterController* controller, EditableLayout* editableLayout, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, controller, editableLayout)}
{ }

FilterManager::~FilterManager() = default;

void FilterManager::setupWidgetConnections()
{
    p->widgetFilter = new WidgetFilter(this);
    p->widgetFilter->start();

    p->clearOverlays();
    p->setupOverlays();

    p->createControlDialog();
    p->updateDialog();

    p->controlDialog->show();
}
} // namespace Fooyin::Filters

#include "moc_filtermanager.cpp"
