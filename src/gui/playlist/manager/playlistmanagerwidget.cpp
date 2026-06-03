/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "playlistmanagerwidget.h"

#include "dialog/autoplaylistdialog.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlistmanagermodel.h"

#include <core/coresettings.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/expandedtreeview.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QAction>
#include <QCloseEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

constexpr auto PlaylistManagerState = "PlaylistManager/State"_L1;

namespace Fooyin {
namespace {
TrackSelection selectionForPlaylist(const Playlist* playlist)
{
    TrackSelection selection;
    if(!playlist || playlist->trackCount() <= 0) {
        return selection;
    }

    selection.tracks         = playlist->tracks();
    selection.playlistId     = playlist->id();
    selection.playlistBacked = true;

    const auto playlistTracks = playlist->playlistTracks();
    selection.playlistIndexes.reserve(playlistTracks.size());
    selection.playlistEntryIds.reserve(playlistTracks.size());

    for(const PlaylistTrack& track : playlistTracks) {
        selection.playlistIndexes.emplace_back(track.indexInPlaylist);
        selection.playlistEntryIds.emplace_back(track.entryId);
    }

    return selection;
}
} // namespace

PlaylistManagerWidget::PlaylistManagerWidget(ActionManager* actionManager, PlaylistController* playlistController,
                                             PlaylistInteractor* playlistInteractor, SettingsManager* settings,
                                             QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistController{playlistController}
    , m_settings{settings}
    , m_model{new PlaylistManagerModel(playlistInteractor, this)}
    , m_proxyModel{new QSortFilterProxyModel(this)}
    , m_view{new ExpandedTreeView(this)}
    , m_context{new WidgetContext(this, Context{IdList{Id{"Fooyin.Context.PlaylistManager."}.append(id())}}, this)}
    , m_activateAction{new QAction(tr("Activate"), this)}
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
    , m_activateOnSingleClick{false}
    , m_selectingPlaylist{false}
    , m_topLevelStateLoaded{false}
{
    setObjectName(name());
    setWindowTitle(name());

    m_actionManager->addContextObject(m_context);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortRole(PlaylistManagerModel::SortRole);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setModel(m_proxyModel);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSortingEnabled(true);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_view->setDragDropMode(QAbstractItemView::DropOnly);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropOverwriteMode(true);
    m_view->setDefaultDropAction(Qt::CopyAction);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* header = m_view->header();
    header->setStretchLastSection(false);
    header->setSectionsMovable(true);
    header->setHighlightSections(false);
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    header->setSectionResizeMode(PlaylistManagerModel::Name, QHeaderView::Stretch);
    header->setSectionResizeMode(PlaylistManagerModel::Tracks, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(PlaylistManagerModel::Duration, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(PlaylistManagerModel::TotalSize, QHeaderView::ResizeToContents);

    for(const auto column : {PlaylistManagerModel::Duration, PlaylistManagerModel::TotalSize}) {
        header->setSectionHidden(column, true);
    }

    resetToPlaylistIndexOrder();
    setupActions();

    QObject::connect(m_view, &QWidget::customContextMenuRequested, this,
                     &PlaylistManagerWidget::showPlaylistContextMenu);
    QObject::connect(header, &QWidget::customContextMenuRequested, this, &PlaylistManagerWidget::showHeaderContextMenu);
    QObject::connect(m_view, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        if(m_activateOnSingleClick) {
            activatePlaylist(index);
        }
    });
    QObject::connect(m_view, &QAbstractItemView::doubleClicked, this, &PlaylistManagerWidget::activatePlaylist);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &PlaylistManagerWidget::selectCurrentPlaylist);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, &PlaylistManagerWidget::selectCurrentPlaylist);
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, &PlaylistManagerWidget::selectCurrentPlaylist);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PlaylistManagerWidget::updateActionState);

    updateActionState();
}

QString PlaylistManagerWidget::name() const
{
    return tr("Playlist Manager");
}

QString PlaylistManagerWidget::layoutName() const
{
    return u"PlaylistManager"_s;
}

void PlaylistManagerWidget::saveLayoutData(QJsonObject& layout)
{
    layout["State"_L1]                 = QString::fromUtf8(saveHeaderState().toBase64());
    layout["ActivateOnSingleClick"_L1] = m_activateOnSingleClick;
}

void PlaylistManagerWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("State"_L1)) {
        m_view->header()->restoreState(QByteArray::fromBase64(layout.value("State"_L1).toString().toUtf8()));
    }

    resetToPlaylistIndexOrder();

    if(layout.contains("ActivateOnSingleClick"_L1)) {
        m_activateOnSingleClick = layout.value("ActivateOnSingleClick"_L1).toBool();
    }
}

void PlaylistManagerWidget::finalise()
{
    selectCurrentPlaylist();
}

QByteArray PlaylistManagerWidget::saveHeaderState() const
{
    auto* header = m_view->header();
    if(!header) {
        return {};
    }

    const int sortSection = header->sortIndicatorSection();
    const auto sortOrder  = header->sortIndicatorOrder();

    const QSignalBlocker block{header};
    header->setSortIndicator(-1, Qt::AscendingOrder);

    const QByteArray state = header->saveState();
    header->setSortIndicator(sortSection, sortOrder);
    return state;
}

void PlaylistManagerWidget::resetToPlaylistIndexOrder()
{
    m_proxyModel->sort(-1);
    m_view->header()->setSortIndicator(-1, Qt::AscendingOrder);
}

QSize PlaylistManagerWidget::sizeHint() const
{
    return {720, 420};
}

void PlaylistManagerWidget::showEvent(QShowEvent* event)
{
    if(isWindowWidget() && !m_topLevelStateLoaded) {
        loadTopLevelState();
    }

    FyWidget::showEvent(event);
}

void PlaylistManagerWidget::closeEvent(QCloseEvent* event)
{
    if(isWindowWidget()) {
        saveTopLevelState();
    }

    FyWidget::closeEvent(event);
}

void PlaylistManagerWidget::addPlaylistContentsMenu(QMenu* menu, Playlist* playlist)
{
    if(!menu || !playlist) {
        return;
    }

    const TrackSelection selection = selectionForPlaylist(playlist);
    if(selection.tracks.empty()) {
        return;
    }

    //: %1 refers to the name of a playlist.
    auto* selectionMenu = new QMenu(tr("%1 contents").arg(playlist->name()), menu);
    m_playlistController->selectionController()->addTrackContextMenu(selectionMenu, selection);
    if(selectionMenu->isEmpty()) {
        delete selectionMenu;
        return;
    }

    menu->addMenu(selectionMenu);
}

void PlaylistManagerWidget::setupActions()
{
    const QStringList editCategory{tr("Edit")};
    const QStringList playlistManagerCategory{tr("Playlist Manager")};

    m_activateAction->setStatusTip(tr("Activate the selected playlist"));
    m_activateAction->setShortcut(QKeySequence(Qt::Key_Return));
    m_activateAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_activateAction->setShortcutVisibleInContextMenu(true);
    addAction(m_activateAction);
    QObject::connect(m_activateAction, &QAction::triggered, this, &PlaylistManagerWidget::activateCurrentPlaylist);

    m_editAutoPlaylistAction->setStatusTip(tr("Edit the selected autoplaylist"));
    m_editAutoPlaylistCmd->setCategories(playlistManagerCategory);
    QObject::connect(m_editAutoPlaylistAction, &QAction::triggered, this,
                     &PlaylistManagerWidget::editCurrentAutoPlaylist);

    m_renameAction->setStatusTip(tr("Rename the selected playlist"));
    m_renameAction->setShortcutVisibleInContextMenu(true);
    m_renameCmd->setCategories(editCategory);
    m_renameCmd->setDescription(tr("Rename"));
    m_renameCmd->setAttribute(ProxyAction::UpdateText);
    m_renameCmd->setDefaultShortcut(Qt::Key_F2);
    QObject::connect(m_renameAction, &QAction::triggered, this, &PlaylistManagerWidget::showRenameEditor);

    m_removeAction->setStatusTip(tr("Remove the selected playlist"));
    m_removeAction->setShortcutVisibleInContextMenu(true);
    m_removeCmd->setCategories(editCategory);
    m_removeCmd->setDescription(tr("Remove"));
    m_removeCmd->setAttribute(ProxyAction::UpdateText);
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeAction, &QAction::triggered, this, &PlaylistManagerWidget::removeCurrentPlaylist);

    m_newPlaylistAction->setStatusTip(tr("Create a new empty playlist"));
    m_newPlaylistAction->setShortcutVisibleInContextMenu(true);
    QObject::connect(m_newPlaylistAction, &QAction::triggered, this, [this]() {
        if(auto* newPlaylist = m_playlistController->playlistHandler()->createEmptyPlaylist()) {
            m_playlistController->changeCurrentPlaylist(newPlaylist);
        }
    });

    m_newAutoPlaylistAction->setStatusTip(tr("Create a new autoplaylist"));
    QObject::connect(m_newAutoPlaylistAction, &QAction::triggered, this, [this]() {
        auto* autoDialog = new AutoPlaylistDialog(Utils::getMainWindow());
        autoDialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(autoDialog, &AutoPlaylistDialog::playlistEdited, autoDialog,
                         [this](const QString& name, const QString& query, const QString& sortQuery, bool forceSorted) {
                             if(auto* newPlaylist = m_playlistController->playlistHandler()->createNewAutoPlaylist(
                                    name, query, sortQuery, forceSorted)) {
                                 m_playlistController->changeCurrentPlaylist(newPlaylist);
                             }
                         });
        autoDialog->show();
    });
}

void PlaylistManagerWidget::updateActionState()
{
    Playlist* playlist{nullptr};

    const QModelIndex proxyIndex = m_view->currentIndex();
    if(proxyIndex.isValid()) {
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        playlist                      = m_model->playlistForRow(sourceIndex.row());
    }

    const bool hasPlaylist = playlist != nullptr;
    const bool isAuto      = hasPlaylist && playlist->isAutoPlaylist();

    if(m_activateAction) {
        m_activateAction->setEnabled(hasPlaylist);
    }
    if(m_editAutoPlaylistAction) {
        m_editAutoPlaylistAction->setEnabled(isAuto);
    }
    if(m_renameAction) {
        if(m_renameCmd) {
            m_renameAction->setShortcut(m_renameCmd->shortcut());
        }
        m_renameAction->setText(isAuto ? tr("Re&name autoplaylist") : tr("Re&name playlist"));
        m_renameAction->setEnabled(hasPlaylist);
    }
    if(m_removeAction) {
        if(m_removeCmd) {
            m_removeAction->setShortcut(m_removeCmd->shortcut());
        }
        m_removeAction->setText(isAuto ? tr("&Remove autoplaylist") : tr("&Remove playlist"));
        m_removeAction->setEnabled(hasPlaylist);
    }
    if(m_newPlaylistAction && m_newPlaylistCmd) {
        m_newPlaylistAction->setShortcut(m_newPlaylistCmd->shortcut());
    }
}

void PlaylistManagerWidget::activatePlaylist(const QModelIndex& proxyIndex) const
{
    if(!proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    if(auto* playlist = m_model->playlistForRow(sourceIndex.row())) {
        m_playlistController->changeCurrentPlaylist(playlist);
    }
}

void PlaylistManagerWidget::activateCurrentPlaylist()
{
    activatePlaylist(m_view->currentIndex());
}

void PlaylistManagerWidget::editCurrentAutoPlaylist()
{
    const QModelIndex proxyIndex = m_view->currentIndex();
    if(!proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    auto* playlist                = m_model->playlistForRow(sourceIndex.row());
    if(!playlist || !playlist->isAutoPlaylist()) {
        return;
    }

    auto* autoDialog
        = new AutoPlaylistDialog(m_playlistController->playlistHandler(), playlist, Utils::getMainWindow());
    autoDialog->setAttribute(Qt::WA_DeleteOnClose);
    autoDialog->show();
}

void PlaylistManagerWidget::removeCurrentPlaylist()
{
    const QModelIndex proxyIndex = m_view->currentIndex();
    if(!proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    if(auto* playlist = m_model->playlistForRow(sourceIndex.row())) {
        m_playlistController->playlistHandler()->removePlaylist(playlist->id());
    }
}

void PlaylistManagerWidget::showPlaylistContextMenu(const QPoint& pos)
{
    Playlist* playlist{nullptr};

    const QModelIndex proxyIndex = m_view->indexAt(pos);
    if(proxyIndex.isValid()) {
        m_view->selectionModel()->setCurrentIndex(proxyIndex,
                                                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        playlist                      = m_model->playlistForRow(sourceIndex.row());
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(playlist) {
        updateActionState();
        menu->addAction(m_activateAction);

        if(playlist->isAutoPlaylist()) {
            menu->addAction(m_editAutoPlaylistAction);
        }

        menu->addAction(m_renameAction);
        menu->addAction(m_removeAction);

        menu->addSeparator();
    }

    menu->addAction(m_newPlaylistAction);
    menu->addAction(m_newAutoPlaylistAction);

    if(playlist) {
        const TrackSelectionController* selectionController = m_playlistController->selectionController();
        if(selectionController && playlist->trackCount() > 0) {
            menu->addSeparator();
            addPlaylistContentsMenu(menu, playlist);
        }
    }

    menu->addSeparator();

    auto* optionsMenu = new QMenu(tr("Options"), menu);

    auto* activateOnSingleClick = new QAction(tr("Activate on single click"), optionsMenu);
    activateOnSingleClick->setCheckable(true);
    activateOnSingleClick->setChecked(m_activateOnSingleClick);
    QObject::connect(activateOnSingleClick, &QAction::toggled, this,
                     [this](bool enabled) { m_activateOnSingleClick = enabled; });
    optionsMenu->addAction(activateOnSingleClick);

    menu->addMenu(optionsMenu);

    menu->popup(m_view->viewport()->mapToGlobal(pos));
}

void PlaylistManagerWidget::showHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    for(const auto column :
        {PlaylistManagerModel::Tracks, PlaylistManagerModel::Duration, PlaylistManagerModel::TotalSize}) {
        auto* action = new QAction(m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString(), menu);
        action->setCheckable(true);
        action->setChecked(!m_view->header()->isSectionHidden(column));
        QObject::connect(action, &QAction::toggled, this,
                         [this, column](bool visible) { m_view->header()->setSectionHidden(column, !visible); });
        menu->addAction(action);
    }

    menu->addSeparator();

    auto* resetAction = new QAction(tr("Restore playlist order"), menu);
    QObject::connect(resetAction, &QAction::triggered, this, [this]() { resetToPlaylistIndexOrder(); });
    menu->addAction(resetAction);

    menu->popup(m_view->header()->viewport()->mapToGlobal(pos));
}

void PlaylistManagerWidget::showRenameEditor()
{
    const QModelIndex proxyIndex = m_view->currentIndex();
    if(!proxyIndex.isValid()) {
        return;
    }

    m_view->edit(proxyIndex.siblingAtColumn(PlaylistManagerModel::Name));
}

void PlaylistManagerWidget::selectCurrentPlaylist()
{
    auto* currentPlaylist         = m_playlistController->currentPlaylist();
    const QModelIndex sourceIndex = m_model->indexForPlaylist(currentPlaylist, PlaylistManagerModel::Name);
    if(!sourceIndex.isValid()) {
        return;
    }

    const QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    if(!proxyIndex.isValid()) {
        return;
    }

    m_selectingPlaylist = true;
    m_view->selectionModel()->setCurrentIndex(proxyIndex,
                                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_view->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    m_selectingPlaylist = false;
}

void PlaylistManagerWidget::saveTopLevelState()
{
    QJsonObject layoutData;
    saveLayoutData(layoutData);
    layoutData["Geometry"_L1] = QString::fromUtf8(saveGeometry().toBase64());

    FyStateSettings stateSettings;
    stateSettings.setValue(PlaylistManagerState, layoutData);
}

void PlaylistManagerWidget::loadTopLevelState()
{
    const FyStateSettings stateSettings;

    const QJsonObject layoutData = stateSettings.value(PlaylistManagerState).toJsonObject();
    if(layoutData.isEmpty()) {
        m_topLevelStateLoaded = true;
        return;
    }

    loadLayoutData(layoutData);

    if(layoutData.contains("Geometry"_L1)) {
        restoreGeometry(QByteArray::fromBase64(layoutData.value("Geometry"_L1).toString().toUtf8()));
    }

    m_topLevelStateLoaded = true;
}

bool PlaylistManagerWidget::isWindowWidget() const
{
    return parentWidget() == nullptr;
}
} // namespace Fooyin
