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

#include "replaygainwidget.h"

#include "replaygaindelegate.h"
#include "replaygainmodel.h"
#include "replaygainview.h"

#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <core/library/libraryutils.h>
#include <core/library/musiclibrary.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto ReplayGainKey = "PropertiesDialog/ReplayGain"_L1;

namespace {
void mergeTracks(Fooyin::TrackList& destination, const Fooyin::TrackList& source)
{
    for(const Fooyin::Track& track : source) {
        const auto trackIt = std::ranges::find_if(destination, [&track](const Fooyin::Track& destinationTrack) {
            return destinationTrack.sameIdentityAs(track);
        });

        if(trackIt != destination.end()) {
            *trackIt = track;
        }
        else {
            destination.emplace_back(track);
        }
    }
}

int replayGainColumnWidth(const Fooyin::ReplayGainView* view, const QString& headerText, const QString& sampleText)
{
    const int textWidth = std::max(view->fontMetrics().horizontalAdvance(sampleText),
                                   view->header()->fontMetrics().horizontalAdvance(headerText));
    return textWidth + 24;
}
} // namespace

namespace Fooyin {
ReplayGainWidget::ReplayGainWidget(MusicLibrary* library, const TrackList& tracks, bool readOnly,
                                   SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_library{library}
    , m_settings{settings}
    , m_view{new ReplayGainView(this)}
    , m_model{new ReplayGainModel(readOnly, this)}
    , m_opusWriteMode{static_cast<OpusRGWriteMode>(m_settings->value<Settings::Core::Internal::OpusHeaderWriteMode>())}
    , m_showHeader{true}
    , m_alternatingColours{true}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ReplayGainDelegate(this));
    m_view->setModel(m_model);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        QMetaObject::invokeMethod(
            this,
            [this]() {
                updateHeaderModes();
                finalise();
            },
            Qt::QueuedConnection);
    });
    loadLayoutData();
    m_model->resetModel(tracks);

    updateHeaderModes();
    finalise();
}

ReplayGainWidget::~ReplayGainWidget()
{
    saveLayoutData();
}

void ReplayGainWidget::apply()
{
    commitPendingChanges();

    if(!m_pendingTracks.empty()) {
        TrackList tracksToWrite;
        tracksToWrite.reserve(m_pendingTracks.size());
        for(const Track& track : std::as_const(m_pendingTracks)) {
            tracksToWrite.emplace_back(prepareOpusRGWriteTrack(track, m_opusWriteMode));
        }

        Q_EMIT tracksChanged(m_pendingTracks);
        m_library->writeTrackMetadata(tracksToWrite);
        m_pendingTracks.clear();
    }
}

void ReplayGainWidget::setTrackScope(const TrackList& tracks)
{
    m_model->resetModel(tracks);
    updateHeaderModes();
    finalise();
}

bool ReplayGainWidget::commitPendingChanges()
{
    const TrackList changedTracks = m_model->applyChanges();
    if(changedTracks.empty()) {
        return true;
    }

    mergeTracks(m_pendingTracks, changedTracks);
    Q_EMIT tracksChanged(changedTracks);
    return true;
}

void ReplayGainWidget::updateTracks(const TrackList& tracks)
{
    m_model->updateTracks(tracks);
    updateHeaderModes();
    finalise();
}

void ReplayGainWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(tr("Show header"), menu);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!m_view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_showHeader = checked;
        finalise();
        saveLayoutData();
    });

    auto* altColours = new QAction(tr("Alternating row colours"), menu);
    altColours->setCheckable(true);
    altColours->setChecked(m_view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_alternatingColours = checked;
        finalise();
        saveLayoutData();
    });

    menu->addAction(showHeaders);
    menu->addAction(altColours);
    menu->popup(event->globalPos());
}

void ReplayGainWidget::finalise()
{
    m_view->setHeaderHidden(!m_showHeader);
    m_view->setAlternatingRowColors(m_alternatingColours);
}

void ReplayGainWidget::loadLayoutData()
{
    const FyStateSettings settings;
    const QByteArray layoutData = QByteArray::fromBase64(settings.value(ReplayGainKey).toByteArray());
    if(layoutData.isEmpty()) {
        return;
    }

    const QJsonObject layout = QJsonDocument::fromJson(layoutData).object();
    m_showHeader             = layout.value("ShowHeader"_L1).toBool(m_showHeader);
    m_alternatingColours     = layout.value("AlternatingRows"_L1).toBool(m_alternatingColours);
}

void ReplayGainWidget::saveLayoutData() const
{
    QJsonObject layout;
    layout["ShowHeader"_L1]      = m_showHeader;
    layout["AlternatingRows"_L1] = m_alternatingColours;

    const QByteArray layoutData = QJsonDocument{layout}.toJson(QJsonDocument::Compact).toBase64();
    FyStateSettings settings;
    settings.setValue(ReplayGainKey, layoutData);
}

void ReplayGainWidget::updateHeaderModes() const
{
    auto* header = m_view->header();
    header->setStretchLastSection(false);

    if(m_model->columnCount({}) == 2) {
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->setSectionResizeMode(1, QHeaderView::Interactive);
        header->resizeSection(
            1, replayGainColumnWidth(m_view, m_model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
                                     u"+123.45 dB"_s));
    }
    else {
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        for(int i{1}; i < header->count(); ++i) {
            header->setSectionResizeMode(i, QHeaderView::Interactive);
            const QString sampleText = (i == 2 || i == 4) ? u"123.456789"_s : u"+123.45 dB"_s;
            header->resizeSection(
                i, replayGainColumnWidth(m_view, m_model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(),
                                         sampleText));
        }
    }
}
} // namespace Fooyin

#include "moc_replaygainwidget.cpp"
