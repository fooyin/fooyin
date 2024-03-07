/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "dirbrowser.h"

#include "dirdelegate.h"
#include "dirtree.h"
#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QFileSystemModel>
#include <QMenu>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>

constexpr auto DirPlaylist = "␟DirBrowserPlaylist␟";

using namespace std::chrono_literals;

namespace Fooyin {
DirBrowser::DirBrowser(TrackSelectionController* selectionController, PlaylistController* playlistController,
                       SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playlistController{playlistController}
    , m_selectionController{selectionController}
    , m_settings{settings}
    , m_iconProvider{std::make_unique<QFileIconProvider>()}
    , m_dirTree{new DirTree(this)}
    , m_model{new QFileSystemModel(this)}
    , m_proxyModel{new DirProxyModel(m_iconProvider.get(), this)}
    , m_playlist{nullptr}
    , m_doubleClickAction{static_cast<TrackAction>(settings->value<Settings::Gui::Internal::DirBrowserDoubleClick>())}
    , m_middleClickAction{static_cast<TrackAction>(settings->value<Settings::Gui::Internal::DirBrowserMiddleClick>())}
{
    Q_UNUSED(m_selectionController)

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_dirTree);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setIconsEnabled(m_settings->value<Settings::Gui::Internal::DirBrowserIcons>());

    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    m_model->setNameFilters(Track::supportedFileExtensions());
    m_model->setNameFilterDisables(false);
    m_model->setReadOnly(true);
    m_model->setIconProvider(m_iconProvider.get());

    m_dirTree->setItemDelegate(new DirDelegate(this));
    m_dirTree->setModel(m_proxyModel);

    QString rootPath = m_settings->value<Settings::Gui::Internal::DirBrowserPath>();
    if(rootPath.isEmpty()) {
        rootPath = QDir::homePath();
    }

    m_dirTree->setRootIndex(m_proxyModel->mapFromSource(m_model->setRootPath(rootPath)));

    QObject::connect(m_dirTree, &QTreeView::doubleClicked, this, &DirBrowser::handleDoubleClick);
    QObject::connect(m_dirTree, &DirTree::middleClicked, this, &DirBrowser::handleMiddleClick);

    QObject::connect(m_model, &QFileSystemModel::layoutChanged, this, [this]() {
        const QModelIndex root = m_model->setRootPath(m_model->rootPath());
        m_dirTree->setRootIndex(m_proxyModel->mapFromSource(root));
        m_proxyModel->reset(root);
        setUpdatesEnabled(true);
    });

    QObject::connect(m_playlistController->playerController(), &PlayerController::playStateChanged, this,
                     [this](PlayState state) { m_proxyModel->setPlayState(state); });

    QObject::connect(m_playlistController->playerController(), &PlayerController::playlistTrackChanged, this,
                     [this](const PlaylistTrack& track) {
                         if(m_playlist && m_playlist->id() == track.playlistId) {
                             m_proxyModel->setPlayingIndex(track.indexInPlaylist);
                         }
                     });

    m_settings->subscribe<Settings::Gui::Internal::DirBrowserIcons>(
        this, [this](bool enabled) { m_proxyModel->setIconsEnabled(enabled); });
    settings->subscribe<Settings::Gui::Internal::DirBrowserDoubleClick>(
        this, [this](int action) { m_doubleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::Gui::Internal::DirBrowserMiddleClick>(
        this, [this](int action) { m_middleClickAction = static_cast<TrackAction>(action); });
}

DirBrowser::~DirBrowser()
{
    m_settings->set<Settings::Gui::Internal::DirBrowserPath>(m_model->rootPath());
}

QString DirBrowser::name() const
{
    return QStringLiteral("Directory Browser");
}

QString DirBrowser::layoutName() const
{
    return QStringLiteral("DirectoryBrowser");
}

void DirBrowser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto canPlay = [this]() {
        const auto selected = m_dirTree->selectionModel()->selectedRows();
        if(selected.empty()) {
            return false;
        }
        return std::ranges::any_of(selected, [](const QModelIndex& index) {
            const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
            return filePath.isFile();
        });
    };

    auto* playAction = new QAction(tr("Play"), menu);
    QObject::connect(playAction, &QAction::triggered, this, [this]() { handleAction(TrackAction::Play); });
    playAction->setEnabled(canPlay());

    auto* addCurrent = new QAction(tr("Add to current playlist"), menu);
    QObject::connect(addCurrent, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::AddCurrentPlaylist); });

    auto* addActive = new QAction(tr("Add to active playlist"), menu);
    QObject::connect(addActive, &QAction::triggered, this, [this]() { handleAction(TrackAction::AddActivePlaylist); });

    auto* sendCurrent = new QAction(tr("Send to current playlist"), menu);
    QObject::connect(sendCurrent, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::SendCurrentPlaylist); });

    auto* sendNew = new QAction(tr("Send to new playlist"), menu);
    QObject::connect(sendNew, &QAction::triggered, this, [this]() { handleAction(TrackAction::SendNewPlaylist); });

    menu->addAction(playAction);
    menu->addSeparator();
    menu->addAction(addCurrent);
    menu->addAction(addActive);
    menu->addAction(sendCurrent);
    menu->addAction(sendNew);

    menu->popup(event->globalPos());
}

void DirBrowser::handleAction(TrackAction action)
{
    const auto selected = m_dirTree->selectionModel()->selectedRows();

    if(selected.empty()) {
        return;
    }

    int row{-1};
    QString playlistName;
    QList<QUrl> files;

    for(const QModelIndex& index : selected) {
        const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
        if(filePath.isDir()) {
            files.append(Utils::File::getUrlsInDir(filePath.absoluteFilePath(), Track::supportedFileExtensions()));
            if(playlistName.isEmpty()) {
                playlistName = filePath.fileName();
            }
        }
        else {
            files.append(QUrl::fromLocalFile(filePath.absoluteFilePath()));
            if(row < 0) {
                row = index.row();
            }
        }
    }

    if(files.empty()) {
        return;
    }

    if(playlistName.isEmpty()) {
        playlistName = m_model->rootDirectory().dirName();
    }

    switch(action) {
        case(TrackAction::Play): {
            const QString currentRoot = QFileInfo{m_model->rootPath()}.absoluteFilePath();
            const auto rootFiles      = Utils::File::getUrlsInDir(currentRoot, Track::supportedFileExtensions());
            TrackList tracks;
            std::ranges::transform(rootFiles, std::back_inserter(tracks),
                                   [](const QUrl& file) { return Track{file.toLocalFile()}; });
            const int playRow = row - (m_proxyModel->canGoUp() ? 1 : 0);
            m_playlistDir     = currentRoot;
            startPlayback(tracks, playRow);
            break;
        }
        case(TrackAction::AddCurrentPlaylist):
            m_playlistController->filesToCurrentPlaylist(files);
            break;
        case(TrackAction::SendCurrentPlaylist):
            m_playlistController->filesToCurrentPlaylist(files, true);
            break;
        case(TrackAction::SendNewPlaylist):
            m_playlistController->filesToNewPlaylist(playlistName, files);
            break;
        case(TrackAction::AddActivePlaylist):
            m_playlistController->filesToActivePlaylist(files);
            break;
        case(TrackAction::None):
        case(TrackAction::Expand):
            break;
    }
}

void DirBrowser::handleDoubleClick(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};

    if(filePath.isDir()) {
        setUpdatesEnabled(false);
        updateDir(filePath.absoluteFilePath());
        return;
    }

    handleAction(m_doubleClickAction);
}

void DirBrowser::handleMiddleClick()
{
    handleAction(m_middleClickAction);
}

void DirBrowser::startPlayback(const TrackList& tracks, int row)
{
    if(!m_playlist) {
        m_playlist = m_playlistController->playlistHandler()->createTempPlaylist(QString::fromLatin1(DirPlaylist));
    }

    m_playlistController->playlistHandler()->replacePlaylistTracks(m_playlist->id(), tracks);

    m_playlist->changeCurrentIndex(row);
    m_playlistController->playlistHandler()->startPlayback(m_playlist);
}

void DirBrowser::updateDir(const QString& dir)
{
    const QModelIndex root = m_model->setRootPath(dir);
    m_dirTree->setRootIndex(m_proxyModel->mapFromSource(root));

    if(dir != m_playlistDir) {
        m_proxyModel->setPlayingIndex(-1);
    }
    else if(m_playlist) {
        m_proxyModel->setPlayingIndex(m_playlist->currentTrackIndex());
    }
}
} // namespace Fooyin

#include "moc_dirbrowser.cpp"
