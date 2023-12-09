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

#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/autoheaderview.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace {
Fooyin::TrackList fetchAllTracks(QTreeView* view)
{
    std::set<int> ids;
    Fooyin::TrackList tracks;

    const QModelIndex parent = view->model()->index(0, 0, {});
    const int rowCount       = view->model()->rowCount(parent);

    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex index = view->model()->index(row, 0, parent);
        const auto indexTracks  = index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>();

        for(const Fooyin::Track& track : indexTracks) {
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

namespace Fooyin::Filters {
struct FilterWidget::Private
{
    FilterWidget* self;

    SettingsManager* settings;

    LibraryFilter filter;
    FilterView* view;
    FilterModel* model;

    AutoHeaderView* header;

    Private(FilterWidget* self, SettingsManager* settings)
        : self{self}
        , settings{settings}
        , view{new FilterView(self)}
        , model{new FilterModel(self)}
        , header{new AutoHeaderView(Qt::Horizontal, self)}
    {
        view->setHeader(header);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        header->setStretchEnabled(true);
        header->setSortIndicatorShown(true);
        header->setSectionsMovable(true);
        header->setFirstSectionMovable(true);
        header->setSectionsClickable(true);
        header->setContextMenuPolicy(Qt::CustomContextMenu);

        header->restoreHeaderState({});
    }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QStringList titles;
        const QModelIndexList selectedIndexes = view->selectionModel()->selectedIndexes();
        std::ranges::transform(selectedIndexes, std::back_inserter(titles),
                               [](const QModelIndex& index) { return index.data().toString(); });
        return titles.join(", "_L1);
    }

    void selectionChanged()
    {
        filter.tracks.clear();

        const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
        QModelIndexList selectedRows;
        for(const QModelIndex& index : indexes) {
            if(index.column() == 0) {
                selectedRows.push_back(index);
            }
        }

        if(selectedRows.empty()) {
            return;
        }

        TrackList tracks;
        for(const auto& index : selectedRows) {
            if(!index.parent().isValid()) {
                tracks = fetchAllTracks(view);
                break;
            }
            const auto newTracks = index.data(FilterItem::Tracks).value<TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(tracks));
        }

        filter.tracks = tracks;
        QMetaObject::invokeMethod(self, "selectionChanged", Q_ARG(LibraryFilter, filter),
                                  Q_ARG(QString, playlistNameFromSelection()));
    }

    void updateAppearance(const QVariant& optionsVar) const
    {
        const auto options = optionsVar.value<FilterOptions>();
        model->setAppearance(options);
        QMetaObject::invokeMethod(view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    }

    void hideHeader(bool hide) const
    {
        if(hide) {
            header->setFixedHeight(0);
        }
        else if(header->sizeHint().height() > 0) {
            header->setFixedHeight(header->sizeHint().height());
        }
    }

    void customHeaderMenuRequested(const QPoint& pos) const
    {
        QMetaObject::invokeMethod(self, "requestHeaderMenu", Q_ARG(const LibraryFilter&, filter),
                                  Q_ARG(AutoHeaderView*, header), Q_ARG(const QPoint&, self->mapToGlobal(pos)));
    }
};

FilterWidget::FilterWidget(SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    setObjectName(FilterWidget::name());

    setFeature(FyWidget::Search);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    p->view->setModel(p->model);
    p->view->setItemDelegate(new FilterDelegate(this));

    layout->addWidget(p->view);

    p->hideHeader(!p->settings->value<Settings::Filters::FilterHeader>());
    setScrollbarEnabled(p->settings->value<Settings::Filters::FilterScrollBar>());
    p->view->setAlternatingRowColors(p->settings->value<Settings::Filters::FilterAltColours>());

    p->updateAppearance(p->settings->value<Settings::Filters::FilterAppearance>());

    p->settings->subscribe<Settings::Filters::FilterAltColours>(p->view, &QAbstractItemView::setAlternatingRowColors);
    p->settings->subscribe<Settings::Filters::FilterHeader>(this, [this](bool enabled) { p->hideHeader(!enabled); });
    p->settings->subscribe<Settings::Filters::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);
    p->settings->subscribe<Settings::Filters::FilterAppearance>(
        this, [this](const QVariant& appearance) { p->updateAppearance(appearance); });

    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() { p->view->expandAll(); });
    QObject::connect(p->header, &QHeaderView::sortIndicatorChanged, p->model, &FilterModel::sortOnColumn);
    QObject::connect(p->header, &FilterView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->customHeaderMenuRequested(pos); });
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

void FilterWidget::reset(const TrackList& tracks)
{
    p->model->reset(p->filter.columns, tracks);
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    p->view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

QString FilterWidget::name() const
{
    return u"Library Filter"_s;
}

QString FilterWidget::layoutName() const
{
    return u"LibraryFilter"_s;
}

void FilterWidget::saveLayoutData(QJsonObject& layout)
{
    QStringList columns;
    std::ranges::transform(p->filter.columns, std::back_inserter(columns),
                           [](const auto& column) { return QString::number(column.id); });

    QByteArray state = p->header->saveHeaderState();
    state            = qCompress(state, 9);

    layout["Columns"_L1] = columns.join("|"_L1);
    layout["State"_L1]   = QString::fromUtf8(state.toBase64());
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Columns"_L1)) {
        const QString columnNames = layout["Columns"_L1].toString();
        const QStringList columns = columnNames.split("|"_L1);
        ColumnIds ids;
        std::ranges::transform(columns, std::back_inserter(ids), [](const QString& column) { return column.toInt(); });

        emit requestColumnsChange(p->filter, ids);
    }

    if(layout.contains("State"_L1)) {
        auto state = QByteArray::fromBase64(layout["State"_L1].toString().toUtf8());

        if(state.isEmpty()) {
            return;
        }

        state = qUncompress(state);

        // Workaround to ensure QHeaderView section count is updated before restoring state
        QMetaObject::invokeMethod(p->model, "headerDataChanged", Q_ARG(Qt::Orientation, Qt::Horizontal), Q_ARG(int, 0),
                                  Q_ARG(int, 0));

        p->header->restoreHeaderState(state);
    }
}

void FilterWidget::finalise()
{
    p->model->sortOnColumn(p->header->sortIndicatorSection(), p->header->sortIndicatorOrder());
}

void FilterWidget::searchEvent(const QString& search)
{
    emit requestSearch(p->filter, search);
}

void FilterWidget::tracksAdded(const TrackList& tracks)
{
    p->model->addTracks(tracks);
}

void FilterWidget::tracksUpdated(const TrackList& tracks)
{
    p->model->updateTracks(tracks);
}

void FilterWidget::tracksRemoved(const TrackList& tracks)
{
    p->model->removeTracks(tracks);
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit requestContextMenu(p->filter, mapToGlobal(event->pos()));
}
} // namespace Fooyin::Filters

#include "moc_filterwidget.cpp"
