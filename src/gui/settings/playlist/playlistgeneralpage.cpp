/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>

namespace Fooyin {
class PlaylistGeneralPageWidget : public SettingsPageWidget
{
public:
    explicit PlaylistGeneralPageWidget(SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void setValues();

    SettingsManager* m_settings;

    QCheckBox* m_cursorFollowsPlayback;
    QCheckBox* m_playbackFollowsCursor;
    QCheckBox* m_rewindPrevious;
    QCheckBox* m_rememberPlaylistState;
    QCheckBox* m_hideSinglePlaylistTab;
};

PlaylistGeneralPageWidget::PlaylistGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_cursorFollowsPlayback{new QCheckBox(tr("Cursor follows playback"), this)}
    , m_playbackFollowsCursor{new QCheckBox(tr("Playback follows cursor"), this)}
    , m_rewindPrevious{new QCheckBox(tr("Rewind track on previous"), this)}
    , m_rememberPlaylistState{new QCheckBox(tr("Remember playlist state"), this)}
    , m_hideSinglePlaylistTab{new QCheckBox(tr("Hide single playlist tab"), this)}
{
    auto* layout = new QGridLayout(this);

    m_rewindPrevious->setToolTip(tr(
        "If the current track has been playing for more than 5s, restart it instead of moving to the previous track"));

    auto* playlistTabs       = new QGroupBox(tr("Playlist Tabs"), this);
    auto* playlistTabsLayout = new QGridLayout(playlistTabs);

    playlistTabsLayout->addWidget(m_hideSinglePlaylistTab, 0, 0);

    layout->addWidget(m_cursorFollowsPlayback, 0, 0, 1, 2);
    layout->addWidget(m_playbackFollowsCursor, 1, 0, 1, 2);
    layout->addWidget(m_rewindPrevious, 2, 0, 1, 2);
    layout->addWidget(m_rememberPlaylistState, 3, 0, 1, 2);
    layout->addWidget(playlistTabs, 4, 0, 1, 2);

    layout->setColumnStretch(layout->columnCount(), 1);
    layout->setRowStretch(layout->rowCount(), 1);

    setValues();
}

void PlaylistGeneralPageWidget::apply()
{
    m_settings->set<Settings::Gui::CursorFollowsPlayback>(m_cursorFollowsPlayback->isChecked());
    m_settings->set<Settings::Gui::PlaybackFollowsCursor>(m_playbackFollowsCursor->isChecked());
    m_settings->set<Settings::Core::RewindPreviousTrack>(m_rewindPrevious->isChecked());
    m_settings->set<Settings::Gui::RememberPlaylistState>(m_rememberPlaylistState->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistTabsHide>(m_hideSinglePlaylistTab->isChecked());
}

void PlaylistGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::CursorFollowsPlayback>();
    m_settings->reset<Settings::Gui::PlaybackFollowsCursor>();
    m_settings->reset<Settings::Core::RewindPreviousTrack>();
    m_settings->reset<Settings::Gui::RememberPlaylistState>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsHide>();

    setValues();
}

void PlaylistGeneralPageWidget::setValues()
{
    m_cursorFollowsPlayback->setChecked(m_settings->value<Settings::Gui::CursorFollowsPlayback>());
    m_playbackFollowsCursor->setChecked(m_settings->value<Settings::Gui::PlaybackFollowsCursor>());
    m_rewindPrevious->setChecked(m_settings->value<Settings::Core::RewindPreviousTrack>());
    m_rememberPlaylistState->setChecked(m_settings->value<Settings::Gui::RememberPlaylistState>());
    m_hideSinglePlaylistTab->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsHide>());
}

PlaylistGeneralPage::PlaylistGeneralPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistGeneral);
    setName(tr("General"));
    setCategory({tr("Playlist")});
    setWidgetCreator([settings] { return new PlaylistGeneralPageWidget(settings); });
}
} // namespace Fooyin
