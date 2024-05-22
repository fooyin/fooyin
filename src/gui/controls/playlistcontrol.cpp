/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcontrol.h"

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>

namespace Fooyin {
struct PlaylistControl::Private
{
    PlaylistControl* self;

    PlayerController* playerController;
    SettingsManager* settings;

    ToolButton* repeat;
    ToolButton* shuffle;

    Private(PlaylistControl* self_, PlayerController* playerController_, SettingsManager* settings_)
        : self{self_}
        , playerController{playerController_}
        , settings{settings_}
        , repeat{new ToolButton(self)}
        , shuffle{new ToolButton(self)}
    {
        repeat->setPopupMode(QToolButton::InstantPopup);

        auto* repeatAction = new QAction(self);
        repeatAction->setToolTip(tr("Repeat"));
        repeat->setDefaultAction(repeatAction);

        auto* shuffleAction = new QAction(self);
        shuffleAction->setToolTip(tr("Shuffle"));
        shuffle->setDefaultAction(shuffleAction);

        setMode(playerController->playMode());

        setupMenus();
        updateButtonStyle();
    }

    void updateButtonStyle() const
    {
        const auto options
            = static_cast<Settings::Gui::ToolButtonOptions>(settings->value<Settings::Gui::ToolButtonStyle>());

        repeat->setStretchEnabled(options & Settings::Gui::Stretch);
        repeat->setAutoRaise(!(options & Settings::Gui::Raise));

        shuffle->setStretchEnabled(options & Settings::Gui::Stretch);
        shuffle->setAutoRaise(!(options & Settings::Gui::Raise));
    }

    void setupMenus()
    {
        auto* menu = new QMenu(self);

        auto* repeatGroup = new QActionGroup(menu);

        auto* defaultAction  = new QAction(tr("Default"), repeatGroup);
        auto* repeatPlaylist = new QAction(tr("Repeat playlist"), repeatGroup);
        auto* repeatTrack    = new QAction(tr("Repeat track"), repeatGroup);

        defaultAction->setCheckable(true);
        repeatPlaylist->setCheckable(true);
        repeatTrack->setCheckable(true);

        auto playMode = playerController->playMode();

        if(playMode & Playlist::RepeatPlaylist) {
            repeatPlaylist->setChecked(true);
        }
        else if(playMode & Playlist::RepeatTrack) {
            repeatTrack->setChecked(true);
        }
        else {
            defaultAction->setChecked(true);
        }

        QObject::connect(defaultAction, &QAction::triggered, self, [this]() {
            const auto pMode = playerController->playMode();
            playerController->setPlayMode(pMode & ~Playlist::RepeatTrack & ~Playlist::RepeatPlaylist);
        });

        QObject::connect(repeatPlaylist, &QAction::triggered, self, [this]() {
            const auto pMode = playerController->playMode();
            playerController->setPlayMode((pMode & ~Playlist::RepeatTrack) | Playlist::RepeatPlaylist);
        });

        QObject::connect(repeatTrack, &QAction::triggered, self, [this]() {
            const auto pMode = playerController->playMode();
            playerController->setPlayMode((pMode & ~Playlist::RepeatPlaylist) | Playlist::RepeatTrack);
        });

        menu->addAction(defaultAction);
        menu->addAction(repeatPlaylist);
        menu->addAction(repeatTrack);

        repeat->setMenu(menu);
    }

    void shuffleClicked() const
    {
        Playlist::PlayModes mode = playerController->playMode();

        if(mode & Playlist::ShuffleTracks) {
            mode &= ~Playlist::ShuffleTracks;
        }
        else {
            mode |= Playlist::ShuffleTracks;
        }

        playerController->setPlayMode(mode);
    }

    void setMode(Playlist::PlayModes mode) const
    {
        if(mode & Playlist::RepeatTrack || mode & Playlist::RepeatPlaylist) {
            repeat->setIcon(Utils::changePixmapColour(Utils::iconFromTheme(Constants::Icons::Repeat).pixmap({128, 128}),
                                                      self->palette().highlight().color()));
        }
        else {
            repeat->setIcon(Utils::iconFromTheme(Constants::Icons::Repeat));
        }

        if(mode & Playlist::ShuffleTracks) {
            shuffle->setIcon(
                Utils::changePixmapColour(Utils::iconFromTheme(Constants::Icons::Shuffle).pixmap({128, 128}),
                                          self->palette().highlight().color()));
        }
        else {
            shuffle->setIcon(Utils::iconFromTheme(Constants::Icons::Shuffle));
        }
    }
};

PlaylistControl::PlaylistControl(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerController, settings)}

{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(p->repeat);
    layout->addWidget(p->shuffle);

    QObject::connect(p->shuffle, &QToolButton::clicked, this, [this]() { p->shuffleClicked(); });
    QObject::connect(playerController, &PlayerController::playModeChanged, this,
                     [this](Playlist::PlayModes mode) { p->setMode(mode); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { p->setMode(p->playerController->playMode()); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, [this]() { p->updateButtonStyle(); });
}

PlaylistControl::~PlaylistControl() = default;

QString PlaylistControl::name() const
{
    return tr("Playlist Controls");
}

QString PlaylistControl::layoutName() const
{
    return QStringLiteral("PlaylistControls");
}
} // namespace Fooyin

#include "moc_playlistcontrol.cpp"
