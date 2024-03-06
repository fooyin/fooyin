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

#include "playlist/playlistcontroller.h"
#include "playlistorganisermodel.h"

#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <stack>

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
struct PlaylistOrganiser::Private
{
    PlaylistOrganiser* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    PlaylistController* playlistController;

    QTreeView* organiserTree;
    PlaylistOrganiserModel* model;

    WidgetContext context;

    QAction* removePlaylist;
    Command* removeCmd;
    QAction* renamePlaylist;
    Command* renameCmd;
    QAction* newGroup;
    Command* newGroupCmd;
    QAction* newPlaylist;
    Command* newPlaylistCmd;

    Id currentPlaylistId;
    bool creatingPlaylist{false};

    Private(PlaylistOrganiser* self_, ActionManager* actionManager_, PlaylistController* playlistController_,
            SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , playlistController{playlistController_}
        , organiserTree{new QTreeView(self)}
        , model{new PlaylistOrganiserModel(playlistController->playlistHandler())}
        , context{self, Context{Id{"Context.PlaylistOrganiser."}.append(Utils::generateRandomHash())}}
        , removePlaylist{new QAction(tr("Remove"))}
        , removeCmd{actionManager->registerAction(removePlaylist, Constants::Actions::Remove, context.context())}
        , renamePlaylist{new QAction(tr("Rename"))}
        , renameCmd{actionManager->registerAction(renamePlaylist, Constants::Actions::Rename, context.context())}
        , newGroup{new QAction(tr("New Group"))}
        , newGroupCmd{actionManager->registerAction(newGroup, "PlaylistOrganiser.NewGroup", context.context())}
        , newPlaylist{new QAction(tr("Create Playlist"))}
        , newPlaylistCmd{actionManager->registerAction(newPlaylist, "PlaylistOrganiser.NewPlaylist", context.context())}
    {
        organiserTree->setHeaderHidden(true);
        organiserTree->setUniformRowHeights(true);
        organiserTree->setSelectionBehavior(QAbstractItemView::SelectRows);
        organiserTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        organiserTree->setDragEnabled(true);
        organiserTree->setAcceptDrops(true);
        organiserTree->setDragDropMode(QAbstractItemView::InternalMove);
        organiserTree->setDefaultDropAction(Qt::MoveAction);
        organiserTree->setDropIndicatorShown(true);

        organiserTree->setModel(model);

        actionManager->addContextObject(&context);

        removeCmd->setDefaultShortcut(QKeySequence::Delete);
        renameCmd->setDefaultShortcut(Qt::Key_F2);
        newGroupCmd->setDefaultShortcut(QKeySequence::AddTab);
        newPlaylistCmd->setDefaultShortcut(QKeySequence::New);

        QAction::connect(newGroup, &QAction::triggered, self, [this]() {
            const auto indexes = organiserTree->selectionModel()->selectedIndexes();
            createGroup(indexes.empty() ? QModelIndex{} : indexes.front());
        });
        QObject::connect(removePlaylist, &QAction::triggered, self,
                         [this]() { model->removeItems(organiserTree->selectionModel()->selectedIndexes()); });
        QObject::connect(renamePlaylist, &QAction::triggered, self, [this]() {
            const auto indexes = organiserTree->selectionModel()->selectedIndexes();
            organiserTree->edit(indexes.empty() ? QModelIndex{} : indexes.front());
        });
        QObject::connect(newPlaylist, &QAction::triggered, self, [this]() {
            const auto indexes = organiserTree->selectionModel()->selectedIndexes();
            createPlaylist(indexes.empty() ? QModelIndex{} : indexes.front());
        });
    }

    void selectionChanged()
    {
        const QModelIndexList selectedIndexes = organiserTree->selectionModel()->selectedIndexes();
        if(selectedIndexes.empty()) {
            return;
        }

        const QModelIndex firstIndex = selectedIndexes.front();
        if(firstIndex.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem) {
            return;
        }

        auto* playlist      = firstIndex.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>();
        const Id playlistId = playlist->id();

        if(std::exchange(currentPlaylistId, playlistId) != playlistId) {
            playlistController->changeCurrentPlaylist(playlist);
        }
    }

    void selectCurrentPlaylist(Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        const Id playlistId = playlist->id();
        if(std::exchange(currentPlaylistId, playlistId) != playlistId) {
            const QModelIndex index = model->indexForPlaylist(playlist);
            if(index.isValid()) {
                organiserTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
            }
        }
    }

    void createGroup(const QModelIndex& index) const
    {
        QModelIndex parent{index};
        if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
            parent = parent.parent();
        }
        const QModelIndex groupIndex = model->createGroup(parent);
        organiserTree->edit(groupIndex);
    }

    void createPlaylist(const QModelIndex& index)
    {
        creatingPlaylist = true;

        if(auto* playlist = playlistController->playlistHandler()->createEmptyPlaylist()) {
            QModelIndex parent{index};
            if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
                parent = parent.parent();
            }
            const QModelIndex playlistIndex = model->createPlaylist(playlist, parent);
            organiserTree->edit(playlistIndex);
        }

        creatingPlaylist = false;
    }
};

PlaylistOrganiser::PlaylistOrganiser(ActionManager* actionManager, PlaylistController* playlistController,
                                     SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, playlistController, settings)}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->organiserTree);

    QObject::connect(p->model, &QAbstractItemModel::rowsMoved, this,
                     [this](const QModelIndex& /*source*/, int /*first*/, int /*last*/, const QModelIndex& target) {
                         if(target.isValid()) {
                             p->organiserTree->expand(target);
                         }
                     });
    QObject::connect(p->model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& index) {
        if(index.isValid()) {
            p->organiserTree->expand(index);
        }
    });
    QObject::connect(p->organiserTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });

    QObject::connect(
        p->playlistController->playlistHandler(), &PlaylistHandler::playlistAdded, this, [this](Playlist* playlist) {
            if(!p->creatingPlaylist) {
                QMetaObject::invokeMethod(
                    p->model, [this, playlist]() { p->model->playlistAdded(playlist); }, Qt::QueuedConnection);
            }
        });
    QObject::connect(p->playlistController->playlistHandler(), &PlaylistHandler::playlistRemoved, p->model,
                     &PlaylistOrganiserModel::playlistRemoved);
    QObject::connect(p->playlistController->playlistHandler(), &PlaylistHandler::playlistRenamed, p->model,
                     &PlaylistOrganiserModel::playlistRenamed);
    QObject::connect(p->playlistController, &PlaylistController::currentPlaylistChanged, this,
                     [this](Playlist* /*prevPlaylist*/, Playlist* playlist) {
                         QMetaObject::invokeMethod(
                             p->model, [this, playlist]() { p->selectCurrentPlaylist(playlist); },
                             Qt::QueuedConnection);
                     });

    if(p->model->restoreModel(p->settings->fileValue(QString::fromLatin1(OrganiserModel)).toByteArray())) {
        const auto state = p->settings->fileValue(QString::fromLatin1(OrganiserState)).toByteArray();
        restoreExpandedState(p->organiserTree, p->model, state);
        p->model->populateMissing();
    }
    else {
        p->model->populate();
    }
    p->selectCurrentPlaylist(p->playlistController->currentPlaylist());
}

PlaylistOrganiser::~PlaylistOrganiser()
{
    p->settings->fileSet(QString::fromLatin1(OrganiserModel), p->model->saveModel());
    p->settings->fileSet(QString::fromLatin1(OrganiserState), saveExpandedState(p->organiserTree, p->model));
}

QString PlaylistOrganiser::name() const
{
    return QStringLiteral("Playlist Organiser");
}

QString PlaylistOrganiser::layoutName() const
{
    return QStringLiteral("PlaylistOrganiser");
}

void PlaylistOrganiser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QPoint point      = p->organiserTree->viewport()->mapFrom(this, event->pos());
    const QModelIndex index = p->organiserTree->indexAt(point);

    p->removePlaylist->setEnabled(index.isValid());
    p->renamePlaylist->setEnabled(index.isValid());

    menu->addAction(p->newPlaylistCmd->action());
    menu->addAction(p->newGroupCmd->action());
    menu->addSeparator();
    menu->addAction(p->renameCmd->action());
    menu->addAction(p->removeCmd->action());

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_playlistorganiser.cpp"
