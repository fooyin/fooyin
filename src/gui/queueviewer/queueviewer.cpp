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
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/expandedtreeview.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QVBoxLayout>

namespace Fooyin {
struct QueueViewer::Private
{
    QueueViewer* m_self;
    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QueueViewerView* m_view;
    QueueViewerModel* m_model;
    WidgetContext* m_context;
    bool m_changingQueue{false};

    QAction* m_remove;
    Command* m_removeCmd;

    Private(QueueViewer* self, ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
            std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings)
        : m_self{self}
        , m_actionManager{actionManager}
        , m_playlistInteractor{playlistInteractor}
        , m_playerController{m_playlistInteractor->playerController()}
        , m_settings{settings}
        , m_view{new QueueViewerView(m_self)}
        , m_model{new QueueViewerModel(std::move(tagLoader), settings, m_self)}
        , m_context{new WidgetContext(m_self, Context{Id{"Context.QueueViewer."}.append(Utils::generateRandomHash())},
                                      m_self)}
        , m_remove{new QAction(tr("Remove"))}
        , m_removeCmd{actionManager->registerAction(m_remove, Constants::Actions::Remove, m_context->context())}
    {
        auto* layout = new QVBoxLayout(m_self);
        layout->setContentsMargins({});
        layout->addWidget(m_view);

        m_view->setModel(m_model);
        m_view->setItemDelegate(new QueueViewerDelegate(m_self));

        actionManager->addContextObject(m_context);

        setupSettings();
        setupActions();
        setupConnections();
    }

    void setupSettings()
    {
        m_model->setShowIcon(m_settings->value<Settings::Gui::Internal::QueueViewerShowIcon>());
        m_view->changeIconSize(m_settings->value<Settings::Gui::Internal::QueueViewerIconSize>().toSize());
        m_view->header()->setHidden(!m_settings->value<Settings::Gui::Internal::QueueViewerHeader>());
        m_view->verticalScrollBar()->setVisible(m_settings->value<Settings::Gui::Internal::QueueViewerScrollBar>());
        m_view->setAlternatingRowColors(m_settings->value<Settings::Gui::Internal::QueueViewerAltColours>());

        m_settings->subscribe<Settings::Gui::Internal::QueueViewerShowIcon>(m_model, [this](const bool show) {
            m_model->setShowIcon(show);
            QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
        });
        m_settings->subscribe<Settings::Gui::Internal::QueueViewerIconSize>(
            m_view, [this](const auto& size) { m_view->changeIconSize(size.toSize()); });
        m_settings->subscribe<Settings::Gui::Internal::QueueViewerHeader>(
            m_view, [this](const bool show) { m_view->header()->setHidden(!show); });
        m_settings->subscribe<Settings::Gui::Internal::QueueViewerScrollBar>(m_view->verticalScrollBar(),
                                                                             &QScrollBar::setVisible);
        m_settings->subscribe<Settings::Gui::Internal::QueueViewerAltColours>(
            m_view, &ExpandedTreeView::setAlternatingRowColors);
    }

    void setupActions()
    {
        m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
        QObject::connect(m_remove, &QAction::triggered, m_self, [this]() { removeSelectedTracks(); });
        QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, m_self,
                         [this]() { m_remove->setEnabled(m_view->selectionModel()->hasSelection()); });
        m_remove->setEnabled(m_view->selectionModel()->hasSelection());
    }

    void setupConnections()
    {
        QObject::connect(m_model, &QueueViewerModel::tracksDropped, m_self,
                         [this](int row, const QByteArray& data) { handleTracksDropped(row, data); });
        QObject::connect(m_model, &QueueViewerModel::playlistTracksDropped, m_self,
                         [this](int row, const QByteArray& data) { handlePlaylistTracksDropped(row, data); });
        QObject::connect(m_model, &QueueViewerModel::queueChanged, m_self, [this]() { handleQueueChanged(); });
        QObject::connect(m_playerController, &PlayerController::trackQueueChanged, m_self, [this]() { resetModel(); });
        QObject::connect(m_playerController, &PlayerController::tracksQueued, m_model, &QueueViewerModel::addTracks);
        QObject::connect(m_playerController, &PlayerController::tracksDequeued, m_model,
                         &QueueViewerModel::removeTracks);
        QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, m_self, [this](const QSize& size) {
            m_settings->set<Settings::Gui::Internal::QueueViewerIconSize>(size);
        });
    }

    void resetModel() const
    {
        if(!m_changingQueue) {
            m_model->reset(m_playerController->playbackQueue().tracks());
        }
    }

    void removeSelectedTracks() const
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
    }

    void handleTracksDropped(int row, const QByteArray& mimeData) const
    {
        const TrackList tracks = Gui::tracksFromMimeData(m_playlistInteractor->library(), mimeData);

        QueueTracks queueTracks;
        for(const Track& track : tracks) {
            queueTracks.emplace_back(track);
        }

        m_model->insertTracks(row, queueTracks);
    }

    void handlePlaylistTracksDropped(int row, const QByteArray& mimeData) const
    {
        const QueueTracks tracks = Gui::queueTracksFromMimeData(m_playlistInteractor->library(), mimeData);
        m_model->insertTracks(row, tracks);
    }

    void handleQueueChanged()
    {
        const QueueTracks tracks = m_model->queueTracks();

        m_changingQueue = true;
        m_playerController->replaceTracks(tracks);
        m_changingQueue = false;
    }
};

QueueViewer::QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, playlistInteractor, std::move(tagLoader), settings)}
{
    p->resetModel();
}

QueueViewer::~QueueViewer() = default;

QString QueueViewer::name() const
{
    return tr("Playback Queue");
}

QString QueueViewer::layoutName() const
{
    return QStringLiteral("PlaybackQueue");
}

void QueueViewer::contextMenuEvent(QContextMenuEvent* event)
{
    if(!p->m_view->selectionModel()->hasSelection()) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(p->m_removeCmd->action());

    menu->popup(event->globalPos());
}
} // namespace Fooyin
