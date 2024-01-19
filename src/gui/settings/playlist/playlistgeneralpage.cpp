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
#include <QLabel>
#include <QSpinBox>

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

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
    QSpinBox* m_thumbnailSize;
};

PlaylistGeneralPageWidget::PlaylistGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_cursorFollowsPlayback{new QCheckBox(tr("Cursor follows playback"), this)}
    , m_playbackFollowsCursor{new QCheckBox(tr("Playback follows cursor"), this)}
    , m_rewindPrevious{new QCheckBox(tr("Rewind track on previous"), this)}
    , m_rememberPlaylistState{new QCheckBox(tr("Remember playlist state"), this)}
    , m_hideSinglePlaylistTab{new QCheckBox(tr("Hide single playlist tab"), this)}
    , m_scrollBars{new QCheckBox(tr("Show Scrollbar"), this)}
    , m_header{new QCheckBox(tr("Show Header"), this)}
    , m_altColours{new QCheckBox(tr("Alternate Row Colours"), this)}
    , m_thumbnailSize{new QSpinBox(this)}
{
    auto* layout = new QGridLayout(this);

    m_rewindPrevious->setToolTip(tr(
        "If the current track has been playing for more than 5s, restart it instead of moving to the previous track"));

    m_thumbnailSize->setMinimum(1);
    m_thumbnailSize->setMaximum(300);
    m_thumbnailSize->setSuffix(QStringLiteral("px"));

    auto* behaviour       = new QGroupBox(tr("Behaviour"), this);
    auto* behaviourLayout = new QGridLayout(behaviour);

    behaviourLayout->addWidget(m_cursorFollowsPlayback, 0, 0, 1, 2);
    behaviourLayout->addWidget(m_playbackFollowsCursor, 1, 0, 1, 2);
    behaviourLayout->addWidget(m_rewindPrevious, 2, 0, 1, 2);
    behaviourLayout->addWidget(m_rememberPlaylistState, 3, 0, 1, 2);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    auto* rowHeightLabel = new QLabel(tr("Thumbnail Size:"), this);

    appearanceLayout->addWidget(m_scrollBars, 0, 0, 1, 2);
    appearanceLayout->addWidget(m_header, 1, 0, 1, 2);
    appearanceLayout->addWidget(m_altColours, 2, 0, 1, 2);
    appearanceLayout->addWidget(rowHeightLabel, 3, 0);
    appearanceLayout->addWidget(m_thumbnailSize, 3, 1);
    appearanceLayout->setColumnStretch(2, 1);
    appearanceLayout->setRowStretch(5, 1);

    auto* playlistTabs       = new QGroupBox(tr("Playlist Tabs"), this);
    auto* playlistTabsLayout = new QGridLayout(playlistTabs);

    playlistTabsLayout->addWidget(m_hideSinglePlaylistTab, 0, 0);

    layout->addWidget(behaviour, 0, 0);
    layout->addWidget(appearance, 0, 1);
    layout->addWidget(playlistTabs, 1, 0);

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
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
    m_settings->set<Settings::Gui::Internal::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistAltColours>(m_altColours->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistThumbnailSize>(m_thumbnailSize->value());
}

void PlaylistGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::CursorFollowsPlayback>();
    m_settings->reset<Settings::Gui::PlaybackFollowsCursor>();
    m_settings->reset<Settings::Core::RewindPreviousTrack>();
    m_settings->reset<Settings::Gui::RememberPlaylistState>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsHide>();
    m_settings->reset<Settings::Gui::Internal::PlaylistScrollBar>();
    m_settings->reset<Settings::Gui::Internal::PlaylistHeader>();
    m_settings->reset<Settings::Gui::Internal::PlaylistAltColours>();
    m_settings->reset<Settings::Gui::Internal::PlaylistThumbnailSize>();

    setValues();
}

void PlaylistGeneralPageWidget::setValues()
{
    m_cursorFollowsPlayback->setChecked(m_settings->value<Settings::Gui::CursorFollowsPlayback>());
    m_playbackFollowsCursor->setChecked(m_settings->value<Settings::Gui::PlaybackFollowsCursor>());
    m_rewindPrevious->setChecked(m_settings->value<Settings::Core::RewindPreviousTrack>());
    m_rememberPlaylistState->setChecked(m_settings->value<Settings::Gui::RememberPlaylistState>());
    m_hideSinglePlaylistTab->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsHide>());
    m_scrollBars->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistAltColours>());
    m_thumbnailSize->setValue(m_settings->value<Settings::Gui::Internal::PlaylistThumbnailSize>());
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
