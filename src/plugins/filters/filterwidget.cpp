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

#include "filterwidget.h"

#include "filterdelegate.h"
#include "filteritem.h"
#include "filtermanager.h"
#include "filtermodel.h"
#include "filterview.h"
#include "settings/filtersettings.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/player/playermanager.h>

#include <utils/actions/actioncontainer.h>
#include <utils/async.h>
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>

namespace Fy::Filters {
struct FilterWidget::Private : QObject
{
    FilterWidget* widget;

    FilterManager* manager;
    Utils::SettingsManager* settings;

    LibraryFilter* filter;
    FilterView* view;
    FilterModel* model;

    Private(FilterWidget* widget, FilterManager* manager, Utils::SettingsManager* settings)
        : widget{widget}
        , manager{manager}
        , settings{settings}
        , filter{manager->registerFilter("")}
        , view{new FilterView(widget)}
        , model{new FilterModel(&filter->field, widget)}
    {
        connect(view->header(), &QHeaderView::sectionClicked, this, &FilterWidget::Private::changeOrder);

        connect(manager, &FilterManager::filteredItems, this, &FilterWidget::Private::resetByIndex);
        connect(manager, &FilterManager::filterChanged, this, &FilterWidget::Private::editFilter);
        connect(manager, &FilterManager::fieldChanged, this, &FilterWidget::Private::fieldChanged);
    }

    void selectionChanged()
    {
        filter->tracks.clear();

        const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
        if(indexes.isEmpty()) {
            return;
        }

        Core::TrackList tracks;
        for(const auto& index : indexes) {
            const bool isAllNode = index.data(FilterItemRole::AllNode).toBool();
            if(isAllNode) {
                tracks = index.data(FilterItemRole::Tracks).value<Core::TrackList>();
                break;
            }
            const auto newTracks = index.data(FilterItemRole::Tracks).value<Core::TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(tracks));
        }

        const auto sortedTracks = Utils::asyncExec<Core::TrackList>([&tracks]() {
            return Core::Library::Sorting::sortTracks(tracks);
        });

        filter->tracks = sortedTracks;
        manager->selectionChanged(filter->index);
    }

    void fieldChanged(const FilterField& field)
    {
        if(filter->field.id == field.id) {
            filter->field = field;
            emit widget->typeChanged(filter->index);
            model->reload(manager->tracks());
            model->sortFilter();
        }
    }

    void editFilter(int index, const QString& name)
    {
        if(filter->index == index) {
            widget->setField(name);
        }
    }

    void changeOrder()
    {
        const auto sortOrder = model->sortOrder();
        if(sortOrder == Qt::AscendingOrder) {
            model->setSortOrder(Qt::DescendingOrder);
        }
        else {
            model->setSortOrder(Qt::AscendingOrder);
        }
        model->sortFilter();
    }

    void resetByIndex(int idx)
    {
        if(idx < filter->index) {
            model->reload(manager->tracks());
            model->sortFilter();
        }
    }

    void resetByType()
    {
        model->reload(manager->tracks());
        model->sortFilter();
    }
};

FilterWidget::FilterWidget(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, manager, settings)}
{
    setObjectName(FilterWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    p->view->setModel(p->model);
    p->view->setItemDelegate(new FilterDelegate(this));

    layout->addWidget(p->view);

    setupConnections();
    setHeaderEnabled(p->settings->value<Settings::FilterHeader>());
    setScrollbarEnabled(p->settings->value<Settings::FilterScrollBar>());
    setAltColors(p->settings->value<Settings::FilterAltColours>());

    p->model->setRowHeight(p->settings->value<Settings::FilterRowHeight>());
    p->model->setFontSize(p->settings->value<Settings::FilterFontSize>());

    p->resetByType();
}

FilterWidget::~FilterWidget()
{
    p->manager->unregisterFilter(p->filter->index);
}

void FilterWidget::setupConnections()
{
    p->settings->subscribe<Settings::FilterAltColours>(this, &FilterWidget::setAltColors);
    p->settings->subscribe<Settings::FilterHeader>(this, &FilterWidget::setHeaderEnabled);
    p->settings->subscribe<Settings::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);
    p->settings->subscribe<Settings::FilterRowHeight>(p->model, &FilterModel::setRowHeight);
    p->settings->subscribe<Settings::FilterFontSize>(p->model, &FilterModel::setFontSize);

    connect(p->view->header(), &FilterView::customContextMenuRequested, this, &FilterWidget::customHeaderMenuRequested);
    connect(p->view, &QTreeView::doubleClicked, this, []() {
        //        p->library->prepareTracks();
    });
    connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, p.get(),
            &FilterWidget::Private::selectionChanged);
    connect(this, &FilterWidget::typeChanged, p->manager, &FilterManager::changeFilter);

    connect(p->manager, &FilterManager::tracksAdded, p->model, &FilterModel::addTracks);
    connect(p->manager, &FilterManager::tracksUpdated, p->model, &FilterModel::updateTracks);
    connect(p->manager, &FilterManager::tracksRemoved, p->model, &FilterModel::removeTracks);
}

void FilterWidget::setField(const QString& name)
{
    p->filter->field = p->manager->findField(name);
    p->model->setField(&p->filter->field);
    emit typeChanged(p->filter->index);
    p->view->clearSelection();
    p->view->scrollToTop();
}

bool FilterWidget::isHeaderEnabled()
{
    return !p->view->isHeaderHidden();
}

void FilterWidget::setHeaderEnabled(bool enabled)
{
    p->view->setHeaderHidden(!enabled);
}

bool FilterWidget::isScrollbarEnabled()
{
    return p->view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    p->view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

bool FilterWidget::hasAltColors()
{
    return p->view->alternatingRowColors();
}

void FilterWidget::setAltColors(bool enabled)
{
    p->view->setAlternatingRowColors(enabled);
}

QString FilterWidget::name() const
{
    return "Filter";
}

void FilterWidget::customHeaderMenuRequested(QPoint pos)
{
    if(!p->filter) {
        return;
    }
    auto* menu = p->manager->filterHeaderMenu(p->filter->index, &p->filter->field);

    if(!menu) {
        return;
    }

    menu->popup(mapToGlobal(pos));
}

void FilterWidget::saveLayout(QJsonArray& array)
{
    if(!p->filter) {
        return;
    }
    QJsonObject options;
    options["Type"] = p->filter->field.name;
    options["Sort"] = Utils::EnumHelper::toString(p->model->sortOrder());

    QJsonObject filter;
    filter[name()] = options;
    array.append(filter);
}

void FilterWidget::loadLayout(const QJsonObject& object)
{
    if(auto order = Utils::EnumHelper::fromString<Qt::SortOrder>(object["Sort"].toString())) {
        p->model->setSortOrder(order.value());
    }
    setField(object["Type"].toString());
}
} // namespace Fy::Filters
