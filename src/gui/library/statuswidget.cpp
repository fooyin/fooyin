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

#include "statuswidget.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>

constexpr int IconSize = 50;

namespace Fy::Gui::Widgets {
struct StatusWidget::Private : QObject
{
    StatusWidget* self;

    Core::Library::MusicLibrary* library;
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;

    Utils::ClickableLabel* iconLabel;
    QPixmap icon;
    Utils::ClickableLabel* playing;

    Private(StatusWidget* self, Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
            Utils::SettingsManager* settings)
        : self{self}
        , library{library}
        , playerManager{playerManager}
        , settings{settings}
        , iconLabel{new Utils::ClickableLabel(self)}
        , icon{QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize)}
        , playing{new Utils::ClickableLabel(self)}
    {
        connect(playing, &Utils::ClickableLabel::clicked, this, &StatusWidget::Private::labelClicked);
        connect(playerManager, &Core::Player::PlayerManager::playStateChanged, this,
                &StatusWidget::Private::stateChanged);
        connect(library, &Core::Library::MusicLibrary::scanProgress, this, &StatusWidget::Private::scanProgressChanged);

        settings->subscribe<Settings::IconTheme>(this, [this]() {
            icon = QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize);
            iconLabel->setPixmap(icon);
        });
    }

    void labelClicked()
    {
        const Core::Player::PlayState ps = playerManager->playState();
        if(ps == Core::Player::PlayState::Playing || ps == Core::Player::PlayState::Paused) {
            emit self->clicked();
        }
    }

    void stateChanged(Core::Player::PlayState state)
    {
        switch(state) {
            case(Core::Player::PlayState::Stopped):
                playing->setText("Waiting for track...");
                break;
            case(Core::Player::PlayState::Playing): {
                const Core::Track track = playerManager->currentTrack();
                const auto playingText  = QString{"%1. %2 (%3) \u2022 %4 \u2022 %5"}.arg(
                    QStringLiteral("%1").arg(track.trackNumber(), 2, 10, QLatin1Char('0')), track.title(),
                    Utils::msToString(track.duration()), track.albumArtist(), track.album());
                playing->setText(playingText);
            }
            case(Core::Player::PlayState::Paused):
                break;
        }
    }

    void scanProgressChanged(int progress)
    {
        if(progress == 100) {
            QTimer::singleShot(2000, playing, &QLabel::clear);
        }
        const auto scanText = QString{"Scanning library: %1%"}.arg(progress);
        playing->setText(scanText);
    }
};

StatusWidget::StatusWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                           Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, library, playerManager, settings)}
{
    setObjectName("Status Bar");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 0, 0, 0);

    p->iconLabel->setPixmap(p->icon);
    p->iconLabel->setScaledContents(true);

    p->iconLabel->setMaximumHeight(22);
    p->iconLabel->setMaximumWidth(22);

    layout->addWidget(p->iconLabel);
    layout->addWidget(p->playing);

    setMinimumHeight(25);
}

StatusWidget::~StatusWidget() = default;

QString StatusWidget::name() const
{
    return "Status";
}
} // namespace Fy::Gui::Widgets
