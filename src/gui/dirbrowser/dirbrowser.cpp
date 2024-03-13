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
#include "dirproxymodel.h"
#include "dirtree.h"
#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/toolbutton.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTreeView>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

constexpr auto DirPlaylist = "␟DirBrowserPlaylist␟";

namespace {
class DirChange : public QUndoCommand
{
public:
    DirChange(Fooyin::DirBrowser* browser, QAbstractItemView* view, const QString& oldPath, const QString& newPath)
        : QUndoCommand{nullptr}
        , m_browser{browser}
        , m_view{view}
    {
        m_oldState.path      = oldPath;
        m_oldState.scrollPos = m_view->verticalScrollBar()->value();

        m_newState.path = newPath;
    }

    [[nodiscard]] QString undoPath() const
    {
        return m_oldState.path;
    }

    void undo() override
    {
        m_newState.scrollPos = m_view->verticalScrollBar()->value();

        QObject::connect(
            m_browser, &Fooyin::DirBrowser::rootChanged, m_browser,
            [this]() { m_view->verticalScrollBar()->setValue(m_oldState.scrollPos); },
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

        m_view->setUpdatesEnabled(false);
        m_browser->updateDir(m_oldState.path);
    }

    void redo() override
    {
        if(m_newState.scrollPos >= 0) {
            QObject::connect(
                m_browser, &Fooyin::DirBrowser::rootChanged, m_browser,
                [this]() { m_view->verticalScrollBar()->setValue(m_newState.scrollPos); },
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
        }

        m_view->setUpdatesEnabled(false);
        m_browser->updateDir(m_newState.path);
    }

private:
    struct State
    {
        QString path;
        int scrollPos{-1};
    };

    Fooyin::DirBrowser* m_browser;
    QAbstractItemView* m_view;

    State m_oldState;
    State m_newState;
};
} // namespace

namespace Fooyin {
struct DirBrowser::Private
{
    DirBrowser* self;

    PlaylistController* playlistController;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    std::unique_ptr<QFileIconProvider> iconProvider;

    QHBoxLayout* controlLayout;
    QPointer<QLineEdit> dirEdit;
    QPointer<ToolButton> backDir;
    QPointer<ToolButton> forwardDir;
    QPointer<ToolButton> upDir;

    Mode mode{Mode::List};
    DirTree* dirTree;
    QFileSystemModel* model;
    DirProxyModel* proxyModel;
    QUndoStack dirHistory;

    Playlist* playlist{nullptr};

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    Private(DirBrowser* self_, TrackSelectionController* selectionController_, PlaylistController* playlistController_,
            SettingsManager* settings_)
        : self{self_}
        , playlistController{playlistController_}
        , selectionController{selectionController_}
        , settings{settings_}
        , controlLayout{new QHBoxLayout()}
        , dirTree{new DirTree(self)}
        , model{new QFileSystemModel(self)}
        , proxyModel{new DirProxyModel(self)}
        , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::Gui::Internal::DirBrowserDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<Settings::Gui::Internal::DirBrowserMiddleClick>())}
    {
        auto* layout = new QVBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addLayout(controlLayout);
        layout->addWidget(dirTree);

        checkIconProvider();

        model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        model->setNameFilters(Track::supportedFileExtensions());
        model->setNameFilterDisables(false);
        model->setReadOnly(true);

        proxyModel->setSourceModel(model);
        proxyModel->setIconsEnabled(settings->value<Settings::Gui::Internal::DirBrowserIcons>());

        dirTree->setItemDelegate(new DirDelegate(self));
        dirTree->setModel(proxyModel);

        QString rootPath = settings->value<Settings::Gui::Internal::DirBrowserPath>();
        if(rootPath.isEmpty()) {
            rootPath = QDir::homePath();
        }
        dirTree->setRootIndex(proxyModel->mapFromSource(model->setRootPath(rootPath)));
        updateIndent(settings->value<Settings::Gui::Internal::DirBrowserListIndent>());
    }

    void checkIconProvider()
    {
        auto* modelProvider = model->iconProvider();
        if(!modelProvider || modelProvider->icon(QFileIconProvider::Folder).isNull()
           || modelProvider->icon(QFileIconProvider::File).isNull()) {
            iconProvider = std::make_unique<QFileIconProvider>();
            model->setIconProvider(iconProvider.get());
        }
    }

    void handleModelUpdated() const
    {
        if(mode == Mode::List) {
            const QModelIndex root = model->setRootPath(model->rootPath());
            dirTree->setRootIndex(proxyModel->mapFromSource(root));
            proxyModel->reset(root);
        }

        updateControlState();
        dirTree->setUpdatesEnabled(true);
    }

    void handleAction(TrackAction action)
    {
        QModelIndexList selected = dirTree->selectionModel()->selectedRows();

        if(selected.empty()) {
            return;
        }

        QString firstPath;

        if(selected.size() == 1) {
            const QModelIndex index = selected.front();
            if(index.isValid()) {
                const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
                if(filePath.isFile()) {
                    // Add all files in same directory
                    selected  = {proxyModel->mapToSource(selected.front()).parent()};
                    firstPath = filePath.absoluteFilePath();
                }
            }
        }

        QList<QUrl> files;

        for(const QModelIndex& index : selected) {
            if(index.isValid()) {
                const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
                if(filePath.isDir()) {
                    files.append(
                        Utils::File::getUrlsInDir(filePath.absoluteFilePath(), Track::supportedFileExtensions()));
                }
                else {
                    files.append(QUrl::fromLocalFile(filePath.absoluteFilePath()));
                }
            }
        }

        if(files.empty()) {
            return;
        }

        if(firstPath.isEmpty()) {
            firstPath = files.front().path();
        }

        QDir parentDir{firstPath};
        parentDir.cdUp();
        const QString playlistName = parentDir.dirName();

        switch(action) {
            case(TrackAction::Play): {
                handlePlayAction(files, firstPath);
                break;
            }
            case(TrackAction::AddCurrentPlaylist):
                playlistController->filesToCurrentPlaylist(files);
                break;
            case(TrackAction::SendCurrentPlaylist):
                playlistController->filesToCurrentPlaylist(files, true);
                break;
            case(TrackAction::SendNewPlaylist):
                playlistController->filesToNewPlaylist(playlistName, files);
                break;
            case(TrackAction::AddActivePlaylist):
                playlistController->filesToActivePlaylist(files);
                break;
            case(TrackAction::None):
            case(TrackAction::Expand):
                break;
        }
    }

    void handlePlayAction(const QList<QUrl>& files, const QString& startingFile)
    {
        int playIndex{0};

        if(!startingFile.isEmpty()) {
            auto rowIt = std::ranges::find_if(
                std::as_const(files), [&startingFile](const QUrl& file) { return file.path() == startingFile; });
            if(rowIt != files.cend()) {
                playIndex = static_cast<int>(std::distance(files.cbegin(), rowIt));
            }
        }

        TrackList tracks;
        std::ranges::transform(files, std::back_inserter(tracks),
                               [](const QUrl& file) { return Track{file.toLocalFile()}; });

        startPlayback(tracks, playIndex);
    }

    void handleDoubleClick(const QModelIndex& index)
    {
        if(!index.isValid()) {
            return;
        }

        const QString path = index.data(QFileSystemModel::FilePathRole).toString();

        if(path.isEmpty() && mode == Mode::List) {
            goUp();
            return;
        }

        const QFileInfo filePath{path};
        if(filePath.isDir()) {
            if(mode == Mode::List) {
                changeRoot(filePath.absoluteFilePath());
            }
            return;
        }

        handleAction(doubleClickAction);
    }

    void handleMiddleClick()
    {
        handleAction(middleClickAction);
    }

    void changeRoot(const QString& root)
    {
        if(root.isEmpty() || !QFileInfo::exists(root)) {
            return;
        }

        if(QDir{root} == QDir{model->rootPath()}) {
            return;
        }

        auto* changeDir = new DirChange(self, dirTree, model->rootPath(), root);
        dirHistory.push(changeDir);
    }

    void updateIndent(bool show) const
    {
        if(show || mode == Mode::Tree) {
            dirTree->resetIndentation();
        }
        else {
            dirTree->setIndentation(0);
        }
    }

    void setControlsEnabled(bool enabled)
    {
        if(enabled && !upDir && !backDir && !forwardDir) {
            upDir      = new ToolButton(self);
            backDir    = new ToolButton(self);
            forwardDir = new ToolButton(self);

            upDir->setStretchEnabled(true);
            upDir->setDefaultAction(new QAction(Utils::iconFromTheme(Constants::Icons::Up), tr("Go up"), upDir));
            backDir->setStretchEnabled(true);
            backDir->setDefaultAction(
                new QAction(Utils::iconFromTheme(Constants::Icons::GoPrevious), tr("Go back"), backDir));
            forwardDir->setStretchEnabled(true);
            forwardDir->setDefaultAction(
                new QAction(Utils::iconFromTheme(Constants::Icons::GoNext), tr("Go forwards"), forwardDir));

            QObject::connect(upDir, &QPushButton::pressed, self, [this]() { goUp(); });
            QObject::connect(backDir, &QPushButton::pressed, self, [this]() {
                if(dirHistory.canUndo()) {
                    dirHistory.undo();
                }
            });
            QObject::connect(forwardDir, &QPushButton::pressed, self, [this]() {
                if(dirHistory.canRedo()) {
                    dirHistory.redo();
                }
            });

            controlLayout->insertWidget(0, upDir);
            controlLayout->insertWidget(0, forwardDir);
            controlLayout->insertWidget(0, backDir);
        }
        else {
            if(backDir) {
                backDir->deleteLater();
            }
            if(forwardDir) {
                forwardDir->deleteLater();
            }
            if(upDir) {
                upDir->deleteLater();
            }
        }
    }

    void setLocationEnabled(bool enabled)
    {
        if(enabled && !dirEdit) {
            dirEdit = new QLineEdit(self);
            QObject::connect(dirEdit, &QLineEdit::textEdited, self, [this](const QString& dir) { changeRoot(dir); });
            controlLayout->addWidget(dirEdit, 1);
            dirEdit->setText(model->rootPath());
        }
        else {
            if(dirEdit) {
                dirEdit->deleteLater();
            }
        }
    }

    void changeMode(Mode newMode)
    {
        mode = newMode;

        const QString rootPath = model->rootPath();

        proxyModel->setFlat(mode == Mode::List);

        const QModelIndex root = model->setRootPath(rootPath);
        dirTree->setRootIndex(proxyModel->mapFromSource(root));

        updateIndent(settings->value<Settings::Gui::Internal::DirBrowserListIndent>());
    }

    void startPlayback(const TrackList& tracks, int row)
    {
        if(!playlist) {
            playlist = playlistController->playlistHandler()->createTempPlaylist(QString::fromLatin1(DirPlaylist));
        }

        playlistController->playlistHandler()->replacePlaylistTracks(playlist->id(), tracks);

        playlist->changeCurrentIndex(row);
        playlistController->playlistHandler()->startPlayback(playlist);
    }

    void updateControlState() const
    {
        if(upDir) {
            upDir->setEnabled(proxyModel->canGoUp());
        }
        if(backDir) {
            backDir->setEnabled(dirHistory.canUndo());
        }
        if(forwardDir) {
            forwardDir->setEnabled(dirHistory.canRedo());
        }
    }

    void goUp()
    {
        QDir root{model->rootPath()};

        if(!root.cdUp()) {
            return;
        }

        const QString newPath = root.absolutePath();

        if(dirHistory.canUndo()) {
            if(const auto* prevDir = static_cast<const DirChange*>(dirHistory.command(dirHistory.index() - 1))) {
                if(prevDir->undoPath() == newPath) {
                    dirHistory.undo();
                    return;
                }
            }
        }

        self->updateDir(newPath);
    }
};

DirBrowser::DirBrowser(TrackSelectionController* selectionController, PlaylistController* playlistController,
                       SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, selectionController, playlistController, settings)}
{
    QObject::connect(p->dirTree, &QTreeView::doubleClicked, this,
                     [this](const QModelIndex& index) { p->handleDoubleClick(index); });
    QObject::connect(p->dirTree, &DirTree::middleClicked, this, [this]() { p->handleMiddleClick(); });

    QObject::connect(p->model, &QAbstractItemModel::layoutChanged, this, [this]() { p->handleModelUpdated(); });
    QObject::connect(
        p->proxyModel, &QAbstractItemModel::modelReset, this, [this]() { emit rootChanged(); }, Qt::QueuedConnection);

    QObject::connect(p->playlistController->playerController(), &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->proxyModel->setPlayState(state); });
    QObject::connect(p->playlistController->playlistHandler(), &PlaylistHandler::activePlaylistChanged, this,
                     [this](Playlist* playlist) {
                         if(p->playlist && playlist->id() != p->playlist->id()) {
                             p->proxyModel->setPlayingPath({});
                         }
                     });
    QObject::connect(p->playlistController->playerController(), &PlayerController::playlistTrackChanged, this,
                     [this](const PlaylistTrack& track) {
                         if(p->playlist && p->playlist->id() == track.playlistId) {
                             p->proxyModel->setPlayingPath(track.track.filepath());
                         }
                     });

    settings->subscribe<Settings::Gui::Internal::DirBrowserDoubleClick>(
        this, [this](int action) { p->doubleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::Gui::Internal::DirBrowserMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
    p->settings->subscribe<Settings::Gui::Internal::DirBrowserMode>(
        this, [this](int mode) { p->changeMode(static_cast<Mode>(mode)); });
    p->settings->subscribe<Settings::Gui::Internal::DirBrowserIcons>(
        this, [this](bool enabled) { p->proxyModel->setIconsEnabled(enabled); });
    p->settings->subscribe<Settings::Gui::Internal::DirBrowserListIndent>(
        this, [this](bool enabled) { p->updateIndent(enabled); });
    p->settings->subscribe<Settings::Gui::Internal::DirBrowserControls>(
        this, [this](bool enabled) { p->setControlsEnabled(enabled); });
    p->settings->subscribe<Settings::Gui::Internal::DirBrowserLocation>(
        this, [this](bool enabled) { p->setLocationEnabled(enabled); });

    p->changeMode(static_cast<Mode>(settings->value<Settings::Gui::Internal::DirBrowserMode>()));
    p->setControlsEnabled(settings->value<Settings::Gui::Internal::DirBrowserControls>());
    p->setLocationEnabled(settings->value<Settings::Gui::Internal::DirBrowserLocation>());
    p->updateControlState();
}

DirBrowser::~DirBrowser()
{
    p->settings->set<Settings::Gui::Internal::DirBrowserPath>(p->model->rootPath());
}

QString DirBrowser::name() const
{
    return QStringLiteral("Directory Browser");
}

QString DirBrowser::layoutName() const
{
    return QStringLiteral("DirectoryBrowser");
}

void DirBrowser::updateDir(const QString& dir)
{
    const QModelIndex root = p->model->setRootPath(dir);
    p->dirTree->setRootIndex(p->proxyModel->mapFromSource(root));

    if(p->dirEdit) {
        p->dirEdit->setText(dir);
    }

    if(p->playlist) {
        p->proxyModel->setPlayingPath(p->playlist->currentTrack().filepath());
    }
}

void DirBrowser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* playAction = new QAction(tr("Play"), menu);
    QObject::connect(playAction, &QAction::triggered, this, [this]() { p->handleAction(TrackAction::Play); });

    auto* addCurrent = new QAction(tr("Add to current playlist"), menu);
    QObject::connect(addCurrent, &QAction::triggered, this,
                     [this]() { p->handleAction(TrackAction::AddCurrentPlaylist); });

    auto* addActive = new QAction(tr("Add to active playlist"), menu);
    QObject::connect(addActive, &QAction::triggered, this,
                     [this]() { p->handleAction(TrackAction::AddActivePlaylist); });

    auto* sendCurrent = new QAction(tr("Send to current playlist"), menu);
    QObject::connect(sendCurrent, &QAction::triggered, this,
                     [this]() { p->handleAction(TrackAction::SendCurrentPlaylist); });

    auto* sendNew = new QAction(tr("Send to new playlist"), menu);
    QObject::connect(sendNew, &QAction::triggered, this, [this]() { p->handleAction(TrackAction::SendNewPlaylist); });

    menu->addAction(playAction);
    menu->addSeparator();
    menu->addAction(addCurrent);
    menu->addAction(addActive);
    menu->addAction(sendCurrent);
    menu->addAction(sendNew);
    menu->addSeparator();

    const QModelIndex index = p->dirTree->indexAt(p->dirTree->mapFromGlobal(event->globalPos()));

    if(index.isValid()) {
        const QFileInfo selectedPath{index.data(QFileSystemModel::FilePathRole).toString()};
        if(selectedPath.isDir()) {
            const QString dir = index.data(QFileSystemModel::FilePathRole).toString();
            auto* setRoot     = new QAction(tr("Set as root"), menu);
            QObject::connect(setRoot, &QAction::triggered, this, [this, dir]() { p->changeRoot(dir); });
            menu->addAction(setRoot);
        }
    }

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_dirbrowser.cpp"
