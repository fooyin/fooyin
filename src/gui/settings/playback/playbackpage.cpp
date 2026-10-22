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

#include "playbackpage.h"

#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/abrepeatcontrols.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpacerItem>

using namespace Qt::StringLiterals;

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
    void updateWidgetState();

    SettingsManager* m_settings;

    QCheckBox* m_restoreActivePlaylistState;
    QCheckBox* m_restorePlaybackState;

    QCheckBox* m_cursorFollowsPlayback;
    QCheckBox* m_playbackFollowsCursor;

    QCheckBox* m_stopAfterCurrent;
    QCheckBox* m_resetStopAfterCurrent;

    QCheckBox* m_rewindPrevious;
    QCheckBox* m_skipUnavailable;
    QCheckBox* m_stopIfActiveDeleted;

    QSpinBox* m_seekStep;
    QSpinBox* m_seekStepLarge;
    QDoubleSpinBox* m_volumeStep;
    QDoubleSpinBox* m_volumeControlStepSmall;
    QDoubleSpinBox* m_volumeControlStepLarge;

    QCheckBox* m_abRepeatSeekToStartOnChange;
    QDoubleSpinBox* m_abRepeatMarkerWidth;
    ColourButton* m_abRepeatMarkerColourA;
    ColourButton* m_abRepeatMarkerColourB;

    SliderEditor* m_playedSlider;
    SliderEditor* m_playedTimeSlider;
    ScriptLineEdit* m_shuffleAlbumsGroup;
    ScriptLineEdit* m_shuffleAlbumsSort;
};

PlaybackPageWidget::PlaybackPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_restoreActivePlaylistState{new QCheckBox(tr("Save/restore active playlist state"), this)}
    , m_restorePlaybackState{new QCheckBox(tr("Save/restore playback state"), this)}
    , m_cursorFollowsPlayback{new QCheckBox(tr("Cursor follows playback"), this)}
    , m_playbackFollowsCursor{new QCheckBox(tr("Playback follows cursor"), this)}
    , m_stopAfterCurrent{new QCheckBox(tr("Stop playback after the current track"), this)}
    , m_resetStopAfterCurrent{new QCheckBox(tr("Reset the above after stopping"), this)}
    , m_rewindPrevious{new QCheckBox(tr("Rewind track on previous"), this)}
    , m_skipUnavailable{new QCheckBox(tr("Skip unavailable tracks"), this)}
    , m_stopIfActiveDeleted{new QCheckBox(tr("Stop playback if the active playlist is deleted"), this)}
    , m_seekStep{new QSpinBox(this)}
    , m_seekStepLarge{new QSpinBox(this)}
    , m_volumeStep{new QDoubleSpinBox(this)}
    , m_volumeControlStepSmall{new QDoubleSpinBox(this)}
    , m_volumeControlStepLarge{new QDoubleSpinBox(this)}
    , m_abRepeatSeekToStartOnChange{new QCheckBox(tr("Seek to A after adjusting the A marker"), this)}
    , m_abRepeatMarkerWidth{new QDoubleSpinBox(this)}
    , m_abRepeatMarkerColourA{new ColourButton(tr("A colour") + u":"_s, true, this)}
    , m_abRepeatMarkerColourB{new ColourButton(tr("B colour") + u":"_s, true, this)}
    , m_playedSlider{new SliderEditor(tr("Played threshold (%)"), this)}
    , m_playedTimeSlider{new SliderEditor(tr("Played threshold (time)"), this)}
    , m_shuffleAlbumsGroup{new ScriptLineEdit(this)}
    , m_shuffleAlbumsSort{new ScriptLineEdit(this)}
{
    auto* layout = new QGridLayout(this);

    m_restoreActivePlaylistState->setToolTip(tr("Save active playlist state on exit and restore it on next startup"));
    m_restorePlaybackState->setToolTip(tr("Save playback state on exit and restore it on next startup"));
    m_rewindPrevious->setToolTip(tr(
        "If the current track has been playing for more than 5 s, restart it instead of moving to the previous track"));
    m_skipUnavailable->setToolTip(
        tr("If the current track in a playlist is unavailable, silently continue to the next track"));

    QObject::connect(m_restoreActivePlaylistState, &QCheckBox::toggled, this, &PlaybackPageWidget::updateWidgetState);
    QObject::connect(m_stopAfterCurrent, &QCheckBox::toggled, this, &PlaybackPageWidget::updateWidgetState);

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    const auto playedToolTip = tr("Track is counted as ‘played’ once either threshold is reached");
    m_playedSlider->setToolTip(playedToolTip);
    m_playedTimeSlider->setToolTip(playedToolTip);

    m_playedSlider->setRange(0, 100);
    m_playedSlider->setSingleStep(25);
    m_playedSlider->setSuffix(u" %"_s);

    m_playedTimeSlider->setRange(30, 600);
    m_playedTimeSlider->setSingleStep(30);
    m_playedTimeSlider->setSuffix(u" s"_s);

    int row{0};
    generalGroupLayout->addWidget(m_restoreActivePlaylistState, row, 0, 1, 2);
    generalGroupLayout->addWidget(m_stopAfterCurrent, row++, 2, 1, 2);
    generalGroupLayout->addItem(new QSpacerItem(10, 0), row, 0);
    generalGroupLayout->addWidget(m_restorePlaybackState, row, 1);
    generalGroupLayout->addItem(new QSpacerItem(10, 0), row, 2);
    generalGroupLayout->addWidget(m_resetStopAfterCurrent, row++, 3);
    generalGroupLayout->addWidget(m_cursorFollowsPlayback, row, 0, 1, 2);
    generalGroupLayout->addWidget(m_rewindPrevious, row++, 2, 1, 2);
    generalGroupLayout->addWidget(m_playbackFollowsCursor, row, 0, 1, 2);
    generalGroupLayout->addWidget(m_skipUnavailable, row++, 2, 1, 2);
    generalGroupLayout->addWidget(m_stopIfActiveDeleted, row++, 0, 1, 4);
    generalGroupLayout->addWidget(m_playedSlider, row++, 0, 1, 4);
    generalGroupLayout->addWidget(m_playedTimeSlider, row++, 0, 1, 4);
    generalGroupLayout->setColumnStretch(1, 1);
    generalGroupLayout->setColumnStretch(3, 1);

    auto* controlsGroup  = new QGroupBox(tr("Controls"), this);
    auto* controlsLayout = new QGridLayout(controlsGroup);

    m_seekStep->setRange(100, 30000);
    m_seekStep->setSuffix(u" ms"_s);

    m_seekStepLarge->setRange(100, 60000);
    m_seekStepLarge->setSuffix(u" ms"_s);

    m_volumeStep->setRange(1, 5);
    m_volumeStep->setSuffix(u" dB"_s);

    m_volumeControlStepSmall->setRange(0.1, 100);
    m_volumeControlStepSmall->setSingleStep(0.5);
    m_volumeControlStepSmall->setSuffix(u" %"_s);

    m_volumeControlStepLarge->setRange(0.1, 100);
    m_volumeControlStepLarge->setSingleStep(0.5);
    m_volumeControlStepLarge->setSuffix(u" %"_s);

    row = 0;
    controlsLayout->addWidget(new QLabel(tr("Seek step (small)") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_seekStep, row++, 1);
    controlsLayout->addWidget(new QLabel(tr("Seek step (large)") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_seekStepLarge, row++, 1);
    controlsLayout->addWidget(new QLabel(tr("Volume shortcut step") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_volumeStep, row++, 1);
    controlsLayout->addWidget(new QLabel(tr("Volume control step (small)") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_volumeControlStepSmall, row++, 1);
    controlsLayout->addWidget(new QLabel(tr("Volume control step (large)") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_volumeControlStepLarge, row++, 1);
    controlsLayout->setColumnStretch(2, 1);

    m_abRepeatMarkerWidth->setRange(0.5, 10.0);
    m_abRepeatMarkerWidth->setDecimals(1);
    m_abRepeatMarkerWidth->setSingleStep(0.5);
    m_abRepeatMarkerWidth->setSuffix(u" px"_s);
    m_abRepeatMarkerColourA->setToolTip(tr("Override the default colour for the A marker"));
    m_abRepeatMarkerColourB->setToolTip(tr("Override the default colour for the B marker"));
    ColourButton::alignLabels({m_abRepeatMarkerColourA, m_abRepeatMarkerColourB});

    auto* repeatGroup  = new QGroupBox(tr("A–B repeat"), this);
    auto* repeatLayout = new QGridLayout(repeatGroup);

    repeatGroup->setToolTip(tr("A–B repeat settings for all seekbars"));
    m_abRepeatSeekToStartOnChange->setToolTip(tr("Seek on release when dragging A while A–B repeat is enabled"));

    row = 0;
    repeatLayout->addWidget(m_abRepeatSeekToStartOnChange, row++, 0, 1, 3);
    repeatLayout->addWidget(new QLabel(tr("Line width") + u":"_s, this), row, 0);
    repeatLayout->addWidget(m_abRepeatMarkerWidth, row++, 1);
    repeatLayout->addWidget(m_abRepeatMarkerColourA, row++, 0, 1, 2);
    repeatLayout->addWidget(m_abRepeatMarkerColourB, row++, 0, 1, 2);
    repeatLayout->setColumnStretch(2, 1);

    auto* shuffleGroup       = new QGroupBox(tr("Shuffle"), this);
    auto* shuffleGroupLayout = new QGridLayout(shuffleGroup);

    shuffleGroupLayout->addWidget(new QLabel(tr("Album grouping pattern") + ":"_L1, this), 0, 0);
    shuffleGroupLayout->addWidget(m_shuffleAlbumsGroup, 0, 1);
    shuffleGroupLayout->addWidget(new QLabel(tr("Album sorting pattern") + ":"_L1, this), 1, 0);
    shuffleGroupLayout->addWidget(m_shuffleAlbumsSort, 1, 1);
    shuffleGroupLayout->setColumnStretch(1, 1);

    row = 0;
    layout->addWidget(generalGroup, row++, 0, 1, 2);
    layout->addWidget(controlsGroup, row, 0, Qt::AlignTop);
    layout->addWidget(repeatGroup, row++, 1, Qt::AlignTop);
    layout->addWidget(shuffleGroup, row++, 0, 1, 2);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(row, 1);

    m_settings->subscribe<Settings::Gui::CursorFollowsPlayback>(m_cursorFollowsPlayback, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Gui::PlaybackFollowsCursor>(m_playbackFollowsCursor, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Core::StopAfterCurrent>(this, [this](const bool enabled) {
        m_stopAfterCurrent->setChecked(enabled);
        updateWidgetState();
    });
    m_settings->subscribe<Settings::Core::ResetStopAfterCurrent>(m_resetStopAfterCurrent, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Core::Internal::PlaylistSkipUnavailable>(m_skipUnavailable, &QCheckBox::setChecked);
}

void PlaybackPageWidget::load()
{
    m_restoreActivePlaylistState->setChecked(
        m_settings->fileValue(Settings::Core::Internal::SaveActivePlaylistState, false).toBool());
    m_restorePlaybackState->setChecked(
        m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool());
    m_cursorFollowsPlayback->setChecked(m_settings->value<Settings::Gui::CursorFollowsPlayback>());
    m_playbackFollowsCursor->setChecked(m_settings->value<Settings::Gui::PlaybackFollowsCursor>());
    m_stopAfterCurrent->setChecked(m_settings->value<Settings::Core::StopAfterCurrent>());
    m_resetStopAfterCurrent->setChecked(m_settings->value<Settings::Core::ResetStopAfterCurrent>());
    m_rewindPrevious->setChecked(m_settings->value<Settings::Core::RewindPreviousTrack>());
    m_skipUnavailable->setChecked(m_settings->value<Settings::Core::Internal::PlaylistSkipUnavailable>());
    m_stopIfActiveDeleted->setChecked(m_settings->value<Settings::Core::StopIfActivePlaylistDeleted>());

    m_seekStep->setValue(m_settings->value<Settings::Gui::SeekStepSmall>());
    m_seekStepLarge->setValue(m_settings->value<Settings::Gui::SeekStepLarge>());
    m_volumeStep->setValue(m_settings->value<Settings::Gui::VolumeStep>());
    m_volumeControlStepSmall->setValue(m_settings->value<Settings::Gui::VolumeControlStepSmall>());
    m_volumeControlStepLarge->setValue(m_settings->value<Settings::Gui::VolumeControlStepLarge>());

    m_abRepeatSeekToStartOnChange->setChecked(m_settings->value<Settings::Gui::ABRepeatSeekOnAChange>());
    m_abRepeatMarkerWidth->setValue(m_settings->value<Settings::Gui::ABRepeatMarkerWidth>());
    const QColor markerColourA = m_settings->value<Settings::Gui::ABRepeatMarkerColourA>().value<QColor>();
    m_abRepeatMarkerColourA->setChecked(markerColourA.isValid());
    m_abRepeatMarkerColourA->setColour(markerColourA.isValid() ? markerColourA
                                                               : ABRepeatControls::defaultColourA(palette()));
    const QColor markerColourB = m_settings->value<Settings::Gui::ABRepeatMarkerColourB>().value<QColor>();
    m_abRepeatMarkerColourB->setChecked(markerColourB.isValid());
    m_abRepeatMarkerColourB->setColour(markerColourB.isValid() ? markerColourB
                                                               : ABRepeatControls::defaultColourB(palette()));

    const double playedThreshold = m_settings->value<Settings::Core::PlayedThreshold>();
    const auto playedPercent     = static_cast<int>(playedThreshold * 100);
    m_playedSlider->setValue(playedPercent);
    m_playedTimeSlider->setValue(m_settings->value<Settings::Core::PlayedThresholdTime>() / 1000);

    m_shuffleAlbumsGroup->setScript(m_settings->value<Settings::Core::ShuffleAlbumsGroupScript>());
    m_shuffleAlbumsSort->setScript(m_settings->value<Settings::Core::ShuffleAlbumsSortScript>());

    updateWidgetState();
}

void PlaybackPageWidget::apply()
{
    m_settings->fileSet(Settings::Core::Internal::SaveActivePlaylistState, m_restoreActivePlaylistState->isChecked());
    m_settings->fileSet(Settings::Core::Internal::SavePlaybackState, m_restorePlaybackState->isChecked());
    m_settings->set<Settings::Gui::CursorFollowsPlayback>(m_cursorFollowsPlayback->isChecked());
    m_settings->set<Settings::Gui::PlaybackFollowsCursor>(m_playbackFollowsCursor->isChecked());
    m_settings->set<Settings::Core::StopAfterCurrent>(m_stopAfterCurrent->isChecked());
    m_settings->set<Settings::Core::ResetStopAfterCurrent>(m_resetStopAfterCurrent->isChecked());
    m_settings->set<Settings::Core::RewindPreviousTrack>(m_rewindPrevious->isChecked());
    m_settings->set<Settings::Core::Internal::PlaylistSkipUnavailable>(m_skipUnavailable->isChecked());
    m_settings->set<Settings::Core::StopIfActivePlaylistDeleted>(m_stopIfActiveDeleted->isChecked());

    m_settings->set<Settings::Gui::SeekStepSmall>(m_seekStep->value());
    m_settings->set<Settings::Gui::SeekStepLarge>(m_seekStepLarge->value());
    m_settings->set<Settings::Gui::VolumeStep>(m_volumeStep->value());
    m_settings->set<Settings::Gui::VolumeControlStepSmall>(m_volumeControlStepSmall->value());
    m_settings->set<Settings::Gui::VolumeControlStepLarge>(m_volumeControlStepLarge->value());

    m_settings->set<Settings::Gui::ABRepeatSeekOnAChange>(m_abRepeatSeekToStartOnChange->isChecked());
    m_settings->set<Settings::Gui::ABRepeatMarkerWidth>(m_abRepeatMarkerWidth->value());
    if(m_abRepeatMarkerColourA->isChecked()) {
        m_settings->set<Settings::Gui::ABRepeatMarkerColourA>(m_abRepeatMarkerColourA->colour());
    }
    else {
        m_settings->reset<Settings::Gui::ABRepeatMarkerColourA>();
    }
    if(m_abRepeatMarkerColourB->isChecked()) {
        m_settings->set<Settings::Gui::ABRepeatMarkerColourB>(m_abRepeatMarkerColourB->colour());
    }
    else {
        m_settings->reset<Settings::Gui::ABRepeatMarkerColourB>();
    }

    const int playedPercent    = m_playedSlider->value();
    const auto playedThreshold = static_cast<double>(playedPercent) / 100;
    m_settings->set<Settings::Core::PlayedThreshold>(playedThreshold);
    m_settings->set<Settings::Core::PlayedThresholdTime>(m_playedTimeSlider->value() * 1000);

    m_settings->set<Settings::Core::ShuffleAlbumsGroupScript>(m_shuffleAlbumsGroup->text());
    m_settings->set<Settings::Core::ShuffleAlbumsSortScript>(m_shuffleAlbumsSort->text());
}

void PlaybackPageWidget::reset()
{
    m_settings->fileRemove(Settings::Core::Internal::SaveActivePlaylistState);
    m_settings->fileRemove(Settings::Core::Internal::SavePlaybackState);
    m_settings->reset<Settings::Gui::CursorFollowsPlayback>();
    m_settings->reset<Settings::Gui::PlaybackFollowsCursor>();
    m_settings->reset<Settings::Core::StopAfterCurrent>();
    m_settings->reset<Settings::Core::ResetStopAfterCurrent>();
    m_settings->reset<Settings::Core::RewindPreviousTrack>();
    m_settings->reset<Settings::Core::Internal::PlaylistSkipUnavailable>();
    m_settings->reset<Settings::Core::StopIfActivePlaylistDeleted>();
    m_settings->reset<Settings::Gui::SeekStepSmall>();
    m_settings->reset<Settings::Gui::SeekStepLarge>();
    m_settings->reset<Settings::Gui::VolumeStep>();
    m_settings->reset<Settings::Gui::VolumeControlStepSmall>();
    m_settings->reset<Settings::Gui::VolumeControlStepLarge>();
    m_settings->reset<Settings::Gui::ABRepeatSeekOnAChange>();
    m_settings->reset<Settings::Gui::ABRepeatMarkerWidth>();
    m_settings->reset<Settings::Gui::ABRepeatMarkerColourA>();
    m_settings->reset<Settings::Gui::ABRepeatMarkerColourB>();
    m_settings->reset<Settings::Core::PlayedThreshold>();
    m_settings->reset<Settings::Core::PlayedThresholdTime>();
    m_settings->reset<Settings::Core::ShuffleAlbumsGroupScript>();
    m_settings->reset<Settings::Core::ShuffleAlbumsSortScript>();
}

void PlaybackPageWidget::updateWidgetState()
{
    m_restorePlaybackState->setEnabled(m_restoreActivePlaylistState->isChecked());
    m_resetStopAfterCurrent->setEnabled(m_stopAfterCurrent->isChecked());
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
