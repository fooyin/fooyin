/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "playbackqueuepage.h"

#include <core/coresettings.h>
#include <core/player/playbackqueue.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class PlaybackQueuePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaybackQueuePageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateWidgetState();

    SettingsManager* m_settings;

    QRadioButton* m_overridesMode;
    QRadioButton* m_playingTracksMode;
    QCheckBox* m_clearQueueOnStartup;
    QCheckBox* m_showQueueIndexes;
    QCheckBox* m_followPlaybackQueue;
    QCheckBox* m_stopWhenQueueFinished;
    QCheckBox* m_followCurrentTrack;
    QCheckBox* m_limitHistory;
    QLabel* m_historyLimitLabel;
    QSpinBox* m_historyLimit;
    QRadioButton* m_playSelectedTracks;
    QRadioButton* m_playContainingGroup;
    QRadioButton* m_playAllTracks;
    QRadioButton* m_queueNextAndPlay;
};

PlaybackQueuePageWidget::PlaybackQueuePageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_overridesMode{new QRadioButton(tr("Playlist-led"), this)}
    , m_playingTracksMode{new QRadioButton(tr("Queue-led"), this)}
    , m_clearQueueOnStartup{new QCheckBox(tr("Start with an empty playback queue"), this)}
    , m_showQueueIndexes{new QCheckBox(tr("Show queue indexes in playlists"), this)}
    , m_followPlaybackQueue{new QCheckBox(tr("Continue from the playlist after queued tracks finish"), this)}
    , m_stopWhenQueueFinished{new QCheckBox(tr("Stop playback after queued tracks finish"), this)}
    , m_followCurrentTrack{new QCheckBox(tr("Scroll the current track to the top"), this)}
    , m_limitHistory{new QCheckBox(tr("Limit recent track history"), this)}
    , m_historyLimitLabel{new QLabel(tr("History limit") + u": "_s, this)}
    , m_historyLimit{new QSpinBox(this)}
    , m_playSelectedTracks{new QRadioButton(tr("Replace with selected tracks"), this)}
    , m_playContainingGroup{new QRadioButton(tr("Replace with the containing group"), this)}
    , m_playAllTracks{new QRadioButton(tr("Replace with all tracks in the current view"), this)}
    , m_queueNextAndPlay{new QRadioButton(tr("Insert selected tracks next and play now"), this)}
{
    m_overridesMode->setToolTip(
        tr("The active playlist drives playback, with queued tracks temporarily taking priority"));
    m_playingTracksMode->setToolTip(
        tr("The queue drives playback, containing the played, current, and upcoming tracks"));

    auto* queueModeGroup = new QButtonGroup(this);
    queueModeGroup->addButton(m_overridesMode);
    queueModeGroup->addButton(m_playingTracksMode);

    QObject::connect(m_playingTracksMode, &QRadioButton::toggled, this, &PlaybackQueuePageWidget::updateWidgetState);

    m_playSelectedTracks->setToolTip(
        tr("Replace Playing Tracks with the selected tracks and start playback at the first selection"));
    m_playContainingGroup->setToolTip(
        tr("Replace Playing Tracks with the first selected track's group and start playback at that track"));
    m_playAllTracks->setToolTip(
        tr("Replace Playing Tracks with the current view and start playback at the first selected track"));
    m_queueNextAndPlay->setToolTip(
        tr("Keep Playing Tracks, insert the selected tracks next, and play the first selection immediately"));

    auto* playNowGroup = new QButtonGroup(this);
    playNowGroup->addButton(m_playSelectedTracks);
    playNowGroup->addButton(m_playContainingGroup);
    playNowGroup->addButton(m_playAllTracks);
    playNowGroup->addButton(m_queueNextAndPlay);

    m_clearQueueOnStartup->setToolTip(tr("Start fooyin without restoring the saved queue or current track"));

    auto* generalGroup       = new QGroupBox(tr("Queue mode"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    int row{0};
    generalGroupLayout->addWidget(m_overridesMode, row++, 0);
    generalGroupLayout->addWidget(m_playingTracksMode, row++, 0);
    generalGroupLayout->addWidget(m_clearQueueOnStartup, row++, 0);

    m_showQueueIndexes->setToolTip(
        tr("Show the position of each queued override beside its source track in playlists"));
    m_followPlaybackQueue->setToolTip(
        tr("After the queue empties, continue after the last queued track in its source playlist"));
    m_stopWhenQueueFinished->setToolTip(
        tr("Stop when the final queued override finishes instead of returning to playlist playback"));

    auto* overridesGroup       = new QGroupBox(tr("Playlist-led options"), this);
    auto* overridesGroupLayout = new QGridLayout(overridesGroup);

    row = 0;
    overridesGroupLayout->addWidget(m_showQueueIndexes, row++, 0);
    overridesGroupLayout->addWidget(m_followPlaybackQueue, row++, 0);
    overridesGroupLayout->addWidget(m_stopWhenQueueFinished, row++, 0);

    m_historyLimit->setRange(-1, 9999);
    m_historyLimit->setSpecialValueText(tr("Unlimited"));

    m_followCurrentTrack->setToolTip(tr("Keep the current track at the top of the Playing Tracks view"));
    m_limitHistory->setToolTip(tr("Automatically remove played tracks beyond the retained history"));
    m_historyLimit->setToolTip(tr("Number of played tracks to retain before the current track"));

    auto* playingTracksGroup       = new QGroupBox(tr("Queue-led options"), this);
    auto* playingTracksGroupLayout = new QGridLayout(playingTracksGroup);

    row = 0;
    playingTracksGroupLayout->addWidget(Gui::createSectionHeader(tr("Play now action"), this), row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_playSelectedTracks, row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_playContainingGroup, row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_playAllTracks, row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_queueNextAndPlay, row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(Gui::createSectionHeader(tr("Display and history"), this), row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_followCurrentTrack, row++, 0, 1, 2);
    playingTracksGroupLayout->addWidget(m_limitHistory, row++, 0);
    playingTracksGroupLayout->setColumnStretch(1, 1);

    auto* historyLimitLayout = new QHBoxLayout();
    historyLimitLayout->setContentsMargins(25, 0, 0, 0);
    historyLimitLayout->addWidget(m_historyLimitLabel);
    historyLimitLayout->addWidget(m_historyLimit);
    historyLimitLayout->addStretch(1);
    playingTracksGroupLayout->addLayout(historyLimitLayout, row++, 0, 1, 2);

    QObject::connect(m_limitHistory, &QCheckBox::toggled, this, &PlaybackQueuePageWidget::updateWidgetState);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0);
    layout->addWidget(overridesGroup, row++, 0);
    layout->addWidget(playingTracksGroup, row++, 0);
    layout->setRowStretch(row, 1);
}

void PlaybackQueuePageWidget::load()
{
    const auto queueMode = static_cast<PlaybackQueueMode>(m_settings->value<Settings::Core::PlaybackQueueMode>());
    m_overridesMode->setChecked(queueMode == PlaybackQueueMode::PlaylistWithOverrides);
    m_playingTracksMode->setChecked(queueMode == PlaybackQueueMode::QueueAsPlaybackSource);
    m_clearQueueOnStartup->setChecked(m_settings->value<Settings::Core::ClearPlaybackQueueOnStartup>());
    m_showQueueIndexes->setChecked(m_settings->value<Settings::Gui::PlaylistShowQueueIndexes>());
    m_followPlaybackQueue->setChecked(m_settings->value<Settings::Core::FollowPlaybackQueue>());
    m_stopWhenQueueFinished->setChecked(m_settings->value<Settings::Core::PlaybackQueueStopWhenFinished>());
    m_followCurrentTrack->setChecked(m_settings->value<Settings::Gui::PlaybackQueueFollowCurrent>());

    const auto playNowAction
        = static_cast<PlayNowAction>(m_settings->value<Settings::Core::PlaybackQueuePlayNowAction>());
    m_playSelectedTracks->setChecked(playNowAction == PlayNowAction::SelectedTracks);
    m_playContainingGroup->setChecked(playNowAction == PlayNowAction::ContainingGroup);
    m_playAllTracks->setChecked(playNowAction == PlayNowAction::AllTracks);
    m_queueNextAndPlay->setChecked(playNowAction == PlayNowAction::QueueNext);

    const int historyLimit = m_settings->value<Settings::Core::PlaybackQueueHistoryLimit>();
    m_limitHistory->setChecked(historyLimit >= 0);
    m_historyLimit->setValue(historyLimit);

    updateWidgetState();
}

void PlaybackQueuePageWidget::apply()
{
    m_settings->set<Settings::Core::PlaybackQueueMode>(
        static_cast<int>(m_playingTracksMode->isChecked() ? PlaybackQueueMode::QueueAsPlaybackSource
                                                          : PlaybackQueueMode::PlaylistWithOverrides));
    m_settings->set<Settings::Core::ClearPlaybackQueueOnStartup>(m_clearQueueOnStartup->isChecked());
    m_settings->set<Settings::Gui::PlaylistShowQueueIndexes>(m_showQueueIndexes->isChecked());
    m_settings->set<Settings::Core::FollowPlaybackQueue>(m_followPlaybackQueue->isChecked());
    m_settings->set<Settings::Core::PlaybackQueueStopWhenFinished>(m_stopWhenQueueFinished->isChecked());
    m_settings->set<Settings::Gui::PlaybackQueueFollowCurrent>(m_followCurrentTrack->isChecked());
    PlayNowAction playNowAction{PlayNowAction::AllTracks};
    if(m_playSelectedTracks->isChecked()) {
        playNowAction = PlayNowAction::SelectedTracks;
    }
    else if(m_playContainingGroup->isChecked()) {
        playNowAction = PlayNowAction::ContainingGroup;
    }
    else if(m_queueNextAndPlay->isChecked()) {
        playNowAction = PlayNowAction::QueueNext;
    }
    m_settings->set<Settings::Core::PlaybackQueuePlayNowAction>(static_cast<int>(playNowAction));
    m_settings->set<Settings::Core::PlaybackQueueHistoryLimit>(m_limitHistory->isChecked() ? m_historyLimit->value()
                                                                                           : -1);
}

void PlaybackQueuePageWidget::reset()
{
    m_settings->reset<Settings::Core::PlaybackQueueMode>();
    m_settings->reset<Settings::Core::ClearPlaybackQueueOnStartup>();
    m_settings->reset<Settings::Gui::PlaylistShowQueueIndexes>();
    m_settings->reset<Settings::Core::FollowPlaybackQueue>();
    m_settings->reset<Settings::Core::PlaybackQueueStopWhenFinished>();
    m_settings->reset<Settings::Gui::PlaybackQueueFollowCurrent>();
    m_settings->reset<Settings::Core::PlaybackQueuePlayNowAction>();
    m_settings->reset<Settings::Core::PlaybackQueueHistoryLimit>();
}

void PlaybackQueuePageWidget::updateWidgetState()
{
    const bool playingTracks = m_playingTracksMode->isChecked();

    m_showQueueIndexes->setEnabled(!playingTracks);
    m_followPlaybackQueue->setEnabled(!playingTracks);
    m_stopWhenQueueFinished->setEnabled(!playingTracks);
    m_followCurrentTrack->setEnabled(playingTracks);
    m_playSelectedTracks->setEnabled(playingTracks);
    m_playContainingGroup->setEnabled(playingTracks);
    m_playAllTracks->setEnabled(playingTracks);
    m_queueNextAndPlay->setEnabled(playingTracks);
    m_limitHistory->setEnabled(playingTracks);
    m_historyLimitLabel->setEnabled(playingTracks && m_limitHistory->isChecked());
    m_historyLimit->setEnabled(playingTracks && m_limitHistory->isChecked());
}

PlaybackQueuePage::PlaybackQueuePage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaybackQueue);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("Queue")});
    setWidgetCreator([settings] { return new PlaybackQueuePageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playbackqueuepage.cpp"
#include "playbackqueuepage.moc"
