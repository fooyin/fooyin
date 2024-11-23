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

#include "queueviewer.h"

#include "guiutils.h"
#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "queueviewerdelegate.h"
#include "queueviewermodel.h"
#include "queueviewerview.h"

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/widgets/expandedtreeview.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
QueueViewer::QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{m_playlistInteractor->playerController()}
    , m_settings{settings}
    , m_view{new QueueViewerView(this)}
    , m_model{new QueueViewerModel(std::move(audioLoader), settings, this)}
    , m_context{new WidgetContext(this, Context{Id{"Context.QueueViewer."}.append(Utils::generateUniqueHash())}, this)}
    , m_remove{new QAction(tr("Remove"), this)}
    , m_removeCmd{nullptr}
    , m_clear{new QAction(tr("&Clear"), this)}
    , m_clearCmd{nullptr}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setModel(m_model);
    m_view->setItemDelegate(new QueueViewerDelegate(this));
    m_view->changeIconSize(m_settings->value<Settings::Gui::Internal::QueueViewerIconSize>().toSize());
    m_view->header()->setHidden(!m_settings->value<Settings::Gui::Internal::QueueViewerHeader>());
    m_view->verticalScrollBar()->setVisible(m_settings->value<Settings::Gui::Internal::QueueViewerScrollBar>());
    m_view->setAlternatingRowColors(m_settings->value<Settings::Gui::Internal::QueueViewerAltColours>());

    setupActions();
    setupConnections();
    resetModel();
}

QString QueueViewer::name() const
{
    return tr("Playback Queue");
}

QString QueueViewer::layoutName() const
{
    return u"PlaybackQueue"_s;
}

void QueueViewer::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(m_removeCmd && m_view->selectionModel()->hasSelection()) {
        menu->addAction(m_removeCmd->action());
    }
    if(m_clearCmd) {
        menu->addAction(m_clearCmd->action());
    }

    menu->popup(event->globalPos());
}

void QueueViewer::setupActions()
{
    m_actionManager->addContextObject(m_context);

    m_remove->setStatusTip(tr("Remove the selected tracks from the playback queue"));
    m_removeCmd = m_actionManager->registerAction(m_remove, Constants::Actions::Remove, m_context->context());
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_remove, &QAction::triggered, this, &QueueViewer::removeSelectedTracks);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_remove->setEnabled(m_view->selectionModel()->hasSelection()); });
    m_remove->setEnabled(m_view->selectionModel()->hasSelection());

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    m_clear->setStatusTip(tr("Remove all tracks in the playback queue"));
    m_clearCmd = m_actionManager->registerAction(m_clear, Constants::Actions::Clear, m_context->context());
    editMenu->addAction(m_clearCmd);
    QObject::connect(m_clear, &QAction::triggered, m_playerController, &PlayerController::clearQueue);
    m_clear->setEnabled(m_model->rowCount({}) > 0);

    auto* selectAllAction = new QAction(tr("&Select all"), this);
    selectAllAction->setStatusTip(tr("Select all tracks in the playback queue"));
    auto* selectAllCmd
        = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, m_context->context());
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, m_view, &QAbstractItemView::selectAll);
}

void QueueViewer::setupConnections()
{
    QObject::connect(m_model, &QueueViewerModel::tracksDropped, this, &QueueViewer::handleTracksDropped);
    QObject::connect(m_model, &QueueViewerModel::playlistTracksDropped, this,
                     &QueueViewer::handlePlaylistTracksDropped);
    QObject::connect(m_model, &QueueViewerModel::queueChanged, this, &QueueViewer::handleQueueChanged);
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::tracksQueued, m_model, &QueueViewerModel::insertTracks);
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, m_model, &QueueViewerModel::removeTracks);
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, this, [this](const QSize& size) {
        m_settings->set<Settings::Gui::Internal::QueueViewerIconSize>(size);
    });

    m_settings->subscribe<Settings::Gui::Internal::QueueViewerShowIcon>(m_view, [this]() {
        QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    });
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerIconSize>(
        m_view, [this](const auto& size) { m_view->changeIconSize(size.toSize()); });
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerHeader>(
        m_view, [this](const bool show) { m_view->header()->setHidden(!show); });
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerScrollBar>(m_view->verticalScrollBar(),
                                                                         &QScrollBar::setVisible);
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerAltColours>(m_view,
                                                                          &ExpandedTreeView::setAlternatingRowColors);
}

void QueueViewer::resetModel() const
{
    if(!m_changingQueue) {
        m_model->reset(m_playerController->playbackQueue().tracks());
    }
}

void QueueViewer::handleRowsChanged() const
{
    m_clear->setEnabled(m_model->rowCount({}) > 0);
}

void QueueViewer::removeSelectedTracks() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    if(selected.empty()) {
        return;
    }

    std::vector<int> indexes;
    indexes.reserve(selected.size());

    for(const QModelIndex& index : selected) {
        indexes.emplace_back(index.row());
    }

    m_playerController->dequeueTracks(indexes);
    m_model->removeIndexes(indexes);
}

void QueueViewer::handleTracksDropped(int row, const QByteArray& mimeData) const
{
    const TrackList tracks = Gui::tracksFromMimeData(m_playlistInteractor->library(), mimeData);

    QueueTracks queueTracks;
    for(const Track& track : tracks) {
        queueTracks.emplace_back(track);
    }

    m_model->insertTracks(queueTracks, row);
}

void QueueViewer::handlePlaylistTracksDropped(int row, const QByteArray& mimeData) const
{
    const QueueTracks tracks = Gui::queueTracksFromMimeData(m_playlistInteractor->library(), mimeData);
    m_model->insertTracks(tracks, row);
}

void QueueViewer::handleQueueChanged()
{
    const QueueTracks tracks = m_model->queueTracks();

    m_changingQueue = true;
    m_playerController->replaceTracks(tracks);
    m_changingQueue = false;
}
} // namespace Fooyin
