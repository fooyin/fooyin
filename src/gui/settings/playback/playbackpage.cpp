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

#include "playbackpage.h"

#include "core/internalcoresettings.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>

namespace Fooyin {
class PlaybackPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaybackPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_restorePlayback;

    QCheckBox* m_cursorFollowsPlayback;
    QCheckBox* m_playbackFollowsCursor;
    QCheckBox* m_rewindPrevious;
    QCheckBox* m_skipUnavailable;
    QSlider* m_playedSlider;
    QSpinBox* m_playedPercent;

    QCheckBox* m_alwaysSend;
    QLineEdit* m_externalPlaylist;
};

PlaybackPageWidget::PlaybackPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_restorePlayback{new QCheckBox(tr("Save/restore playback state"), this)}
    , m_cursorFollowsPlayback{new QCheckBox(tr("Cursor follows playback"), this)}
    , m_playbackFollowsCursor{new QCheckBox(tr("Playback follows cursor"), this)}
    , m_rewindPrevious{new QCheckBox(tr("Rewind track on previous"), this)}
    , m_skipUnavailable{new QCheckBox(tr("Skip unavailable tracks"), this)}
    , m_playedSlider{new QSlider(Qt::Horizontal, this)}
    , m_playedPercent{new QSpinBox(this)}
    , m_alwaysSend{new QCheckBox(tr("Always send to playlist"), this)}
    , m_externalPlaylist{new QLineEdit(this)}
{
    auto* layout = new QGridLayout(this);

    m_restorePlayback->setToolTip(tr("Save playback state on exit and restore it on next startup"));
    m_rewindPrevious->setToolTip(tr(
        "If the current track has been playing for more than 5s, restart it instead of moving to the previous track"));
    m_skipUnavailable->setToolTip(
        tr("If the current track in a playlist is unavailable, silently continue to the next track"));
    m_externalPlaylist->setToolTip(tr("When opening files, always send to playlist, replacing all existing tracks"));

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    auto* playedLabel = new QLabel(tr("Played threshold") + QStringLiteral(":"), this);

    const auto playedToolTip
        = tr("The percentage of a track that must be listened to before it is counted as 'played'");
    playedLabel->setToolTip(playedToolTip);
    m_playedSlider->setToolTip(playedToolTip);
    m_playedPercent->setToolTip(playedToolTip);

    m_playedSlider->setRange(0, 100);
    m_playedSlider->setSingleStep(25);

    m_playedPercent->setRange(0, 100);
    m_playedPercent->setSingleStep(25);
    m_playedPercent->setSuffix(QStringLiteral(" %"));

    QObject::connect(m_playedSlider, &QSlider::valueChanged, this,
                     [this](int value) { m_playedPercent->setValue(value); });
    QObject::connect(m_playedPercent, &QSpinBox::valueChanged, this,
                     [this](int value) { m_playedSlider->setValue(value); });

    auto* playedLayout = new QGridLayout();
    playedLayout->addWidget(playedLabel, 0, 0);
    playedLayout->addWidget(m_playedSlider, 0, 1);
    playedLayout->addWidget(m_playedPercent, 0, 2);
    playedLayout->setColumnStretch(1, 1);

    int row{0};
    generalGroupLayout->addWidget(m_restorePlayback, row++, 0, 1, 2);
    generalGroupLayout->addWidget(m_cursorFollowsPlayback, row++, 0, 1, 2);
    generalGroupLayout->addWidget(m_playbackFollowsCursor, row++, 0, 1, 2);
    generalGroupLayout->addWidget(m_rewindPrevious, row++, 0, 1, 2);
    generalGroupLayout->addWidget(m_skipUnavailable, row++, 0, 1, 2);
    generalGroupLayout->addLayout(playedLayout, row++, 0, 1, 2);

    auto* externalGroup       = new QGroupBox(tr("External files"), this);
    auto* externalGroupLayout = new QGridLayout(externalGroup);

    auto* playlistLabel = new QLabel(tr("Playlist name") + QStringLiteral(":"), this);

    externalGroupLayout->addWidget(m_alwaysSend, 0, 0, 1, 2);
    externalGroupLayout->addWidget(playlistLabel, 1, 0);
    externalGroupLayout->addWidget(m_externalPlaylist, 1, 1);

    layout->addWidget(generalGroup, 0, 0);
    layout->addWidget(externalGroup, 1, 0);
    layout->setRowStretch(2, 1);
}

void PlaybackPageWidget::load()
{
    m_restorePlayback->setChecked(m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool());
    m_cursorFollowsPlayback->setChecked(m_settings->value<Settings::Gui::CursorFollowsPlayback>());
    m_playbackFollowsCursor->setChecked(m_settings->value<Settings::Gui::PlaybackFollowsCursor>());
    m_rewindPrevious->setChecked(m_settings->value<Settings::Core::RewindPreviousTrack>());
    m_skipUnavailable->setChecked(
        m_settings->fileValue(Settings::Core::Internal::PlaylistSkipUnavailable, false).toBool());

    const double playedThreshold = m_settings->value<Settings::Core::PlayedThreshold>();
    const auto playedPercent     = static_cast<int>(playedThreshold * 100);
    m_playedSlider->setValue(playedPercent);
    m_playedPercent->setValue(playedPercent);

    m_alwaysSend->setChecked(m_settings->value<Settings::Core::OpenFilesSendTo>());
    m_externalPlaylist->setText(m_settings->value<Settings::Core::OpenFilesPlaylist>());
}

void PlaybackPageWidget::apply()
{
    m_settings->fileSet(Settings::Core::Internal::SavePlaybackState, m_restorePlayback->isChecked());
    m_settings->set<Settings::Gui::CursorFollowsPlayback>(m_cursorFollowsPlayback->isChecked());
    m_settings->set<Settings::Gui::PlaybackFollowsCursor>(m_playbackFollowsCursor->isChecked());
    m_settings->set<Settings::Core::RewindPreviousTrack>(m_rewindPrevious->isChecked());
    m_settings->fileSet(Settings::Core::Internal::PlaylistSkipUnavailable, m_skipUnavailable->isChecked());

    const int playedPercent    = m_playedPercent->value();
    const auto playedThreshold = static_cast<double>(playedPercent) / 100;
    m_settings->set<Settings::Core::PlayedThreshold>(playedThreshold);

    m_settings->set<Settings::Core::OpenFilesSendTo>(m_alwaysSend->isChecked());
    m_settings->set<Settings::Core::OpenFilesPlaylist>(m_externalPlaylist->text());
}

void PlaybackPageWidget::reset()
{
    m_settings->fileRemove(Settings::Core::Internal::SavePlaybackState);
    m_settings->reset<Settings::Gui::CursorFollowsPlayback>();
    m_settings->reset<Settings::Gui::PlaybackFollowsCursor>();
    m_settings->reset<Settings::Core::RewindPreviousTrack>();
    m_settings->reset<Settings::Core::RewindPreviousTrack>();
    m_settings->fileRemove(Settings::Core::Internal::PlaylistSkipUnavailable);
    m_settings->reset<Settings::Core::PlayedThreshold>();

    m_settings->reset<Settings::Core::OpenFilesSendTo>();
    m_settings->reset<Settings::Core::OpenFilesPlaylist>();
}

PlaybackPage::PlaybackPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Playback);
    setName(tr("General"));
    setCategory({tr("Playback")});
    setWidgetCreator([settings] { return new PlaybackPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playbackpage.cpp"
#include "playbackpage.moc"
