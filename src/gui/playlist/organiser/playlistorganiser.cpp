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

#include "playlistorganiser.h"

#include "dialog/autoplaylistdialog.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlistorganiserdelegate.h"
#include "playlistorganisermodel.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QMainWindow>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <stack>

using namespace Qt::StringLiterals;

constexpr auto OrganiserModel = "PlaylistOrganiser/Model";
constexpr auto OrganiserState = "PlaylistOrganiser/State";

namespace {
QByteArray saveExpandedState(QTreeView* view, QAbstractItemModel* model)
{
    QByteArray data;
    QDataStream stream{&data, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    std::stack<QModelIndex> indexesToSave;
    indexesToSave.emplace();

    while(!indexesToSave.empty()) {
        const QModelIndex index = indexesToSave.top();
        indexesToSave.pop();

        if(model->hasChildren(index)) {
            if(index.isValid()) {
                stream << view->isExpanded(index);
            }
            const int childCount = model->rowCount(index);
            for(int row{childCount - 1}; row >= 0; --row) {
                indexesToSave.push(model->index(row, 0, index));
            }
        }
    }

    data = qCompress(data, 9);

    return data;
}

void restoreExpandedState(QTreeView* view, QAbstractItemModel* model, QByteArray data)
{
    if(data.isEmpty()) {
        return;
    }

    data = qUncompress(data);

    QDataStream stream{&data, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    std::stack<QModelIndex> indexesToSave;
    indexesToSave.emplace();

    while(!indexesToSave.empty()) {
        const QModelIndex index = indexesToSave.top();
        indexesToSave.pop();

        if(model->hasChildren(index)) {
            if(index.isValid()) {
                bool expanded;
                stream >> expanded;
                view->setExpanded(index, expanded);
            }
            const int childCount = model->rowCount(index);
            for(int row{childCount - 1}; row >= 0; --row) {
                indexesToSave.push(model->index(row, 0, index));
            }
        }
    }
}
} // namespace

namespace Fooyin {
PlaylistOrganiser::PlaylistOrganiser(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                     SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{playlistInteractor->playlistController()->playerController()}
    , m_organiserTree{new QTreeView(this)}
    , m_model{new PlaylistOrganiserModel(playlistInteractor->handler(), m_playerController)}
    , m_context{new WidgetContext(this, Context{Id{"Context.PlaylistOrganiser."}.append(Utils::generateUniqueHash())},
                                  this)}
    , m_removePlaylist{new QAction(tr("Remove"), this)}
    , m_removeCmd{actionManager->registerAction(m_removePlaylist, Constants::Actions::Remove, m_context->context())}
    , m_renamePlaylist{new QAction(tr("Rename"), this)}
    , m_renameCmd{actionManager->registerAction(m_renamePlaylist, Constants::Actions::Rename, m_context->context())}
    , m_newGroup{new QAction(tr("New group"), this)}
    , m_newGroupCmd{actionManager->registerAction(m_newGroup, "PlaylistOrganiser.NewGroup", m_context->context())}
    , m_newPlaylist{new QAction(tr("Create playlist"), this)}
    , m_newPlaylistCmd{actionManager->registerAction(m_newPlaylist, Constants::Actions::NewPlaylist,
                                                     m_context->context())}
    , m_newAutoPlaylist{new QAction(tr("Create autoplaylist"), this)}
    , m_newAutoPlaylistCmd{actionManager->registerAction(m_newAutoPlaylist, Constants::Actions::NewAutoPlaylist,
                                                         m_context->context())}
    , m_editAutoPlaylist{new QAction(tr("Edit autoplaylist"), this)}
    , m_editAutoPlaylistCmd{
          actionManager->registerAction(m_editAutoPlaylist, Constants::Actions::EditAutoPlaylist, m_context->context())}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_organiserTree);

    m_organiserTree->setHeaderHidden(true);
    m_organiserTree->setUniformRowHeights(true);
    m_organiserTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_organiserTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_organiserTree->setDragEnabled(true);
    m_organiserTree->setAcceptDrops(true);
    m_organiserTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_organiserTree->setDefaultDropAction(Qt::MoveAction);
    m_organiserTree->setDropIndicatorShown(true);
    m_organiserTree->setAllColumnsShowFocus(true);

    m_organiserTree->setModel(m_model);
    m_organiserTree->setItemDelegate(new PlaylistOrganiserDelegate(this));

    actionManager->addContextObject(m_context);

    const QStringList editCategory{tr("Edit")};
    const QStringList organiserCategory{tr("Playlist Organiser")};

    m_removeCmd->setCategories(editCategory);
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);

    m_renameCmd->setCategories(editCategory);
    m_renameCmd->setDefaultShortcut(Qt::Key_F2);
    m_renameCmd->setAttribute(ProxyAction::UpdateText);

    m_newGroupCmd->setCategories(organiserCategory);
    m_newGroup->setStatusTip(tr("Create a new empty group"));
    m_newGroupCmd->setAttribute(ProxyAction::UpdateText);
    m_newGroupCmd->setDefaultShortcut(QKeySequence::AddTab);

    QAction::connect(m_newGroup, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        createGroup(indexes.empty() ? QModelIndex{} : indexes.front());
    });
    QObject::connect(m_removePlaylist, &QAction::triggered, this,
                     [this]() { m_model->removeItems(m_organiserTree->selectionModel()->selectedIndexes()); });
    QObject::connect(m_renamePlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        m_organiserTree->edit(indexes.empty() ? QModelIndex{} : indexes.front());
    });
    QObject::connect(m_newPlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        createPlaylist(indexes.empty() ? QModelIndex{} : indexes.front(), false);
    });
    QObject::connect(m_newAutoPlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        createPlaylist(indexes.empty() ? QModelIndex{} : indexes.front(), true);
    });
    QObject::connect(m_editAutoPlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        editAutoPlaylist(indexes.empty() ? QModelIndex{} : indexes.front());
    });

    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this,
                     [this](const QModelIndex& /*source*/, int /*first*/, int /*last*/, const QModelIndex& target) {
                         if(target.isValid()) {
                             m_organiserTree->expand(target);
                         }
                     });
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& index) {
        if(index.isValid()) {
            m_organiserTree->expand(index);
        }
    });
    QObject::connect(m_model, &PlaylistOrganiserModel::filesDroppedOnPlaylist, this,
                     &PlaylistOrganiser::filesToPlaylist);
    QObject::connect(m_model, &PlaylistOrganiserModel::filesDroppedOnGroup, this, &PlaylistOrganiser::filesToGroup);
    QObject::connect(m_model, &PlaylistOrganiserModel::tracksDroppedOnPlaylist, this,
                     &PlaylistOrganiser::tracksToPlaylist);
    QObject::connect(m_model, &PlaylistOrganiserModel::tracksDroppedOnGroup, this, &PlaylistOrganiser::tracksToGroup);
    QObject::connect(m_organiserTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { selectionChanged(); });

    QObject::connect(
        m_playlistInteractor->handler(), &PlaylistHandler::playlistAdded, this, [this](Playlist* playlist) {
            if(!m_creatingPlaylist) {
                QMetaObject::invokeMethod(
                    m_model, [this, playlist]() { m_model->playlistAdded(playlist); }, Qt::QueuedConnection);
            }
        });
    QObject::connect(m_playlistInteractor->handler(), &PlaylistHandler::playlistRemoved, m_model,
                     &PlaylistOrganiserModel::playlistRemoved);
    QObject::connect(m_playlistInteractor->handler(), &PlaylistHandler::playlistRenamed, m_model,
                     &PlaylistOrganiserModel::playlistRenamed);
    QObject::connect(
        m_playlistInteractor->playlistController(), &PlaylistController::currentPlaylistChanged, this,
        [this]() { QMetaObject::invokeMethod(m_model, [this]() { selectCurrentPlaylist(); }, Qt::QueuedConnection); });
    QObject::connect(m_playlistInteractor->playlistController(), &PlaylistController::playlistsLoaded, this,
                     [this]() { selectCurrentPlaylist(); });

    if(m_model->restoreModel(m_settings->fileValue(OrganiserModel).toByteArray())) {
        const auto state = m_settings->fileValue(OrganiserState).toByteArray();
        restoreExpandedState(m_organiserTree, m_model, state);
        m_model->populateMissing();
    }
    else {
        m_model->populate();
    }

    selectCurrentPlaylist();
}

PlaylistOrganiser::~PlaylistOrganiser()
{
    m_settings->fileSet(OrganiserModel, m_model->saveModel());
    m_settings->fileSet(OrganiserState, saveExpandedState(m_organiserTree, m_model));
}

QString PlaylistOrganiser::name() const
{
    return tr("Playlist Organiser");
}

QString PlaylistOrganiser::layoutName() const
{
    return u"PlaylistOrganiser"_s;
}

void PlaylistOrganiser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(menu, &QObject::destroyed, this, [this]() {
        m_removePlaylist->setEnabled(true);
        m_renamePlaylist->setEnabled(true);
    });

    const QPoint point      = m_organiserTree->viewport()->mapFrom(this, event->pos());
    const QModelIndex index = m_organiserTree->indexAt(point);

    int playlistCount{0};
    int groupCount{0};

    const auto selected = m_organiserTree->selectionModel()->selectedRows();
    for(const auto& selectedIndex : selected) {
        if(selectedIndex.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
            ++playlistCount;
        }
        else {
            ++groupCount;
        }
    }

    if(playlistCount > 0 && groupCount == 0) {
        m_renamePlaylist->setStatusTip(tr("Rename the selected playlist"));
        m_removePlaylist->setStatusTip(tr("Remove the selected playlists"));
    }
    else if(groupCount > 0 && playlistCount == 0) {
        m_renamePlaylist->setStatusTip(tr("Rename the selected group"));
        m_removePlaylist->setStatusTip(tr("Remove the selected groups"));
    }
    else if(playlistCount > 0 && groupCount > 0) {
        m_removePlaylist->setStatusTip(tr("Remove the selected playlists and groups"));
    }

    m_removePlaylist->setEnabled(index.isValid());
    m_renamePlaylist->setEnabled(selected.size() == 1 && index.isValid());

    menu->addAction(m_newPlaylistCmd->action());
    menu->addAction(m_newAutoPlaylistCmd->action());
    menu->addAction(m_newGroupCmd->action());

    if(index.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
        if(auto* savePlaylist = m_actionManager->command(Constants::Actions::SavePlaylist)) {
            menu->addSeparator();
            menu->addAction(savePlaylist->action());
        }
    }

    menu->addSeparator();

    if(playlistCount == 1) {
        if(auto* playlist = index.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>()) {
            if(playlist->isAutoPlaylist()) {
                menu->addAction(m_editAutoPlaylistCmd->action());
            }
        }
    }

    menu->addAction(m_renameCmd->action());
    menu->addAction(m_removeCmd->action());

    menu->popup(event->globalPos());
}

void PlaylistOrganiser::selectionChanged()
{
    const QModelIndexList selectedIndexes = m_organiserTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    const QModelIndex firstIndex = selectedIndexes.front();
    if(firstIndex.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem) {
        return;
    }

    auto* playlist       = firstIndex.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>();
    const UId playlistId = playlist->id();

    if(std::exchange(m_currentPlaylistId, playlistId) != playlistId) {
        m_playlistInteractor->playlistController()->changeCurrentPlaylist(playlist);
    }
}

void PlaylistOrganiser::selectCurrentPlaylist()
{
    auto* playlist = m_playlistInteractor->playlistController()->currentPlaylist();
    if(!playlist) {
        return;
    }

    const UId playlistId = playlist->id();
    if(std::exchange(m_currentPlaylistId, playlistId) != playlistId) {
        const QModelIndex index = m_model->indexForPlaylist(playlist);
        if(index.isValid()) {
            m_organiserTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        }
    }
}

void PlaylistOrganiser::createGroup(const QModelIndex& index) const
{
    QModelIndex parent{index};
    if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
        parent = parent.parent();
    }
    const QModelIndex groupIndex = m_model->createGroup(parent);
    m_organiserTree->edit(groupIndex);
}

void PlaylistOrganiser::createPlaylist(const QModelIndex& index, bool autoPlaylist)
{
    m_creatingPlaylist = true;

    const auto addToModel = [this, index](Playlist* playlist) {
        QModelIndex parent{index};
        if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
            parent = parent.parent();
        }
        return m_model->createPlaylist(playlist, parent);
    };

    if(autoPlaylist) {
        auto* autoDialog = new AutoPlaylistDialog(Utils::getMainWindow());
        autoDialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(autoDialog, &QDialog::finished, this, [this]() { m_creatingPlaylist = false; });
        QObject::connect(autoDialog, &AutoPlaylistDialog::playlistEdited, this,
                         [this, addToModel](const QString& name, const QString& query) {
                             if(auto* playlist = m_playlistInteractor->handler()->createNewAutoPlaylist(name, query)) {
                                 addToModel(playlist);
                             }
                         });
        autoDialog->show();
        return;
    }

    if(auto* playlist = m_playlistInteractor->handler()->createEmptyPlaylist()) {
        const QModelIndex playlistIndex = addToModel(playlist);
        m_organiserTree->edit(playlistIndex);
    }

    m_creatingPlaylist = false;
}

void PlaylistOrganiser::editAutoPlaylist(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    if(auto* playlist = index.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>()) {
        if(playlist->isAutoPlaylist()) {
            auto* autoDialog
                = new AutoPlaylistDialog(m_playlistInteractor->handler(), playlist, Utils::getMainWindow());
            autoDialog->setAttribute(Qt::WA_DeleteOnClose);
            autoDialog->show();
        }
    }
}

void PlaylistOrganiser::filesToPlaylist(const QList<QUrl>& urls, const UId& id)
{
    if(urls.empty()) {
        return;
    }

    m_playlistInteractor->filesToPlaylist(urls, id);
}

void PlaylistOrganiser::filesToGroup(const QList<QUrl>& urls, const QString& group, int index)
{
    if(urls.empty()) {
        return;
    }

    m_playlistInteractor->filesToTracks(urls, [this, group, index](const TrackList& tracks) {
        const QSignalBlocker block{m_playlistInteractor->handler()};
        const QString name = Track::findCommonField(tracks);
        if(auto* playlist = m_playlistInteractor->handler()->createNewPlaylist(name, tracks)) {
            m_model->playlistInserted(playlist, group, index);
        }
    });
}

void PlaylistOrganiser::tracksToPlaylist(const std::vector<int>& trackIds, const UId& id)
{
    const auto tracks = m_playlistInteractor->library()->tracksForIds(trackIds);
    if(tracks.empty()) {
        return;
    }

    if(m_playlistInteractor->handler()->playlistById(id)) {
        m_playlistInteractor->handler()->appendToPlaylist(id, tracks);
    }
}

void PlaylistOrganiser::tracksToGroup(const std::vector<int>& trackIds, const QString& group, int index)
{
    const auto tracks = m_playlistInteractor->library()->tracksForIds(trackIds);
    if(tracks.empty()) {
        return;
    }

    const QSignalBlocker block{m_playlistInteractor->handler()};

    const QString name = Track::findCommonField(tracks);
    if(auto* playlist = m_playlistInteractor->handler()->createNewPlaylist(name, tracks)) {
        m_model->playlistInserted(playlist, group, index);
    }
}
} // namespace Fooyin

#include "moc_playlistorganiser.cpp"
