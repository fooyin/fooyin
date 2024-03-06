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

    m_dirTree->setModel(m_proxyModel);

    QString rootPath = m_settings->value<Settings::Gui::Internal::DirBrowserPath>();
    if(rootPath.isEmpty()) {
        rootPath = QDir::homePath();
    }

    m_dirTree->setRootIndex(m_proxyModel->mapFromSource(m_model->setRootPath(rootPath)));

    QObject::connect(m_dirTree, &QTreeView::doubleClicked, this, &DirBrowser::handleDoubleClick);

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

    auto* showIcons = new QAction(tr("Show icons"), menu);
    showIcons->setCheckable(true);
    showIcons->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserIcons>());
    QObject::connect(showIcons, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Settings::Gui::Internal::DirBrowserIcons>(checked); });

    menu->addAction(showIcons);

    menu->popup(event->globalPos());
}

void DirBrowser::handleDoubleClick(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};

    if(!filePath.exists()) {
        return;
    }

    if(filePath.isDir()) {
        setUpdatesEnabled(false);
        updateDir(filePath.absoluteFilePath());
        return;
    }

    const QStringList files = Utils::File::getFilesInDir(filePath.absolutePath(), Track::supportedFileExtensions());
    if(files.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(files, std::back_inserter(tracks), [](const QString& file) { return Track{file}; });

    m_playlistDir = filePath.absolutePath();

    if(!m_playlist) {
        m_playlist = m_playlistController->playlistHandler()->createTempPlaylist(QString::fromLatin1(DirPlaylist));
    }

    m_playlistController->playlistHandler()->replacePlaylistTracks(m_playlist->id(), tracks);

    int row = index.row();

    if(m_proxyModel->canGoUp()) {
        --row;
    }

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
