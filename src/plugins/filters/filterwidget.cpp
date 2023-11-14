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
#include "filterfwd.h"
#include "filteritem.h"
#include "filtermodel.h"
#include "filterview.h"
#include "settings/filtersettings.h"

#include <core/player/playermanager.h>
#include <utils/actions/actioncontainer.h>
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace {
Fy::Core::TrackList fetchAllTracks(QTreeView* view)
{
    std::set<int> ids;
    Fy::Core::TrackList tracks;

    const int rowCount = view->model()->rowCount({});
    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex index = view->model()->index(row, 0, {});
        const auto indexTracks  = index.data(Fy::Filters::FilterItemRole::Tracks).value<Fy::Core::TrackList>();
        for(const Fy::Core::Track& track : indexTracks) {
            const int id = track.id();
            if(!ids.contains(id)) {
                ids.emplace(id);
                tracks.push_back(track);
            }
        }
    }
    return tracks;
}
} // namespace

namespace Fy::Filters {
struct FilterWidget::Private
{
    FilterWidget* self;

    Utils::SettingsManager* settings;

    LibraryFilter filter;
    FilterView* view;
    FilterModel* model;

    Private(FilterWidget* self, Utils::SettingsManager* settings)
        : self{self}
        , settings{settings}
        , view{new FilterView(self)}
        , model{new FilterModel(filter.field, self)}
    { }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QString title;
        const QModelIndexList selectedIndexes = view->selectionModel()->selectedIndexes();
        for(const auto& index : selectedIndexes) {
            if(!title.isEmpty()) {
                title.append(", ");
            }
            title.append(index.data().toString());
        }
        return title;
    }

    void selectionChanged()
    {
        filter.tracks.clear();

        const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
        if(indexes.isEmpty()) {
            return;
        }

        Core::TrackList tracks;
        for(const auto& index : indexes) {
            const bool isAllNode = index.data(FilterItemRole::AllNode).toBool();
            if(isAllNode) {
                tracks = fetchAllTracks(view);
                break;
            }
            const auto newTracks = index.data(FilterItemRole::Tracks).value<Core::TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(tracks));
        }

        filter.tracks = tracks;
        QMetaObject::invokeMethod(self, "selectionChanged", Q_ARG(LibraryFilter, filter),
                                  Q_ARG(QString, playlistNameFromSelection()));
    }

    void changeOrder() const
    {
        const auto sortOrder = model->sortOrder();
        if(sortOrder == Qt::AscendingOrder) {
            model->setSortOrder(Qt::DescendingOrder);
        }
        else {
            model->setSortOrder(Qt::AscendingOrder);
        }
    }

    void updateAppearance(const QVariant& optionsVar) const
    {
        const auto options = optionsVar.value<FilterOptions>();
        model->setAppearance(options);
        QMetaObject::invokeMethod(view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    }
};

FilterWidget::FilterWidget(Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    setObjectName(FilterWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    p->view->setModel(p->model);
    p->view->setItemDelegate(new FilterDelegate(this));

    layout->addWidget(p->view);

    p->view->setHeaderHidden(!p->settings->value<Settings::FilterHeader>());
    setScrollbarEnabled(p->settings->value<Settings::FilterScrollBar>());
    p->view->setAlternatingRowColors(p->settings->value<Settings::FilterAltColours>());

    p->updateAppearance(p->settings->value<Settings::FilterAppearance>());

    p->settings->subscribe<Settings::FilterAltColours>(p->view, &QAbstractItemView::setAlternatingRowColors);
    p->settings->subscribe<Settings::FilterHeader>(this, [this](bool enabled) { p->view->setHeaderHidden(!enabled); });
    p->settings->subscribe<Settings::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);
    p->settings->subscribe<Settings::FilterAppearance>(
        this, [this](const QVariant& appearance) { p->updateAppearance(appearance); });

    QObject::connect(p->view->header(), &QHeaderView::sectionClicked, this, [this]() { p->changeOrder(); });
    QObject::connect(p->view->header(), &FilterView::customContextMenuRequested, this,
                     &FilterWidget::customHeaderMenuRequested);
    QObject::connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });

    QObject::connect(p->view, &FilterView::doubleClicked, this,
                     [this]() { emit doubleClicked(p->playlistNameFromSelection()); });
    QObject::connect(p->view, &FilterView::middleClicked, this,
                     [this]() { emit middleClicked(p->playlistNameFromSelection()); });
}

FilterWidget::~FilterWidget()
{
    emit filterDeleted(p->filter);
}

void FilterWidget::changeFilter(const LibraryFilter& filter)
{
    p->filter = filter;
}

void FilterWidget::reset(const Core::TrackList& tracks)
{
    p->model->reset(p->filter.field, tracks);
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    p->view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

QString FilterWidget::name() const
{
    return u"Filter"_s;
}

void FilterWidget::customHeaderMenuRequested(QPoint pos)
{
    emit requestHeaderMenu(p->filter, mapToGlobal(pos));
}

void FilterWidget::saveLayout(QJsonArray& array)
{
    QJsonObject options;
    options["Type"_L1] = p->filter.field.name;
    options["Sort"_L1] = Utils::EnumHelper::toString(p->model->sortOrder());

    QJsonObject filter;
    filter[name()] = options;
    array.append(filter);
}

void FilterWidget::loadLayout(const QJsonObject& object)
{
    if(auto order = Utils::EnumHelper::fromString<Qt::SortOrder>(object["Sort"_L1].toString())) {
        p->model->setSortOrder(order.value());
    }
    emit requestFieldChange(p->filter, object["Type"_L1].toString());
}

void FilterWidget::tracksAdded(const Core::TrackList& tracks)
{
    p->model->addTracks(tracks);
}

void FilterWidget::tracksUpdated(const Core::TrackList& tracks)
{
    p->model->updateTracks(tracks);
}

void FilterWidget::tracksRemoved(const Core::TrackList& tracks)
{
    p->model->removeTracks(tracks);
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit requestContextMenu(p->filter, mapToGlobal(event->pos()));
}
} // namespace Fy::Filters

#include "moc_filterwidget.cpp"
