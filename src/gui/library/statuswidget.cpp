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

namespace Fooyin {
struct StatusWidget::Private
{
    StatusWidget* self;

    MusicLibrary* library;
    PlayerManager* playerManager;
    SettingsManager* settings;

    ClickableLabel* iconLabel;
    QPixmap icon;
    ClickableLabel* playing;

    Private(StatusWidget* self, MusicLibrary* library, PlayerManager* playerManager, SettingsManager* settings)
        : self{self}
        , library{library}
        , playerManager{playerManager}
        , settings{settings}
        , iconLabel{new ClickableLabel(self)}
        , icon{QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize)}
        , playing{new ClickableLabel(self)}
    { }

    void labelClicked() const
    {
        const PlayState ps = playerManager->playState();
        if(ps == PlayState::Playing || ps == PlayState::Paused) {
            QMetaObject::invokeMethod(self, &StatusWidget::clicked);
        }
    }

    // TODO: Make scriptable
    void updatePlayingText() const
    {
        const Track track         = playerManager->currentTrack();
        const QString playingText = Utils::addLeadingZero(track.trackNumber(), 2) + ". " + track.title() + "("
                                  + Utils::msToString(track.duration()) + ")" + " \u2022 " + track.albumArtist()
                                  + " \u2022 " + track.album();
        playing->setText(playingText);
    }

    void stateChanged(PlayState state) const
    {
        switch(state) {
            case(PlayState::Stopped):
                playing->setText(tr("Waiting for track..."));
                break;
            case(PlayState::Playing): {
                updatePlayingText();
                break;
            }
            case(PlayState::Paused):
                break;
        }
    }

    void scanProgressChanged(int progress) const
    {
        if(progress == 100) {
            QTimer::singleShot(2000, playing, &QLabel::clear);
        }
        const QString scanText = QStringLiteral("Scanning library: ") + QString::number(progress) + QStringLiteral("%");
        playing->setText(scanText);
    }
};

StatusWidget::StatusWidget(MusicLibrary* library, PlayerManager* playerManager, SettingsManager* settings,
                           QWidget* parent)
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

    QObject::connect(p->playing, &ClickableLabel::clicked, this, [this]() { p->labelClicked(); });
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(library, &MusicLibrary::scanProgress, this,
                     [this](int progress) { p->scanProgressChanged(progress); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() {
        p->icon = QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize);
        p->iconLabel->setPixmap(p->icon);
    });
}

StatusWidget::~StatusWidget() = default;

QString StatusWidget::name() const
{
    return QStringLiteral("Status");
}
} // namespace Fooyin

#include "moc_statuswidget.cpp"
