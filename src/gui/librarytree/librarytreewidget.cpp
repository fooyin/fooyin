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

#include "librarytreewidget.h"

#include "librarytreeappearance.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreesettings.h"
#include "librarytreeview.h"

#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <core/library/tracksort.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <QCoro/QCoroCore>

using namespace Qt::Literals::StringLiterals;

namespace {
void getLowestIndexes(const QTreeView* treeView, const QModelIndex& index, QModelIndexList& bottomIndexes)
{
    if(!index.isValid()) {
        getLowestIndexes(treeView, treeView->rootIndex(), bottomIndexes);
        return;
    }

    const int rowCount = treeView->model()->rowCount(index);
    if(rowCount == 0) {
        bottomIndexes.append(index);
        return;
    }

    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = treeView->model()->index(row, 0, index);
        getLowestIndexes(treeView, childIndex, bottomIndexes);
    }
}
} // namespace

namespace Fooyin {
class LibraryTreeWidgetPrivate
{
public:
    LibraryTreeWidgetPrivate(LibraryTreeWidget* self, MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
                             TrackSelectionController* trackSelection, SettingsManager* settings);

    void reset() const;

    void changeGrouping(const LibraryTreeGrouping& newGrouping);
    void addGroupMenu(QMenu* parent);

    void setScrollbarEnabled(bool enabled) const;
    void updateAppearance(const QVariant& optionsVar) const;

    void setupHeaderContextMenu(const QPoint& pos);

    QCoro::Task<void> selectionChanged() const;
    QCoro::Task<void> searchChanged(QString search);
    [[nodiscard]] QString playlistNameFromSelection() const;

    void handleDoubleClick() const;
    void handleMiddleClick() const;

    LibraryTreeWidget* self;

    MusicLibrary* library;
    LibraryTreeGroupRegistry* groupsRegistry;
    TrackSelectionController* trackSelection;
    SettingsManager* settings;

    LibraryTreeGrouping grouping;

    QVBoxLayout* layout;
    LibraryTreeView* libraryTree;
    LibraryTreeModel* model;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    QString prevSearch;
    TrackList prevSearchTracks;
};

LibraryTreeWidgetPrivate::LibraryTreeWidgetPrivate(LibraryTreeWidget* self, MusicLibrary* library,
                                                   LibraryTreeGroupRegistry* groupsRegistry,
                                                   TrackSelectionController* trackSelection, SettingsManager* settings)
    : self{self}
    , library{library}
    , groupsRegistry{groupsRegistry}
    , trackSelection{trackSelection}
    , settings{settings}
    , layout{new QVBoxLayout(self)}
    , libraryTree{new LibraryTreeView(self)}
    , model{new LibraryTreeModel(self)}
    , doubleClickAction{static_cast<TrackAction>(settings->value(LibraryTreeDoubleClick).toInt())}
    , middleClickAction{static_cast<TrackAction>(settings->value(LibraryTreeMiddleClick).toInt())}
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(libraryTree);

    libraryTree->setModel(model);

    libraryTree->setExpandsOnDoubleClick(doubleClickAction == TrackAction::Expand);
    libraryTree->setHeaderHidden(!settings->value(LibraryTreeHeader).toBool());
    setScrollbarEnabled(settings->value(LibraryTreeScrollBar).toBool());
    libraryTree->setAlternatingRowColors(settings->value(LibraryTreeAltColours).toBool());

    changeGrouping(groupsRegistry->itemByName(u""_s));

    if(!library->isEmpty()) {
        reset();
    }

    updateAppearance(settings->value(LibraryTreeAppearanceOptions));
}

void LibraryTreeWidgetPrivate::reset() const
{
    model->reset(library->tracks());
}

void LibraryTreeWidgetPrivate::changeGrouping(const LibraryTreeGrouping& newGrouping)
{
    if(std::exchange(grouping, newGrouping) != newGrouping) {
        model->changeGrouping(grouping);
        reset();
    }
}

void LibraryTreeWidgetPrivate::addGroupMenu(QMenu* parent)
{
    auto* groupMenu = new QMenu(u"Grouping"_s, parent);

    auto* treeGroups = new QActionGroup(groupMenu);

    const auto& groups = groupsRegistry->items();
    for(const auto& [_, group] : groups) {
        auto* switchGroup = new QAction(group.name, groupMenu);
        QObject::connect(switchGroup, &QAction::triggered, self, [this, group]() { changeGrouping(group); });
        switchGroup->setCheckable(true);
        switchGroup->setChecked(grouping.id == group.id);
        groupMenu->addAction(switchGroup);
        treeGroups->addAction(switchGroup);
    }

    parent->addMenu(groupMenu);
}

void LibraryTreeWidgetPrivate::setScrollbarEnabled(bool enabled) const
{
    libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void LibraryTreeWidgetPrivate::updateAppearance(const QVariant& optionsVar) const
{
    const auto options = optionsVar.value<LibraryTreeAppearance>();
    model->setAppearance(options);
    QMetaObject::invokeMethod(libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

void LibraryTreeWidgetPrivate::setupHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addGroupMenu(menu);
    menu->popup(self->mapToGlobal(pos));
}

QCoro::Task<void> LibraryTreeWidgetPrivate::selectionChanged() const
{
    const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        co_return;
    }

    QModelIndexList trackIndexes;
    for(const QModelIndex& index : selectedIndexes) {
        getLowestIndexes(libraryTree, index, trackIndexes);
    }

    TrackList tracks;
    for(const auto& index : trackIndexes) {
        const int level = index.data(LibraryTreeItem::Level).toInt();
        if(level < 0) {
            tracks.clear();
            tracks = library->tracks();
            break;
        }
        const auto indexTracks = index.data(LibraryTreeItem::Tracks).value<TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    const auto sortedTracks = co_await Utils::asyncExec([&tracks]() { return Sorting::sortTracks(tracks); });
    trackSelection->changeSelectedTracks(sortedTracks, playlistNameFromSelection());

    if(settings->value(LibraryTreePlaylistEnabled).toBool()) {
        const QString playlistName = settings->value(LibraryTreeAutoPlaylist).toString();
        const bool autoSwitch      = settings->value(LibraryTreeAutoSwitch).toBool();

        trackSelection->executeAction(TrackAction::SendNewPlaylist,
                                      autoSwitch ? PlaylistAction::Switch : PlaylistAction::None, playlistName);
    }
    co_return;
}

QCoro::Task<void> LibraryTreeWidgetPrivate::searchChanged(QString search)
{
    const bool reset = prevSearch.length() > search.length();
    prevSearch       = search;

    if(search.isEmpty()) {
        prevSearchTracks.clear();
        model->reset(library->tracks());
        co_return;
    }

    TrackList tracksToFilter = !reset && !prevSearchTracks.empty() ? prevSearchTracks : library->tracks();

    const auto tracks = co_await Utils::asyncExec(
        [search, tracksToFilter]() { return Filter::filterTracks(tracksToFilter, search); });

    prevSearchTracks = tracks;
    model->reset(tracks);
}

QString LibraryTreeWidgetPrivate::playlistNameFromSelection() const
{
    QString title;
    const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
    for(const auto& index : selectedIndexes) {
        if(!title.isEmpty()) {
            title.append(", ");
        }
        title.append(index.data().toString());
    }
    return title;
}

void LibraryTreeWidgetPrivate::handleDoubleClick() const
{
    const bool autoSwitch = settings->value(LibraryTreeAutoSwitch).toBool();
    trackSelection->executeAction(doubleClickAction, autoSwitch ? PlaylistAction::Switch : PlaylistAction::None,
                                  playlistNameFromSelection());
}

void LibraryTreeWidgetPrivate::handleMiddleClick() const
{
    const bool autoSwitch = settings->value(LibraryTreeAutoSwitch).toBool();
    trackSelection->executeAction(middleClickAction, autoSwitch ? PlaylistAction::Switch : PlaylistAction::None,
                                  playlistNameFromSelection());
}

LibraryTreeWidget::LibraryTreeWidget(MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
                                     TrackSelectionController* trackSelection, SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<LibraryTreeWidgetPrivate>(this, library, groupsRegistry, trackSelection, settings)}
{
    setObjectName(LibraryTreeWidget::name());

    setFeature(FyWidget::Search);

    QObject::connect(p->libraryTree, &LibraryTreeView::doubleClicked, this, [this]() { p->handleDoubleClick(); });
    QObject::connect(p->libraryTree, &LibraryTreeView::middleMouseClicked, this, [this]() { p->handleMiddleClick(); });
    QObject::connect(p->libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });
    QObject::connect(p->libraryTree->header(), &QHeaderView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->setupHeaderContextMenu(pos); });
    QObject::connect(groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, this,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(p->grouping.id == changedGrouping.id) {
                             p->changeGrouping(changedGrouping);
                         }
                     });

    QObject::connect(library, &MusicLibrary::tracksLoaded, this, [this]() { p->reset(); });
    QObject::connect(library, &MusicLibrary::tracksAdded, p->model, &LibraryTreeModel::addTracks);
    QObject::connect(library, &MusicLibrary::tracksScanned, p->model, &LibraryTreeModel::addTracks);
    QObject::connect(library, &MusicLibrary::tracksUpdated, p->model, &LibraryTreeModel::updateTracks);
    QObject::connect(library, &MusicLibrary::tracksDeleted, p->model, &LibraryTreeModel::removeTracks);
    QObject::connect(library, &MusicLibrary::tracksSorted, this, [this]() { p->reset(); });
    QObject::connect(library, &MusicLibrary::libraryRemoved, this, [this]() { p->reset(); });
    QObject::connect(library, &MusicLibrary::libraryChanged, this, [this]() { p->reset(); });

    settings->subscribe(LibraryTreeDoubleClick, this, [this](const QVariant& action) {
        p->doubleClickAction = static_cast<TrackAction>(action.toInt());
        p->libraryTree->setExpandsOnDoubleClick(p->doubleClickAction == TrackAction::Expand);
    });
    settings->subscribe(LibraryTreeMiddleClick, this, [this](const QVariant& action) {
        p->middleClickAction = static_cast<TrackAction>(action.toInt());
    });
    settings->subscribe(LibraryTreeHeader, this,
                        [this](const QVariant& show) { p->libraryTree->setHeaderHidden(!show.toBool()); });
    settings->subscribe(LibraryTreeScrollBar, this,
                        [this](const QVariant& show) { p->setScrollbarEnabled(show.toBool()); });
    settings->subscribe(LibraryTreeAltColours, this,
                        [this](const QVariant& enable) { p->libraryTree->setAlternatingRowColors(enable.toBool()); });
    settings->subscribe(LibraryTreeAppearanceOptions, this, [this](const QVariant& var) { p->updateAppearance(var); });
}

QString LibraryTreeWidget::name() const
{
    return u"Library Tree"_s;
}

QString LibraryTreeWidget::layoutName() const
{
    return u"LibraryTree"_s;
}

void LibraryTreeWidget::saveLayoutData(QJsonObject& layout)
{
    layout["Grouping"_L1] = p->grouping.name;
}

void LibraryTreeWidget::loadLayoutData(const QJsonObject& layout)
{
    const LibraryTreeGrouping grouping = p->groupsRegistry->itemByName(layout["Grouping"_L1].toString());
    if(grouping.isValid()) {
        p->changeGrouping(grouping);
    }
}

void LibraryTreeWidget::searchEvent(const QString& search)
{
    p->searchChanged(search);
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    p->trackSelection->addTrackPlaylistContextMenu(menu);
    p->addGroupMenu(menu);
    p->trackSelection->addTrackContextMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}
} // namespace Fooyin

#include "moc_librarytreewidget.cpp"
