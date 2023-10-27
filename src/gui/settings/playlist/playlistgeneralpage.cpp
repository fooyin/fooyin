/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistgeneralpage.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>

namespace Fy::Gui::Settings {
class PlaylistGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistGeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void setValues();

    Utils::SettingsManager* m_settings;

    QCheckBox* m_cursorFollowsPlayback;
    QCheckBox* m_playbackFollowsCursor;
    QCheckBox* m_rewindPrevious;
};

PlaylistGeneralPageWidget::PlaylistGeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_cursorFollowsPlayback{new QCheckBox(tr("Cursor follows playback"), this)}
    , m_playbackFollowsCursor{new QCheckBox(tr("Playback follows cursor"), this)}
    , m_rewindPrevious{new QCheckBox(tr("Rewind track on previous"), this)}
{
    auto* layout = new QGridLayout(this);

    m_rewindPrevious->setToolTip(tr(
        "If the current track has been playing for more than 5s, restart it instead of moving to the previous track"));

    layout->addWidget(m_cursorFollowsPlayback, 0, 0, 1, 2);
    layout->addWidget(m_playbackFollowsCursor, 1, 0, 1, 2);
    layout->addWidget(m_rewindPrevious, 2, 0, 1, 2);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(3, 1);

    setValues();
}

void PlaylistGeneralPageWidget::apply()
{
    m_settings->set<Settings::CursorFollowsPlayback>(m_cursorFollowsPlayback->isChecked());
    m_settings->set<Settings::PlaybackFollowsCursor>(m_playbackFollowsCursor->isChecked());
    m_settings->set<Core::Settings::RewindPreviousTrack>(m_rewindPrevious->isChecked());
}

void PlaylistGeneralPageWidget::reset()
{
    m_settings->reset<Settings::CursorFollowsPlayback>();
    m_settings->reset<Settings::PlaybackFollowsCursor>();
    m_settings->reset<Core::Settings::RewindPreviousTrack>();

    setValues();
}

void PlaylistGeneralPageWidget::setValues()
{
    m_cursorFollowsPlayback->setChecked(m_settings->value<Settings::CursorFollowsPlayback>());
    m_playbackFollowsCursor->setChecked(m_settings->value<Settings::PlaybackFollowsCursor>());
    m_rewindPrevious->setChecked(m_settings->value<Core::Settings::RewindPreviousTrack>());
}

PlaylistGeneralPage::PlaylistGeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistGeneral);
    setName(tr("General"));
    setCategory({tr("Playlist")});
    setWidgetCreator([settings] { return new PlaylistGeneralPageWidget(settings); });
}
} // namespace Fy::Gui::Settings
