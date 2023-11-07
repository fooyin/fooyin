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

#include "playlistwidget.h"

#include "gui/guiconstants.h"
#include "gui/trackselectioncontroller.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistmodel.h"
#include "playlistview.h"
#include "playlistwidget_p.h"
#include "presetregistry.h"

#include <core/library/sortingregistry.h>
#include <core/library/tracksort.h>
#include <core/playlist/playlistmanager.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/async.h>
#include <utils/headerview.h>
#include <utils/settings/settingsdialogcontroller.h>

#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>

#include <QCoro/QCoroCore>

#include <stack>

namespace {
void expandTree(QTreeView* view, QAbstractItemModel* model, const QModelIndex& parent, int first, int last)
{
    while(first <= last) {
        const QModelIndex child = model->index(first, 0, parent);
        view->expand(child);
        ++first;
    }
}

Fy::Core::TrackList getAllTracks(QAbstractItemModel* model, const QModelIndexList& indexes)
{
    Fy::Core::TrackList tracks;

    std::stack<QModelIndex> indexStack;
    for(const QModelIndex& index : indexes | std::views::reverse) {
        indexStack.push(index);
    }

    while(!indexStack.empty()) {
        const QModelIndex currentIndex = indexStack.top();
        indexStack.pop();

        while(model->canFetchMore(currentIndex)) {
            model->fetchMore(currentIndex);
        }

        const int rowCount = model->rowCount(currentIndex);
        if(rowCount == 0) {
            tracks.push_back(
                currentIndex.data(Fy::Gui::Widgets::Playlist::PlaylistItem::Role::ItemData).value<Fy::Core::Track>());
        }
        else {
            for(int row{rowCount - 1}; row >= 0; --row) {
                const QModelIndex childIndex = model->index(row, 0, currentIndex);
                indexStack.push(childIndex);
            }
        }
    }
    return tracks;
}
} // namespace

namespace Fy::Gui::Widgets::Playlist {
PlaylistWidgetPrivate::PlaylistWidgetPrivate(PlaylistWidget* self, Utils::ActionManager* actionManager,
                                             PlaylistController* playlistController,
                                             TrackSelectionController* selectionController,
                                             Utils::SettingsManager* settings)
    : self{self}
    , actionManager{actionManager}
    , selectionController{selectionController}
    , settings{settings}
    , settingsDialog{settings->settingsDialog()}
    , playlistController{playlistController}
    , layout{new QHBoxLayout(self)}
    , model{new PlaylistModel(settings, self)}
    , playlistView{new PlaylistView(self)}
    , header{new Utils::HeaderView(Qt::Horizontal, self)}
    , playlistContext{new Utils::WidgetContext(self, Utils::Context{Constants::Context::Playlist}, self)}
{
    layout->setContentsMargins(0, 0, 0, 0);

    header->setStretchLastSection(true);
    playlistView->setHeader(header);
    header->setContextMenuPolicy(Qt::CustomContextMenu);

    playlistView->setModel(model);
    playlistView->setItemDelegate(new PlaylistDelegate(self));

    layout->addWidget(playlistView);

    setHeaderHidden(settings->value<Settings::PlaylistHeader>());
    setScrollbarHidden(settings->value<Settings::PlaylistScrollBar>());

    changePreset(playlistController->presetRegistry()->itemByName(settings->value<Settings::CurrentPreset>()));

    setupConnections();
    setupActions();

    model->setCurrentPlaylistIsActive(playlistController->currentIsActive());
}

void PlaylistWidgetPrivate::setupConnections()
{
    QObject::connect(playlistView->header(), &QHeaderView::customContextMenuRequested, this,
                     &PlaylistWidgetPrivate::customHeaderMenuRequested);
    QObject::connect(playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PlaylistWidgetPrivate::selectionChanged);
    QObject::connect(playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidgetPrivate::doubleClicked);

    QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, playlistView, &QAbstractItemView::clearSelection);
    QObject::connect(model, &PlaylistModel::tracksChanged, this, &PlaylistWidgetPrivate::playlistTracksChanged);
    QObject::connect(model, &QAbstractItemModel::modelReset, this, &PlaylistWidgetPrivate::resetTree);
    QObject::connect(model, &QAbstractItemModel::rowsInserted, this,
                     [this](const QModelIndex& parent, int first, int last) {
                         expandTree(playlistView, model, parent, first, last);
                     });

    QObject::connect(header, &Utils::HeaderView::leftClicked, this, &PlaylistWidgetPrivate::switchContextMenu);

    QObject::connect(playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &PlaylistWidgetPrivate::changePlaylist);
    QObject::connect(playlistController, &PlaylistController::currentTrackChanged, model,
                     &PlaylistModel::currentTrackChanged);
    QObject::connect(playlistController, &PlaylistController::playStateChanged, model,
                     &PlaylistModel::playStateChanged);
    QObject::connect(playlistController->playlistHandler(), &Core::Playlist::PlaylistManager::activeTrackChanged, this,
                     &PlaylistWidgetPrivate::followCurrentTrack);
    QObject::connect(playlistController->presetRegistry(), &PresetRegistry::presetChanged, this,
                     &PlaylistWidgetPrivate::onPresetChanged);

    settings->subscribe<Settings::PlaylistHeader>(this, &PlaylistWidgetPrivate::setHeaderHidden);
    settings->subscribe<Settings::PlaylistScrollBar>(this, &PlaylistWidgetPrivate::setScrollbarHidden);
    settings->subscribe<Settings::CurrentPreset>(this, [this](const QString& presetName) {
        const auto preset = playlistController->presetRegistry()->itemByName(presetName);
        changePreset(preset);
    });
}

void PlaylistWidgetPrivate::setupActions()
{
    actionManager->addContextObject(playlistContext);

    auto* editMenu = actionManager->actionContainer(Constants::Menus::Edit);

    auto* undo = new QAction(PlaylistWidget::tr("&Undo"), self);
    editMenu->addAction(actionManager->registerAction(undo, Constants::Actions::Undo, playlistContext->context()));
    QObject::connect(undo, &QAction::triggered, this, [this]() { playlistController->undoPlaylistChanges(); });
    QObject::connect(playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, undo]() { undo->setEnabled(playlistController->canUndo()); });
    undo->setEnabled(playlistController->canUndo());

    auto* redo = new QAction(PlaylistWidget::tr("&Redo"), self);
    editMenu->addAction(actionManager->registerAction(redo, Constants::Actions::Redo, playlistContext->context()));
    QObject::connect(redo, &QAction::triggered, this, [this]() { playlistController->redoPlaylistChanges(); });
    QObject::connect(playlistController, &PlaylistController::playlistHistoryChanged, this,
                     [this, redo]() { redo->setEnabled(playlistController->canRedo()); });
    redo->setEnabled(playlistController->canRedo());

    editMenu->addSeparator();

    auto* clear = new QAction(PlaylistWidget::tr("&Clear"), self);
    editMenu->addAction(actionManager->registerAction(clear, Constants::Actions::Clear, playlistContext->context()));
    QObject::connect(clear, &QAction::triggered, this, [this]() {
        if(auto* playlist = playlistController->currentPlaylist()) {
            playlist->clear();
            model->reset(currentPreset, playlist);
        }
    });

    auto* selectAll = new QAction(PlaylistWidget::tr("&Select All"), self);
    editMenu->addAction(
        actionManager->registerAction(selectAll, Constants::Actions::SelectAll, playlistContext->context()));
    QObject::connect(selectAll, &QAction::triggered, playlistView, &QTreeView::selectAll);
}

void PlaylistWidgetPrivate::onPresetChanged(const PlaylistPreset& preset)
{
    if(currentPreset.id == preset.id) {
        changePreset(preset);
    }
}

void PlaylistWidgetPrivate::changePreset(const PlaylistPreset& preset)
{
    currentPreset = preset;
    if(auto* playlist = playlistController->currentPlaylist()) {
        model->reset(currentPreset, playlist);
    }
}

void PlaylistWidgetPrivate::changePlaylist(Core::Playlist::Playlist* playlist) const
{
    model->reset(currentPreset, playlist);
}

void PlaylistWidgetPrivate::resetTree() const
{
    playlistView->expandAll();
    playlistView->scrollToTop();
}

bool PlaylistWidgetPrivate::isHeaderHidden() const
{
    return playlistView->isHeaderHidden();
}

bool PlaylistWidgetPrivate::isScrollbarHidden() const
{
    return playlistView->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void PlaylistWidgetPrivate::setHeaderHidden(bool showHeader) const
{
    playlistView->setHeaderHidden(!showHeader);
}

void PlaylistWidgetPrivate::setScrollbarHidden(bool showScrollBar) const
{
    playlistView->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void PlaylistWidgetPrivate::selectionChanged()
{
    if(changingSelection) {
        return;
    }

    changingSelection = true;

    QItemSelectionModel* selectionModel = playlistView->selectionModel();

    QModelIndexList indexes = selectionModel->selectedIndexes();
    QItemSelection itemsToSelect;
    QItemSelection itemsToDeselect;

    Core::TrackList tracks;
    int firstIndex{-1};

    while(!indexes.empty()) {
        const QModelIndex& index = indexes.front();
        indexes.pop_front();

        if(!index.isValid()) {
            continue;
        }

        const auto type = index.data(PlaylistItem::Type).toInt();

        if(type != PlaylistItem::Track) {
            itemsToDeselect.emplace_back(index);

            const QItemSelection children{model->index(0, 0, index),
                                          model->index(model->rowCount(index) - 1, 0, index)};
            const auto childIndexes = children.indexes();

            auto childrenToSelect = std::views::filter(
                childIndexes, [&](const QModelIndex& childIndex) { return !selectionModel->isSelected(childIndex); });

            std::ranges::copy(childrenToSelect, std::back_inserter(indexes));
        }
        else {
            itemsToSelect.emplace_back(index);
            tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<Core::Track>());
            const int trackIndex = index.data(PlaylistItem::Role::Index).toInt();
            firstIndex           = firstIndex >= 0 ? std::min(firstIndex, trackIndex) : trackIndex;
        }
    }
    selectionModel->select(itemsToDeselect, QItemSelectionModel::Deselect);
    selectionModel->select(itemsToSelect, QItemSelectionModel::Select);

    if(!tracks.empty()) {
        selectionController->changeSelectedTracks(firstIndex, tracks);

        if(settings->value<Settings::PlaybackFollowsCursor>()) {
            if(auto* currentPlaylist = playlistController->currentPlaylist()) {
                if(currentPlaylist->currentTrackIndex() != firstIndex) {
                    if(playlistController->playState() != Core::Player::PlayState::Playing) {
                        currentPlaylist->changeCurrentTrack(firstIndex);
                    }
                    else {
                        currentPlaylist->scheduleNextIndex(firstIndex);
                    }
                    playlistController->playlistHandler()->schedulePlaylist(currentPlaylist);
                }
            }
        }
    }
    changingSelection = false;
}

void PlaylistWidgetPrivate::playlistTracksChanged(int index) const
{
    const Core::TrackList tracks = getAllTracks(model, {QModelIndex{}});

    if(!tracks.empty()) {
        const QModelIndexList selected = playlistView->selectionModel()->selectedIndexes();
        if(!selected.empty()) {
            const int firstIndex                 = selected.front().data(PlaylistItem::Role::Index).toInt();
            const Core::TrackList selectedTracks = getAllTracks(model, selected);
            selectionController->changeSelectedTracks(firstIndex, selectedTracks);
        }
    }

    playlistController->playlistHandler()->clearSchedulePlaylist();

    if(auto* playlist = playlistController->currentPlaylist()) {
        playlist->replaceTracks(tracks);
        if(index >= 0) {
            playlist->changeCurrentTrack(index);
        }
        model->updateHeader(playlist);
    }
}

void PlaylistWidgetPrivate::tracksRemoved() const
{
    const auto selected = playlistView->selectionModel()->selectedIndexes();

    QModelIndexList trackSelection;
    Core::Playlist::IndexSet indexes;

    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            trackSelection.push_back(index);
            indexes.emplace(index.data(PlaylistItem::Index).toInt());
        }
    }

    playlistController->saveCurrentPlaylist();

    model->removeTracks(trackSelection);
    playlistController->playlistHandler()->clearSchedulePlaylist();

    if(auto* playlist = playlistController->currentPlaylist()) {
        playlist->removeTracks(indexes);
        model->updateHeader(playlist);
    }

    playlistController->saveCurrentPlaylist();
}

void PlaylistWidgetPrivate::customHeaderMenuRequested(QPoint pos)
{
    auto* menu = new QMenu(self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* presetsMenu = new QMenu(PlaylistWidget::tr("Presets"), menu);

    const auto& presets = playlistController->presetRegistry()->items();

    for(const auto& [index, preset] : presets) {
        const QString name = preset.name;
        auto* switchPreset = new QAction(name, presetsMenu);
        if(preset == currentPreset) {
            presetsMenu->setDefaultAction(switchPreset);
        }
        QObject::connect(switchPreset, &QAction::triggered, self,
                         [this, name]() { settings->set<Settings::CurrentPreset>(name); });
        presetsMenu->addAction(switchPreset);
        //            }
    }
    menu->addMenu(presetsMenu);

    menu->popup(self->mapToGlobal(pos));
}

void PlaylistWidgetPrivate::doubleClicked(const QModelIndex& /*index*/) const
{
    selectionController->executeAction(TrackAction::Play);
    model->currentTrackChanged(playlistController->currentTrack());
    playlistView->clearSelection();
}

void PlaylistWidgetPrivate::followCurrentTrack(const Core::Track& track, int index) const
{
    if(!settings->value<Settings::CursorFollowsPlayback>()) {
        return;
    }

    if(playlistController->playState() != Core::Player::PlayState::Playing) {
        return;
    }

    if(!track.isValid()) {
        return;
    }

    const QModelIndex modelIndex = model->indexForTrackIndex(track, index);
    if(!modelIndex.isValid()) {
        return;
    }

    const QRect indexRect    = playlistView->visualRect(modelIndex);
    const QRect viewportRect = playlistView->viewport()->rect();

    if((indexRect.top() < 0) || (viewportRect.bottom() - indexRect.bottom() < 0)) {
        playlistView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    }

    playlistView->setCurrentIndex(modelIndex);
}

void PlaylistWidgetPrivate::switchContextMenu(int /*section*/, QPoint pos)
{
    auto* menu = new QMenu(self);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    //        const auto currentPlaylist = playlistController->currentPlaylist();
    const auto& playlists = playlistController->playlists();

    for(const auto& playlist : playlists) {
        auto* switchPl = new QAction(playlist->name(), menu);
        const int id   = playlist->id();
        QObject::connect(switchPl, &QAction::triggered, self,
                         [this, id]() { playlistController->changeCurrentPlaylist(id); });
        menu->addAction(switchPl);
    }
    menu->popup(self->mapToGlobal(pos));
}

QCoro::Task<void> PlaylistWidgetPrivate::changeSort(QString script) const
{
    /* if(playlistView->selectionModel()->hasSelection()) { }
     else*/
    if(auto* playlist = playlistController->currentPlaylist()) {
        const auto sortedTracks = co_await Utils::asyncExec(
            [&script, &playlist]() { return Core::Library::Sorting::calcSortTracks(script, playlist->tracks()); });
        playlist->replaceTracks(sortedTracks);
        changePlaylist(playlist);
    }
    co_return;
}

void PlaylistWidgetPrivate::addSortMenu(QMenu* parent)
{
    auto* sortMenu = new QMenu(PlaylistWidget::tr("Sort"), parent);

    const auto& groups = playlistController->sortRegistry()->items();
    for(const auto& [index, script] : groups) {
        auto* switchSort = new QAction(script.name, sortMenu);
        QObject::connect(switchSort, &QAction::triggered, self, [this, script]() { changeSort(script.script); });
        sortMenu->addAction(switchSort);
    }
    parent->addMenu(sortMenu);
}

PlaylistWidget::PlaylistWidget(Utils::ActionManager* actionManager, PlaylistController* playlistController,
                               TrackSelectionController* selectionController, Utils::SettingsManager* settings,
                               QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<PlaylistWidgetPrivate>(this, actionManager, playlistController, selectionController, settings)}
{
    setObjectName("Playlist");
}

PlaylistWidget::~PlaylistWidget() = default;

QString PlaylistWidget::name() const
{
    return QStringLiteral("Playlist");
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* removeRows = new QAction(tr("Remove"), menu);
    QObject::connect(removeRows, &QAction::triggered, this, [this]() { p->tracksRemoved(); });
    menu->addAction(removeRows);

    menu->addSeparator();

    p->addSortMenu(menu);
    p->selectionController->addTrackContextMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}

void PlaylistWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(p->selectionController->hasTracks()) {
            p->selectionController->executeAction(TrackAction::Play);
        }
        p->playlistView->clearSelection();
    }
    QWidget::keyPressEvent(event);
}
} // namespace Fy::Gui::Widgets::Playlist
