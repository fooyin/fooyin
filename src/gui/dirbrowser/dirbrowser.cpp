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

#include "dirbrowserconfigwidget.h"
#include "dirdelegate.h"
#include "dirproxymodel.h"
#include "dirtree.h"
#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTreeView>
#include <QUndoCommand>

using namespace Qt::StringLiterals;

// Settings keys
constexpr auto DirBrowserIconsKey           = u"DirectoryBrowser/Icons";
constexpr auto DirBrowserDoubleClickKey     = u"DirectoryBrowser/DoubleClickBehaviour";
constexpr auto DirBrowserMiddleClickKey     = u"DirectoryBrowser/MiddleClickBehaviour";
constexpr auto DirBrowserModeKey            = u"DirectoryBrowser/Mode";
constexpr auto DirBrowserListIndentKey      = u"DirectoryBrowser/IndentList";
constexpr auto DirBrowserControlsKey        = u"DirectoryBrowser/Controls";
constexpr auto DirBrowserLocationKey        = u"DirectoryBrowser/LocationBar";
constexpr auto DirBrowserShowSymLinksKey    = u"DirectoryBrowser/SymLinks";
constexpr auto DirBrowserShowHiddenKey      = u"DirectoryBrowser/Hidden";
constexpr auto DirBrowserSendPlaybackKey    = u"DirectoryBrowser/StartPlaybackOnSend";
constexpr auto DirBrowserShowHorizScrollKey = u"DirectoryBrowser/ShowHorizontalScrollbar";

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
        saveSelectedRow(m_oldState);

        m_newState.path = newPath;
    }

    [[nodiscard]] QString undoPath() const
    {
        return m_oldState.path;
    }

    void undo() override
    {
        m_newState.scrollPos = m_view->verticalScrollBar()->value();
        saveSelectedRow(m_newState);

        QObject::connect(
            m_browser, &Fooyin::DirBrowser::rootChanged, m_browser,
            [this]() {
                m_view->verticalScrollBar()->setValue(m_oldState.scrollPos);
                restoreSelectedRow(m_oldState);
            },
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

        m_view->setUpdatesEnabled(false);
        m_browser->updateDir(m_oldState.path);
    }

    void redo() override
    {
        if(m_newState.scrollPos >= 0) {
            QObject::connect(
                m_browser, &Fooyin::DirBrowser::rootChanged, m_browser,
                [this]() {
                    m_view->verticalScrollBar()->setValue(m_newState.scrollPos);
                    restoreSelectedRow(m_newState);
                },
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
        int selectedRow{-1};
    };

    void saveSelectedRow(State& state) const
    {
        const auto selected = m_view->selectionModel()->selectedRows();

        if(!selected.empty()) {
            state.selectedRow = selected.front().row();
        }
    }

    void restoreSelectedRow(const State& state)
    {
        if(state.selectedRow >= 0) {
            const auto index = m_view->model()->index(state.selectedRow, 0, {});
            if(index.isValid()) {
                m_view->setCurrentIndex(index);
            }
        }
    }

    Fooyin::DirBrowser* m_browser;
    QAbstractItemView* m_view;

    State m_oldState;
    State m_newState;
};
} // namespace

namespace Fooyin {
DirBrowser::DirBrowser(const QStringList& supportedExtensions, ActionManager* actionManager,
                       PlaylistInteractor* playlistInteractor, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_supportedExtensions{Utils::extensionsToWildcards(supportedExtensions)}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistHandler{m_playlistInteractor->handler()}
    , m_settings{settings}
    , m_controlLayout{new QHBoxLayout()}
    , m_setup{false}
    , m_mode{Mode::List}
    , m_dirTree{new DirTree(this)}
    , m_model{new QFileSystemModel(this)}
    , m_proxyModel{new DirProxyModel(true, this)}
    , m_defaultFilters{QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot}
    , m_showSymLinks{false}
    , m_showHidden{false}
    , m_sendPlayback{true}
    , m_listIndent{true}
    , m_playlist{nullptr}
    , m_doubleClickAction{TrackAction::Play}
    , m_middleClickAction{TrackAction::None}
    , m_context{new WidgetContext(
          this, Context{Id{"Fooyin.Context.DirBrowser."}.append(reinterpret_cast<uintptr_t>(this))}, this)}
    , m_goUp{new QAction(Utils::iconFromTheme(Constants::Icons::Up), tr("Go up"), this)}
    , m_goBack{new QAction(Utils::iconFromTheme(Constants::Icons::GoPrevious), tr("Go back"), this)}
    , m_goForward{new QAction(Utils::iconFromTheme(Constants::Icons::GoNext), tr("Go forwards"), this)}
    , m_playAction{new QAction(tr("&Play"), this)}
    , m_addCurrent{new QAction(tr("Add to &current playlist"), this)}
    , m_addActive{new QAction(tr("Add to &active playlist"), this)}
    , m_sendCurrent{new QAction(tr("&Replace current playlist"), this)}
    , m_sendNew{new QAction(tr("Create &new playlist"), this)}
    , m_addQueue{new QAction(tr("Add to playback &queue"), this)}
    , m_queueNext{new QAction(tr("Queue to play next"), this)}
    , m_sendQueue{new QAction(tr("Replace playback q&ueue"), this)}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addLayout(m_controlLayout);
    layout->addWidget(m_dirTree);

    checkIconProvider();

    updateFilters();
    m_model->setNameFilters(m_supportedExtensions);
    m_model->setNameFilterDisables(false);
    m_model->setReadOnly(true);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setIconsEnabled(true);

    m_dirTree->viewport()->installEventFilter(new ToolTipFilter(this));
    m_dirTree->setItemDelegate(new DirDelegate(this));
    m_dirTree->setModel(m_proxyModel);
    m_dirTree->setShowHorizontalScrollbar(true);

    updateIndent(true);
    m_actionManager->addContextObject(m_context);

    const QStringList browserCategory{tr("Directory Browser")};

    m_goUp->setStatusTip(tr("Go up to the parent directory"));
    auto* goUpCmd = m_actionManager->registerAction(m_goUp, "Directory Browser.GoUp", m_context->context());
    goUpCmd->setCategories(browserCategory);
    QObject::connect(m_goUp, &QAction::triggered, this, [this]() { goUp(); });

    m_goBack->setStatusTip(tr("Return to the previous directory"));
    auto* goBackCmd = m_actionManager->registerAction(m_goBack, "Directory Browser.GoBack", m_context->context());
    goBackCmd->setCategories(browserCategory);
    QObject::connect(m_goBack, &QAction::triggered, this, [this]() {
        if(m_dirHistory.canUndo()) {
            m_dirHistory.undo();
        }
    });

    m_goForward->setStatusTip(tr("Undo a Go->Back action"));
    auto* goForwardCmd
        = m_actionManager->registerAction(m_goForward, "Directory Browser.GoForward", m_context->context());
    goForwardCmd->setCategories(browserCategory);
    QObject::connect(m_goForward, &QAction::triggered, this, [this]() {
        if(m_dirHistory.canRedo()) {
            m_dirHistory.redo();
        }
    });

    const QStringList tracksCategory{tr("Tracks")};

    m_playAction->setStatusTip(tr("Start playback of the selected files"));
    QObject::connect(m_playAction, &QAction::triggered, this, [this]() { handleAction(TrackAction::Play, false); });

    m_addCurrent->setStatusTip(tr("Append selected tracks to the current playlist"));
    auto* addCurrentCmd
        = m_actionManager->registerAction(m_addCurrent, Constants::Actions::AddToCurrent, m_context->context());
    addCurrentCmd->setCategories(tracksCategory);
    QObject::connect(m_addCurrent, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::AddCurrentPlaylist, true); });

    m_addActive->setStatusTip(tr("Append selected tracks to the active playlist"));
    auto* addActiveCmd
        = m_actionManager->registerAction(m_addActive, Constants::Actions::AddToActive, m_context->context());
    addActiveCmd->setCategories(tracksCategory);
    QObject::connect(m_addActive, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::AddActivePlaylist, true); });

    m_sendCurrent->setStatusTip(tr("Replace contents of the current playlist with the selected tracks"));
    auto* sendCurrentCmd
        = m_actionManager->registerAction(m_sendCurrent, Constants::Actions::SendToCurrent, m_context->context());
    sendCurrentCmd->setCategories(tracksCategory);
    QObject::connect(m_sendCurrent, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::SendCurrentPlaylist, true); });

    m_sendNew->setStatusTip(tr("Create a new playlist containing the selected tracks"));
    auto* sendNewCmd = m_actionManager->registerAction(m_sendNew, Constants::Actions::SendToNew, m_context->context());
    sendNewCmd->setCategories(tracksCategory);
    QObject::connect(m_sendNew, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::SendNewPlaylist, true); });

    m_addQueue->setStatusTip(tr("Add the selected tracks to the playback queue"));
    auto* addQueueCmd
        = m_actionManager->registerAction(m_addQueue, Constants::Actions::AddToQueue, m_context->context());
    addQueueCmd->setCategories(tracksCategory);
    QObject::connect(m_addQueue, &QAction::triggered, this, [this]() { handleAction(TrackAction::AddToQueue, true); });

    m_queueNext->setStatusTip(tr("Add the selected tracks to the front of the playback queue"));
    auto* queueNextCmd
        = m_actionManager->registerAction(m_queueNext, Constants::Actions::QueueNext, m_context->context());
    queueNextCmd->setCategories(tracksCategory);
    QObject::connect(m_queueNext, &QAction::triggered, this, [this]() { handleAction(TrackAction::QueueNext, true); });

    m_sendQueue->setStatusTip(tr("Replace the playback queue with the selected tracks"));
    auto* sendQueue
        = m_actionManager->registerAction(m_sendQueue, Constants::Actions::SendToQueue, m_context->context());
    sendQueue->setCategories(tracksCategory);
    QObject::connect(m_sendQueue, &QAction::triggered, this,
                     [this]() { handleAction(TrackAction::SendToQueue, true); });

    QObject::connect(m_dirTree, &QTreeView::doubleClicked, this,
                     [this](const QModelIndex& index) { handleDoubleClick(index); });
    QObject::connect(m_dirTree, &DirTree::middleClicked, this, [this]() { handleMiddleClick(); });
    QObject::connect(m_dirTree, &DirTree::backClicked, this, [this]() {
        if(m_dirHistory.canUndo()) {
            m_dirHistory.undo();
        }
    });
    QObject::connect(m_dirTree, &DirTree::forwardClicked, this, [this]() {
        if(m_dirHistory.canRedo()) {
            m_dirHistory.redo();
        }
    });

    QObject::connect(m_model, &QAbstractItemModel::layoutChanged, this, [this]() { handleModelUpdated(); });
    QObject::connect(
        m_proxyModel, &QAbstractItemModel::modelReset, this, [this]() { handleModelReset(); }, Qt::QueuedConnection);

    settings->subscribe<Settings::Gui::Theme>(m_proxyModel, &DirProxyModel::resetPalette);
    settings->subscribe<Settings::Gui::Style>(m_proxyModel, &DirProxyModel::resetPalette);

    m_config = defaultConfig();
    applyConfig(m_config);
}

void DirBrowser::checkIconProvider()
{
    auto* modelProvider = m_model->iconProvider();
    if(!modelProvider || modelProvider->icon(QFileIconProvider::Folder).isNull()
       || modelProvider->icon(QFileIconProvider::File).isNull()) {
        m_iconProvider = std::make_unique<QFileIconProvider>();
        m_model->setIconProvider(m_iconProvider.get());
    }
}

void DirBrowser::handleModelUpdated() const
{
    if(m_mode == DirBrowser::Mode::List) {
        const QModelIndex root = m_model->setRootPath(m_model->rootPath());
        m_dirTree->setRootIndex(m_proxyModel->mapFromSource(root));
        m_proxyModel->reset(root);
    }

    updateControlState();
    m_dirTree->setUpdatesEnabled(true);
}

QueueTracks DirBrowser::loadQueueTracks(const TrackList& tracks) const
{
    QueueTracks queueTracks;

    UId playlistId;
    if(m_playlist) {
        playlistId = m_playlist->id();
    }

    for(const Track& track : tracks) {
        queueTracks.emplace_back(track, playlistId);
    }

    return queueTracks;
}

void DirBrowser::handleAction(TrackAction action, bool onlySelection)
{
    QModelIndexList selected = m_dirTree->selectionModel()->selectedRows();

    if(selected.empty()) {
        return;
    }

    QString firstPath;

    if(selected.size() == 1) {
        const QModelIndex index = selected.front();
        if(index.isValid()) {
            const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
            if(!onlySelection && filePath.isFile()) {
                // Add all files in same directory
                selected  = {m_proxyModel->mapToSource(selected.front()).parent()};
                firstPath = filePath.absoluteFilePath();
            }
        }
    }

    QList<QUrl> files;

    for(const QModelIndex& index : std::as_const(selected)) {
        if(index.isValid()) {
            const QFileInfo filePath{index.data(QFileSystemModel::FilePathRole).toString()};
            if(filePath.isDir()) {
                if(!onlySelection) {
                    files.append(Utils::File::getUrlsInDir(filePath.absoluteFilePath(), m_supportedExtensions));
                }
                else {
                    files.append(
                        Utils::File::getUrlsInDirRecursive(filePath.absoluteFilePath(), m_supportedExtensions));
                }
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

    const bool startPlayback = m_sendPlayback;

    switch(action) {
        case(TrackAction::Play):
            handlePlayAction(files, firstPath);
            break;
        case(TrackAction::AddCurrentPlaylist):
            m_playlistInteractor->filesToCurrentPlaylist(files);
            break;
        case(TrackAction::SendCurrentPlaylist):
            m_playlistInteractor->filesToCurrentPlaylistReplace(files, startPlayback);
            break;
        case(TrackAction::SendNewPlaylist):
            m_playlistInteractor->filesToNewPlaylist(playlistName, files, startPlayback);
            break;
        case(TrackAction::AddActivePlaylist):
            m_playlistInteractor->filesToActivePlaylist(files);
            break;
        case(TrackAction::AddToQueue):
            m_playlistInteractor->filesToTracks(files, [this](const TrackList& tracks) {
                m_playlistInteractor->playerController()->queueTracks(loadQueueTracks(tracks));
            });
            break;
        case(TrackAction::QueueNext):
            m_playlistInteractor->filesToTracks(files, [this](const TrackList& tracks) {
                m_playlistInteractor->playerController()->queueTracksNext(loadQueueTracks(tracks));
            });
            break;
        case(TrackAction::SendToQueue):
            m_playlistInteractor->filesToTracks(files, [this](const TrackList& tracks) {
                m_playlistInteractor->playerController()->replaceTracks(loadQueueTracks(tracks));
            });
            break;
        case(TrackAction::None):
            break;
    }
}

void DirBrowser::handlePlayAction(const QList<QUrl>& files, const QString& startingFile)
{
    int playIndex{0};

    if(!startingFile.isEmpty()) {
        auto rowIt = std::ranges::find_if(std::as_const(files),
                                          [&startingFile](const QUrl& file) { return file.path() == startingFile; });
        if(rowIt != files.cend()) {
            playIndex = static_cast<int>(std::distance(files.cbegin(), rowIt));
        }
    }

    TrackList tracks;
    std::ranges::transform(files, std::back_inserter(tracks),
                           [](const QUrl& file) { return Track{file.toLocalFile()}; });

    startPlayback(tracks, playIndex);
}

void DirBrowser::handleDoubleClick(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    const QString path = index.data(QFileSystemModel::FilePathRole).toString();

    if(path.isEmpty() && m_mode == DirBrowser::Mode::List) {
        goUp();
        return;
    }

    const QFileInfo filePath{path};
    if(filePath.isDir()) {
        if(m_mode == DirBrowser::Mode::List) {
            changeRoot(filePath.absoluteFilePath());
        }
        return;
    }

    handleAction(m_doubleClickAction, m_doubleClickAction != TrackAction::Play);
}

void DirBrowser::handleMiddleClick()
{
    handleAction(m_middleClickAction, true);
}

void DirBrowser::handleModelReset()
{
    if(!m_setup) {
        m_setup = true;

        QString rootPath = m_rootPath;
        if(rootPath.isEmpty()) {
            rootPath = QDir::homePath();
        }

        updateDir(rootPath);
    }

    emit this->rootChanged();
    m_dirTree->selectionModel()->setCurrentIndex(m_proxyModel->index(0, 0, {}), QItemSelectionModel::NoUpdate);
    m_dirTree->resizeView();
}

void DirBrowser::updateDir(const QString& dir)
{
    const QModelIndex root = m_model->setRootPath(dir);
    m_rootPath             = m_model->rootPath();
    m_dirTree->setRootIndex(m_proxyModel->mapFromSource(root));

    if(m_dirEdit) {
        m_dirEdit->setText(m_rootPath);
    }

    if(m_playlist) {
        m_proxyModel->setPlayingPath(m_playlist->currentTrack().filepath());
    }
}

void DirBrowser::changeRoot(const QString& root)
{
    if(root.isEmpty() || !QFileInfo::exists(root)) {
        return;
    }

    if(QDir{root} == QDir{m_model->rootPath()}) {
        return;
    }

    auto* changeDir = new DirChange(this, m_dirTree, m_model->rootPath(), root);
    m_dirHistory.push(changeDir);
}

void DirBrowser::updateIndent(bool show)
{
    m_listIndent = show;
    if(show || m_mode == DirBrowser::Mode::Tree) {
        m_dirTree->resetIndentation();
    }
    else {
        m_dirTree->setIndentation(0);
    }
}

void DirBrowser::setDoubleClickAction(const int action)
{
    m_doubleClickAction = static_cast<TrackAction>(action);
}

void DirBrowser::setMiddleClickAction(const int action)
{
    m_middleClickAction = static_cast<TrackAction>(action);
}

void DirBrowser::setSendPlayback(const bool enabled)
{
    m_sendPlayback = enabled;
}

void DirBrowser::setShowIconsEnabled(const bool enabled)
{
    m_proxyModel->setIconsEnabled(enabled);
}

void DirBrowser::setListIndentEnabled(const bool enabled)
{
    updateIndent(enabled);
}

void DirBrowser::setShowHorizontalScrollbar(const bool enabled)
{
    m_dirTree->setShowHorizontalScrollbar(enabled);
}

void DirBrowser::setRootPath(const QString& rootPath)
{
    QString path = rootPath;
    if(path.isEmpty() || !QFileInfo::exists(path)) {
        path = QDir::homePath();
    }

    m_rootPath = path;

    if(m_setup) {
        changeRoot(m_rootPath);
    }
}

QString DirBrowser::rootPath() const
{
    return m_model->rootPath();
}

void DirBrowser::updateFilters()
{
    QFlags<QDir::Filter> newFilters = m_defaultFilters;

    if(!m_showSymLinks) {
        newFilters |= QDir::NoSymLinks;
    }
    if(m_showHidden) {
        newFilters |= QDir::Hidden;
    }

    m_model->setFilter(newFilters);
}

void DirBrowser::setControlsEnabled(bool enabled)
{
    if(enabled && !m_upDir && !m_backDir && !m_forwardDir) {
        m_upDir      = new ToolButton(this);
        m_backDir    = new ToolButton(this);
        m_forwardDir = new ToolButton(this);

        m_upDir->setDefaultAction(m_goUp);
        m_backDir->setDefaultAction(m_goBack);
        m_forwardDir->setDefaultAction(m_goForward);

        m_controlLayout->insertWidget(0, m_upDir);
        m_controlLayout->insertWidget(0, m_forwardDir);
        m_controlLayout->insertWidget(0, m_backDir);
    }
    else {
        if(m_backDir) {
            m_backDir->deleteLater();
        }
        if(m_forwardDir) {
            m_forwardDir->deleteLater();
        }
        if(m_upDir) {
            m_upDir->deleteLater();
        }
    }
}

void DirBrowser::setLocationEnabled(bool enabled)
{
    if(enabled && !m_dirEdit) {
        m_dirEdit = new QLineEdit(this);
        QObject::connect(m_dirEdit, &QLineEdit::textEdited, this, [this](const QString& dir) { changeRoot(dir); });
        m_controlLayout->addWidget(m_dirEdit, 1);
        m_dirEdit->setText(m_model->rootPath());
    }
    else {
        if(m_dirEdit) {
            m_dirEdit->deleteLater();
        }
    }
}

void DirBrowser::setShowSymLinksEnabled(bool enabled)
{
    m_showSymLinks = enabled;
    updateFilters();
}

void DirBrowser::setShowHidden(bool enabled)
{
    m_showHidden = enabled;
    updateFilters();
}

void DirBrowser::changeMode(DirBrowser::Mode newMode)
{
    m_mode = newMode;

    const QString rootPath = m_model->rootPath();

    m_proxyModel->setFlat(m_mode == DirBrowser::Mode::List);

    const QModelIndex root = m_model->setRootPath(rootPath);
    m_dirTree->setRootIndex(m_proxyModel->mapFromSource(root));

    updateIndent(m_listIndent);
}

void DirBrowser::startPlayback(const TrackList& tracks, int row)
{
    if(!m_playlist) {
        m_playlist = m_playlistHandler->createTempPlaylist(tempPlaylistName());
        if(!m_playlist) {
            return;
        }
    }

    m_playlistHandler->replacePlaylistTracks(m_playlist->id(), tracks);

    m_playlist->changeCurrentIndex(row);
    m_playlistInteractor->playerController()->startPlayback(m_playlist);
}

void DirBrowser::updateControlState() const
{
    if(m_upDir) {
        m_upDir->setEnabled(m_proxyModel->canGoUp());
    }
    if(m_backDir) {
        m_backDir->setEnabled(m_dirHistory.canUndo());
    }
    if(m_forwardDir) {
        m_forwardDir->setEnabled(m_dirHistory.canRedo());
    }
}

void DirBrowser::goUp()
{
    QDir root{m_model->rootPath()};

    if(!root.cdUp()) {
        return;
    }

    const QString newPath = root.absolutePath();

    if(m_dirHistory.canUndo()) {
        if(const auto* prevDir = static_cast<const DirChange*>(m_dirHistory.command(m_dirHistory.index() - 1))) {
            if(prevDir->undoPath() == newPath) {
                m_dirHistory.undo();
                return;
            }
        }
    }

    auto* changeDir = new DirChange(this, m_dirTree, m_model->rootPath(), newPath);
    m_dirHistory.push(changeDir);
}

QString DirBrowser::tempPlaylistName() const
{
    return u"␟DirBrowserPlaylist.%1␟"_s.arg(id().name());
}

DirBrowser::~DirBrowser() = default;

QString DirBrowser::name() const
{
    return tr("Directory Browser");
}

QString DirBrowser::layoutName() const
{
    return u"DirectoryBrowser"_s;
}

void DirBrowser::saveLayoutData(QJsonObject& layout)
{
    auto config     = m_config;
    config.rootPath = rootPath();
    saveConfigToLayout(config, layout);
}

void DirBrowser::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

DirBrowser::ConfigData DirBrowser::defaultConfig() const
{
    auto config{factoryConfig()};

    config.doubleClickAction  = m_settings->fileValue(DirBrowserDoubleClickKey, config.doubleClickAction).toInt();
    config.middleClickAction  = m_settings->fileValue(DirBrowserMiddleClickKey, config.middleClickAction).toInt();
    config.sendPlayback       = m_settings->fileValue(DirBrowserSendPlaybackKey, config.sendPlayback).toBool();
    config.showIcons          = m_settings->fileValue(DirBrowserIconsKey, config.showIcons).toBool();
    config.indentList         = m_settings->fileValue(DirBrowserListIndentKey, config.indentList).toBool();
    config.showHorizScrollbar = m_settings->fileValue(DirBrowserShowHorizScrollKey, config.showHorizScrollbar).toBool();
    config.mode = static_cast<Mode>(m_settings->fileValue(DirBrowserModeKey, static_cast<int>(config.mode)).toInt());
    config.showControls = m_settings->fileValue(DirBrowserControlsKey, config.showControls).toBool();
    config.showLocation = m_settings->fileValue(DirBrowserLocationKey, config.showLocation).toBool();
    config.showSymLinks = m_settings->fileValue(DirBrowserShowSymLinksKey, config.showSymLinks).toBool();
    config.showHidden   = m_settings->fileValue(DirBrowserShowHiddenKey, config.showHidden).toBool();

    return config;
}

DirBrowser::ConfigData DirBrowser::factoryConfig() const
{
    return {
        .doubleClickAction  = 5,
        .middleClickAction  = 0,
        .sendPlayback       = true,
        .showIcons          = true,
        .indentList         = true,
        .showHorizScrollbar = true,
        .mode               = Mode::List,
        .showControls       = true,
        .showLocation       = true,
        .showSymLinks       = false,
        .showHidden         = false,
        .rootPath           = QDir::homePath(),
    };
}

const DirBrowser::ConfigData& DirBrowser::currentConfig() const
{
    return m_config;
}

void DirBrowser::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(DirBrowserDoubleClickKey, config.doubleClickAction);
    m_settings->fileSet(DirBrowserMiddleClickKey, config.middleClickAction);
    m_settings->fileSet(DirBrowserSendPlaybackKey, config.sendPlayback);
    m_settings->fileSet(DirBrowserIconsKey, config.showIcons);
    m_settings->fileSet(DirBrowserListIndentKey, config.indentList);
    m_settings->fileSet(DirBrowserShowHorizScrollKey, config.showHorizScrollbar);
    m_settings->fileSet(DirBrowserModeKey, static_cast<int>(config.mode));
    m_settings->fileSet(DirBrowserControlsKey, config.showControls);
    m_settings->fileSet(DirBrowserLocationKey, config.showLocation);
    m_settings->fileSet(DirBrowserShowSymLinksKey, config.showSymLinks);
    m_settings->fileSet(DirBrowserShowHiddenKey, config.showHidden);
}

void DirBrowser::clearSavedDefaults() const
{
    m_settings->fileRemove(DirBrowserDoubleClickKey);
    m_settings->fileRemove(DirBrowserMiddleClickKey);
    m_settings->fileRemove(DirBrowserSendPlaybackKey);
    m_settings->fileRemove(DirBrowserIconsKey);
    m_settings->fileRemove(DirBrowserListIndentKey);
    m_settings->fileRemove(DirBrowserShowHorizScrollKey);
    m_settings->fileRemove(DirBrowserModeKey);
    m_settings->fileRemove(DirBrowserControlsKey);
    m_settings->fileRemove(DirBrowserLocationKey);
    m_settings->fileRemove(DirBrowserShowSymLinksKey);
    m_settings->fileRemove(DirBrowserShowHiddenKey);
}

void DirBrowser::applyConfig(const ConfigData& config)
{
    m_config = config;

    setDoubleClickAction(m_config.doubleClickAction);
    setMiddleClickAction(m_config.middleClickAction);
    setSendPlayback(m_config.sendPlayback);
    setShowIconsEnabled(m_config.showIcons);
    setListIndentEnabled(m_config.indentList);
    setShowHorizontalScrollbar(m_config.showHorizScrollbar);
    changeMode(m_config.mode);
    setControlsEnabled(m_config.showControls);
    setLocationEnabled(m_config.showLocation);
    setShowSymLinksEnabled(m_config.showSymLinks);
    setShowHidden(m_config.showHidden);
    setRootPath(m_config.rootPath);
    updateControlState();
}

void DirBrowser::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["DoubleClickAction"_L1]       = config.doubleClickAction;
    layout["MiddleClickAction"_L1]       = config.middleClickAction;
    layout["SendPlayback"_L1]            = config.sendPlayback;
    layout["ShowIcons"_L1]               = config.showIcons;
    layout["IndentList"_L1]              = config.indentList;
    layout["ShowHorizontalScrollbar"_L1] = config.showHorizScrollbar;
    layout["Mode"_L1]                    = static_cast<int>(config.mode);
    layout["ShowControls"_L1]            = config.showControls;
    layout["ShowLocation"_L1]            = config.showLocation;
    layout["ShowSymLinks"_L1]            = config.showSymLinks;
    layout["ShowHidden"_L1]              = config.showHidden;
    layout["RootPath"_L1]                = config.rootPath;
}

DirBrowser::ConfigData DirBrowser::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("DoubleClickAction"_L1)) {
        config.doubleClickAction = layout.value("DoubleClickAction"_L1).toInt();
    }
    if(layout.contains("MiddleClickAction"_L1)) {
        config.middleClickAction = layout.value("MiddleClickAction"_L1).toInt();
    }
    if(layout.contains("SendPlayback"_L1)) {
        config.sendPlayback = layout.value("SendPlayback"_L1).toBool();
    }
    if(layout.contains("ShowIcons"_L1)) {
        config.showIcons = layout.value("ShowIcons"_L1).toBool();
    }
    if(layout.contains("IndentList"_L1)) {
        config.indentList = layout.value("IndentList"_L1).toBool();
    }
    if(layout.contains("ShowHorizontalScrollbar"_L1)) {
        config.showHorizScrollbar = layout.value("ShowHorizontalScrollbar"_L1).toBool();
    }
    if(layout.contains("Mode"_L1)) {
        const int mode = layout.value("Mode"_L1).toInt();
        if(mode == static_cast<int>(Mode::Tree) || mode == static_cast<int>(Mode::List)) {
            config.mode = static_cast<Mode>(mode);
        }
    }
    if(layout.contains("ShowControls"_L1)) {
        config.showControls = layout.value("ShowControls"_L1).toBool();
    }
    if(layout.contains("ShowLocation"_L1)) {
        config.showLocation = layout.value("ShowLocation"_L1).toBool();
    }
    if(layout.contains("ShowSymLinks"_L1)) {
        config.showSymLinks = layout.value("ShowSymLinks"_L1).toBool();
    }
    if(layout.contains("ShowHidden"_L1)) {
        config.showHidden = layout.value("ShowHidden"_L1).toBool();
    }
    if(layout.contains("RootPath"_L1)) {
        config.rootPath = layout.value("RootPath"_L1).toString();
    }

    if(config.rootPath.isEmpty()) {
        config.rootPath = QDir::homePath();
    }

    return config;
}

void DirBrowser::openConfigDialog()
{
    showConfigDialog(new DirBrowserConfigDialog(this, this));
}

void DirBrowser::playstateChanged(Player::PlayState state)
{
    m_proxyModel->setPlayState(state);
}

void DirBrowser::activePlaylistChanged(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    if(!m_playlist) {
        m_playlist = m_playlistHandler->playlistByName(tempPlaylistName());
    }

    if(!playlist || !m_playlist || (playlist && playlist->id() != m_playlist->id())) {
        m_proxyModel->setPlayingPath({});
    }
}

void DirBrowser::playlistTrackChanged(const PlaylistTrack& track)
{
    if(m_playlist && m_playlist->id() == track.playlistId) {
        m_proxyModel->setPlayingPath(track.track.filepath());
    }
}

void DirBrowser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(m_playAction);
    menu->addSeparator();
    menu->addAction(m_addCurrent);
    menu->addAction(m_addActive);
    menu->addAction(m_sendCurrent);
    menu->addAction(m_sendNew);
    menu->addSeparator();
    menu->addAction(m_addQueue);
    menu->addAction(m_queueNext);
    menu->addAction(m_sendQueue);
    menu->addSeparator();

    const QModelIndex index = m_dirTree->indexAt(m_dirTree->mapFromGlobal(event->globalPos()));

    if(index.isValid()) {
        const QFileInfo selectedPath{index.data(QFileSystemModel::FilePathRole).toString()};
        if(selectedPath.isDir()) {
            const QString dir = index.data(QFileSystemModel::FilePathRole).toString();
            auto* setRoot     = new QAction(tr("Set as root"), menu);
            QObject::connect(setRoot, &QAction::triggered, this, [this, dir]() { changeRoot(dir); });
            menu->addAction(setRoot);
        }
    }

    addConfigureAction(menu);

    menu->popup(event->globalPos());
}

void DirBrowser::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        const auto indexes = m_dirTree->selectionModel()->selectedRows();
        if(!indexes.empty()) {
            handleDoubleClick(indexes.front());
        }
    }
    else if(key == Qt::Key_Backspace) {
        goUp();
    }

    QWidget::keyPressEvent(event);
}
} // namespace Fooyin

#include "moc_dirbrowser.cpp"
