/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "playlistbox.h"

#include "dialog/autoplaylistdialog.h"
#include "playlist/playlistcontroller.h"

#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/widgets/popuplineedit.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>
#include <utils/crypto.h>
#include <utils/utils.h>

#include <QAction>
#include <QComboBox>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
PlaylistBox::PlaylistBox(ActionManager* actionManager, PlaylistController* playlistController, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistController{playlistController}
    , m_playlistHandler{m_playlistController->playlistHandler()}
    , m_playlistBox{new QComboBox(this)}
    , m_context{new WidgetContext(this, Context{IdList{Id{"Fooyin.Context.PlaylistSwitcher."}.append(id())}}, this)}
    , m_editAutoPlaylistAction{new QAction(tr("&Edit autoplaylist"), this)}
    , m_editAutoPlaylistCmd{m_actionManager->registerAction(m_editAutoPlaylistAction,
                                                            Constants::Actions::EditAutoPlaylist, m_context->context())}
    , m_renameAction{new QAction(tr("Re&name playlist"), this)}
    , m_renameCmd{m_actionManager->registerAction(m_renameAction, Constants::Actions::Rename, m_context->context())}
    , m_removeAction{new QAction(tr("&Remove playlist"), this)}
    , m_removeCmd{m_actionManager->registerAction(m_removeAction, Constants::Actions::Remove, m_context->context())}
    , m_newPlaylistAction{new QAction(tr("Add &new playlist"), this)}
    , m_newPlaylistCmd{m_actionManager->registerAction(m_newPlaylistAction, Constants::Actions::NewPlaylist,
                                                       m_context->context())}
    , m_newAutoPlaylistAction{new QAction(tr("Add new &autoplaylist"), this)}
    , m_newAutoPlaylistCmd{m_actionManager->registerAction(m_newAutoPlaylistAction, Constants::Actions::NewAutoPlaylist,
                                                           m_context->context())}
    , m_renameCancelled{false}
{
    QObject::setObjectName(PlaylistBox::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_playlistBox);

    m_actionManager->addContextObject(m_context);

    setupActions();

    QObject::connect(m_playlistBox, &QComboBox::currentIndexChanged, this, &PlaylistBox::changePlaylist);
    m_playlistBox->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_playlistBox, &QWidget::customContextMenuRequested, this, &PlaylistBox::showContextMenu);
    QObject::connect(m_playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { currentPlaylistChanged(m_playlistController->currentPlaylist()); });
    QObject::connect(
        m_playlistController, &PlaylistController::currentPlaylistChanged, this,
        [this](const Playlist* /*prevPlaylist*/, const Playlist* playlist) { currentPlaylistChanged(playlist); });
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistBox::addPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistBox::removePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     [this](const Playlist* playlist) { playlistRenamed(playlist); });
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     [this](const Playlist*, const Playlist*) { updateActionState(); });

    setupBox();
    currentPlaylistChanged(m_playlistController->currentPlaylist());
    updateActionState();
}

QString PlaylistBox::name() const
{
    return tr("Playlist Switcher");
}

QString PlaylistBox::layoutName() const
{
    return u"PlaylistSwitcher"_s;
}

void PlaylistBox::setupBox()
{
    // Prevent currentIndexChanged triggering changePlaylist
    const QSignalBlocker block{m_playlistBox};

    const auto& playlists = m_playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist);
    }
}

void PlaylistBox::addPlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_playlistBox->addItem(playlist->name(), QVariant::fromValue(playlist->id()));
}

void PlaylistBox::removePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    if(const auto* current = currentPlaylist(); current && current->id() == playlist->id()) {
        closeRenameEditor();
    }

    const int count = m_playlistBox->count();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<UId>() == playlist->id()) {
            m_playlistBox->removeItem(i);
        }
    }
}

void PlaylistBox::playlistRenamed(const Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_playlistBox->count();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<UId>() == playlist->id()) {
            m_playlistBox->setItemText(i, playlist->name());
        }
    }
}

void PlaylistBox::currentPlaylistChanged(const Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_playlistBox->count();
    const UId id    = playlist->id();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<UId>() == id) {
            // Prevent currentIndexChanged triggering changePlaylist
            const QSignalBlocker block{m_playlistBox};
            m_playlistBox->setCurrentIndex(i);
        }
    }
}

void PlaylistBox::changePlaylist(int index)
{
    if(index < 0 || index >= m_playlistBox->count()) {
        return;
    }

    const auto id = m_playlistBox->itemData(index).value<UId>();
    m_playlistController->changeCurrentPlaylist(id);
}

void PlaylistBox::setupActions()
{
    const QStringList editCategory{tr("Edit")};
    const QStringList switcherCategory{tr("Playlist Switcher")};

    m_editAutoPlaylistAction->setStatusTip(tr("Edit the selected autoplaylist"));
    m_editAutoPlaylistCmd->setCategories(switcherCategory);
    QObject::connect(m_editAutoPlaylistAction, &QAction::triggered, this, &PlaylistBox::editCurrentAutoPlaylist);

    m_renameAction->setStatusTip(tr("Rename the selected playlist"));
    m_renameAction->setShortcutVisibleInContextMenu(true);
    m_renameCmd->setCategories(editCategory);
    m_renameCmd->setDescription(tr("Rename"));
    m_renameCmd->setAttribute(ProxyAction::UpdateText);
    m_renameCmd->setDefaultShortcut(Qt::Key_F2);
    QObject::connect(m_renameAction, &QAction::triggered, this, &PlaylistBox::showRenameEditor);

    m_removeAction->setStatusTip(tr("Remove the selected playlist"));
    m_removeAction->setShortcutVisibleInContextMenu(true);
    m_removeCmd->setCategories(editCategory);
    m_removeCmd->setDescription(tr("Remove"));
    m_removeCmd->setAttribute(ProxyAction::UpdateText);
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeAction, &QAction::triggered, this, &PlaylistBox::removeCurrentPlaylist);

    m_newPlaylistAction->setStatusTip(tr("Create a new empty playlist"));
    m_newPlaylistAction->setShortcutVisibleInContextMenu(true);
    QObject::connect(m_newPlaylistAction, &QAction::triggered, this, [this]() {
        closeRenameEditor();

        if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
            m_playlistController->changeCurrentPlaylist(playlist);
        }
    });

    m_newAutoPlaylistAction->setStatusTip(tr("Create a new autoplaylist"));
    m_newAutoPlaylistCmd->setCategories(switcherCategory);
    QObject::connect(m_newAutoPlaylistAction, &QAction::triggered, this, [this]() {
        closeRenameEditor();

        auto* autoDialog = new AutoPlaylistDialog(Utils::getMainWindow());
        autoDialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(autoDialog, &AutoPlaylistDialog::playlistEdited, autoDialog,
                         [this](const QString& name, const QString& query, const QString& sortQuery, bool forceSorted) {
                             if(auto* playlist
                                = m_playlistHandler->createNewAutoPlaylist(name, query, sortQuery, forceSorted)) {
                                 m_playlistController->changeCurrentPlaylist(playlist);
                             }
                         });
        autoDialog->show();
    });
}

void PlaylistBox::updateActionState()
{
    const bool hasPlaylist = currentPlaylist() != nullptr;
    const bool isAuto      = hasPlaylist && currentPlaylist()->isAutoPlaylist();

    if(m_editAutoPlaylistAction) {
        m_editAutoPlaylistAction->setEnabled(isAuto);
    }
    if(m_renameAction) {
        m_renameAction->setText(isAuto ? tr("Re&name autoplaylist") : tr("Re&name playlist"));
        m_renameAction->setEnabled(hasPlaylist);
    }
    if(m_removeAction) {
        m_removeAction->setText(isAuto ? tr("&Remove autoplaylist") : tr("&Remove playlist"));
        m_removeAction->setEnabled(hasPlaylist);
    }
}

void PlaylistBox::showContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    updateActionState();

    menu->addAction(m_newPlaylistCmd ? m_newPlaylistCmd->action() : m_newPlaylistAction);
    menu->addAction(m_newAutoPlaylistCmd ? m_newAutoPlaylistCmd->action() : m_newAutoPlaylistAction);

    if(auto* playlist = currentPlaylist()) {
        menu->addSeparator();

        if(playlist->isAutoPlaylist()) {
            menu->addAction(m_editAutoPlaylistCmd ? m_editAutoPlaylistCmd->action() : m_editAutoPlaylistAction);
        }

        menu->addAction(m_renameCmd ? m_renameCmd->action() : m_renameAction);
        menu->addAction(m_removeCmd ? m_removeCmd->action() : m_removeAction);
    }

    menu->popup(m_playlistBox->mapToGlobal(pos));
}

void PlaylistBox::editCurrentAutoPlaylist()
{
    auto* playlist = currentPlaylist();
    if(!playlist || !playlist->isAutoPlaylist()) {
        return;
    }

    closeRenameEditor();

    auto* autoDialog = new AutoPlaylistDialog(m_playlistHandler, playlist, Utils::getMainWindow());
    autoDialog->setAttribute(Qt::WA_DeleteOnClose);
    autoDialog->show();
}

void PlaylistBox::showRenameEditor()
{
    auto* playlist = currentPlaylist();
    if(!playlist) {
        return;
    }

    closeRenameEditor();
    m_renameCancelled = false;

    m_lineEdit = new PopupLineEdit(playlist->name(), this);
    m_lineEdit->setAttribute(Qt::WA_DeleteOnClose);
    m_lineEdit->setGeometry(m_playlistBox->geometry());

    QObject::connect(m_lineEdit, &PopupLineEdit::editingCancelled, this, &PlaylistBox::cancelRenameEditor);
    QObject::connect(m_lineEdit, &QLineEdit::editingFinished, this, [this, playlistId = playlist->id()]() {
        if(!m_lineEdit) {
            return;
        }

        const QString text = m_lineEdit->text();
        closeRenameEditor();

        if(m_renameCancelled) {
            return;
        }

        if(auto* current = m_playlistHandler->playlistById(playlistId)) {
            if(text != current->name()) {
                m_playlistHandler->renamePlaylist(playlistId, text);
            }
        }
    });

    m_lineEdit->show();
    m_lineEdit->selectAll();
    m_lineEdit->setFocus(Qt::ActiveWindowFocusReason);
}

void PlaylistBox::closeRenameEditor()
{
    if(m_lineEdit) {
        m_lineEdit->close();
    }
}

void PlaylistBox::cancelRenameEditor()
{
    m_renameCancelled = true;
    closeRenameEditor();
}

void PlaylistBox::removeCurrentPlaylist()
{
    if(auto* playlist = currentPlaylist()) {
        closeRenameEditor();
        m_playlistHandler->removePlaylist(playlist->id());
    }
}

Playlist* PlaylistBox::currentPlaylist() const
{
    return m_playlistController->currentPlaylist();
}
} // namespace Fooyin
