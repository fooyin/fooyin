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
#include "playlist/playlistinteractor.h"
#include "queueviewerconfigwidget.h"
#include "queueviewerdelegate.h"
#include "queueviewermodel.h"
#include "queueviewerview.h"

#include <core/player/playercontroller.h>
#include <gui/configdialog.h>
#include <gui/guiconstants.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>

using namespace Qt::StringLiterals;

// Settings
constexpr auto QueueViewerShowIconKey    = u"PlaybackQueue/ShowIcon";
constexpr auto QueueViewerIconSizeKey    = u"PlaybackQueue/IconSize";
constexpr auto QueueViewerHeaderKey      = u"PlaybackQueue/Header";
constexpr auto QueueViewerScrollBarKey   = u"PlaybackQueue/Scrollbar";
constexpr auto QueueViewerAltColoursKey  = u"PlaybackQueue/AlternatingColours";
constexpr auto QueueViewerLeftScriptKey  = u"PlaybackQueue/LeftScript";
constexpr auto QueueViewerRightScriptKey = u"PlaybackQueue/RightScript";
constexpr auto QueueViewerShowCurrentKey = u"PlaybackQueue/ShowCurrent";

namespace Fooyin {
QueueViewer::QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{m_playlistInteractor->playerController()}
    , m_settings{settings}
    , m_view{new QueueViewerView(this)}
    , m_model{new QueueViewerModel(std::move(audioLoader), m_playerController, settings, this)}
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

    m_config = defaultConfig();
    applyConfig(m_config);

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

void QueueViewer::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void QueueViewer::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

void QueueViewer::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(m_removeCmd && m_view->selectionModel()->hasSelection()) {
        m_remove->setEnabled(canRemoveSelected());
        menu->addAction(m_removeCmd->action());
    }
    if(m_clearCmd) {
        m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);
        menu->addAction(m_clearCmd->action());
    }

    auto* showCurrent = new QAction(tr("Show playing queue track"), menu);
    showCurrent->setCheckable(true);
    showCurrent->setChecked(m_config.showCurrent);
    QObject::connect(showCurrent, &QAction::triggered, showCurrent, [this](bool enabled) {
        auto config        = m_config;
        config.showCurrent = enabled;
        applyConfig(config);
    });

    menu->addSeparator();
    menu->addAction(showCurrent);
    menu->addSeparator();

    addConfigureAction(menu, false);

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
                     [this]() { m_remove->setEnabled(canRemoveSelected()); });
    m_remove->setEnabled(canRemoveSelected());

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    m_clear->setStatusTip(tr("Remove all tracks in the playback queue"));
    m_clearCmd = m_actionManager->registerAction(m_clear, Constants::Actions::Clear, m_context->context());
    editMenu->addAction(m_clearCmd);
    QObject::connect(m_clear, &QAction::triggered, m_playerController, &PlayerController::clearQueue);
    m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);

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
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_model,
                     &QueueViewerModel::currentTrackChanged);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_model,
                     &QueueViewerModel::playbackStateChanged);
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, this, [this](const QSize& size) {
        if(m_config.iconSize == size) {
            return;
        }

        m_config.iconSize = size;
        m_model->setIconSize(size);
    });
    QObject::connect(m_view, &QAbstractItemView::doubleClicked, this, &QueueViewer::handleQueueDoubleClicked);
}

void QueueViewer::resetModel() const
{
    if(!m_changingQueue) {
        m_model->reset(m_playerController->playbackQueue().tracks());
    }
}

bool QueueViewer::canRemoveSelected() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    return std::ranges::any_of(selected, [this](const QModelIndex& index) {
        const auto track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        return !m_playerController->currentIsQueueTrack() || track != m_playerController->currentPlaylistTrack();
    });
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
        const auto track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        if(!m_playerController->currentIsQueueTrack() || track != m_playerController->currentPlaylistTrack()) {
            indexes.emplace_back(m_model->queueIndex(index));
        }
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

void QueueViewer::handleQueueDoubleClicked(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    const int queueIndex = m_model->queueIndex(index);

    std::vector<int> indexes;
    indexes.reserve(queueIndex);

    std::ranges::copy(std::views::iota(0, queueIndex), std::back_inserter(indexes));

    m_playerController->dequeueTracks(indexes);
    m_model->removeIndexes(indexes);

    m_playerController->next();
}

QueueViewer::ConfigData QueueViewer::defaultConfig() const
{
    auto config{factoryConfig()};

    config.leftScript      = m_settings->fileValue(QueueViewerLeftScriptKey, config.leftScript).toString();
    config.rightScript     = m_settings->fileValue(QueueViewerRightScriptKey, config.rightScript).toString();
    config.showCurrent     = m_settings->fileValue(QueueViewerShowCurrentKey, config.showCurrent).toBool();
    config.showIcon        = m_settings->fileValue(QueueViewerShowIconKey, config.showIcon).toBool();
    config.iconSize        = m_settings->fileValue(QueueViewerIconSizeKey, config.iconSize).toSize();
    config.showHeader      = m_settings->fileValue(QueueViewerHeaderKey, config.showHeader).toBool();
    config.showScrollBar   = m_settings->fileValue(QueueViewerScrollBarKey, config.showScrollBar).toBool();
    config.alternatingRows = m_settings->fileValue(QueueViewerAltColoursKey, config.alternatingRows).toBool();

    return config;
}

QueueViewer::ConfigData QueueViewer::factoryConfig() const
{
    return {
        .leftScript      = u"%title%$crlf()%album%"_s,
        .rightScript     = u"%duration%"_s,
        .showCurrent     = true,
        .showIcon        = true,
        .iconSize        = QSize{36, 36},
        .showHeader      = true,
        .showScrollBar   = true,
        .alternatingRows = false,
    };
}

const QueueViewer::ConfigData& QueueViewer::currentConfig() const
{
    return m_config;
}

void QueueViewer::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(QueueViewerLeftScriptKey, config.leftScript);
    m_settings->fileSet(QueueViewerRightScriptKey, config.rightScript);
    m_settings->fileSet(QueueViewerShowCurrentKey, config.showCurrent);
    m_settings->fileSet(QueueViewerShowIconKey, config.showIcon);
    m_settings->fileSet(QueueViewerIconSizeKey, config.iconSize);
    m_settings->fileSet(QueueViewerHeaderKey, config.showHeader);
    m_settings->fileSet(QueueViewerScrollBarKey, config.showScrollBar);
    m_settings->fileSet(QueueViewerAltColoursKey, config.alternatingRows);
}

void QueueViewer::clearSavedDefaults() const
{
    m_settings->fileRemove(QueueViewerLeftScriptKey);
    m_settings->fileRemove(QueueViewerRightScriptKey);
    m_settings->fileRemove(QueueViewerShowCurrentKey);
    m_settings->fileRemove(QueueViewerShowIconKey);
    m_settings->fileRemove(QueueViewerIconSizeKey);
    m_settings->fileRemove(QueueViewerHeaderKey);
    m_settings->fileRemove(QueueViewerScrollBarKey);
    m_settings->fileRemove(QueueViewerAltColoursKey);
}

void QueueViewer::applyConfig(const ConfigData& config)
{
    m_config = config;

    m_model->setScripts(m_config.leftScript, m_config.rightScript);
    m_model->setShowCurrent(m_config.showCurrent);
    m_model->setShowIcon(m_config.showIcon);
    m_model->setIconSize(m_config.iconSize);

    m_view->changeIconSize(m_config.iconSize);
    m_view->header()->setHidden(!m_config.showHeader);
    m_view->verticalScrollBar()->setVisible(m_config.showScrollBar);
    m_view->setAlternatingRowColors(m_config.alternatingRows);

    QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

QueueViewer::ConfigData QueueViewer::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("LeftScript"_L1)) {
        config.leftScript = layout.value("LeftScript"_L1).toString();
    }
    if(layout.contains("RightScript"_L1)) {
        config.rightScript = layout.value("RightScript"_L1).toString();
    }
    if(layout.contains("ShowCurrent"_L1)) {
        config.showCurrent = layout.value("ShowCurrent"_L1).toBool();
    }
    if(layout.contains("ShowIcon"_L1)) {
        config.showIcon = layout.value("ShowIcon"_L1).toBool();
    }
    if(layout.contains("IconWidth"_L1) && layout.contains("IconHeight"_L1)) {
        config.iconSize = {layout.value("IconWidth"_L1).toInt(), layout.value("IconHeight"_L1).toInt()};
    }
    if(layout.contains("ShowHeader"_L1)) {
        config.showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollBar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        config.alternatingRows = layout.value("AlternatingRows"_L1).toBool();
    }

    if(!config.iconSize.isValid()) {
        config.iconSize = factoryConfig().iconSize;
    }

    return config;
}

void QueueViewer::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["LeftScript"_L1]      = config.leftScript;
    layout["RightScript"_L1]     = config.rightScript;
    layout["ShowCurrent"_L1]     = config.showCurrent;
    layout["ShowIcon"_L1]        = config.showIcon;
    layout["IconWidth"_L1]       = config.iconSize.width();
    layout["IconHeight"_L1]      = config.iconSize.height();
    layout["ShowHeader"_L1]      = config.showHeader;
    layout["ShowScrollbar"_L1]   = config.showScrollBar;
    layout["AlternatingRows"_L1] = config.alternatingRows;
}

void QueueViewer::openConfigDialog()
{
    showConfigDialog(new QueueViewerConfigDialog(this, this));
}
} // namespace Fooyin
