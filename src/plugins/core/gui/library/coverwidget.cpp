/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "coverwidget.h"

#include "core/library/models/track.h"
#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "core/widgets/widgetprovider.h"

#include <QHBoxLayout>
#include <QLabel>
#include <pluginsystem/pluginmanager.h>

struct CoverWidget::Private
{
    PlayerManager* playerManager;
    Library::MusicLibrary* library;

    QHBoxLayout* layout;
    QLabel* coverLabel;
    QString coverPath;
    QPixmap cover;
    bool hasCover{false};

    explicit Private()
        : playerManager(PluginSystem::object<PlayerManager>())
        , library(PluginSystem::object<Library::MusicLibrary>())
    { }
};

CoverWidget::CoverWidget(QWidget* parent)
    : FyWidget(parent)
    , p(std::make_unique<Private>())
{
    setObjectName("Artwork");
    setupUi();

    connect(p->playerManager, &PlayerManager::currentTrackChanged, this, &CoverWidget::reloadCover);
    connect(p->library, &Library::MusicLibrary::tracksChanged, this, &CoverWidget::reloadCover);
    connect(p->library, &Library::MusicLibrary::tracksSelChanged, this, &CoverWidget::reloadCover);

    reloadCover();
}

QString CoverWidget::name() const
{
    return "Artwork";
}

void CoverWidget::layoutEditingMenu(QMenu* menu)
{
    Q_UNUSED(menu)
}

CoverWidget::~CoverWidget() = default;

void CoverWidget::setupUi()
{
    p->layout = new QHBoxLayout(this);
    p->layout->setAlignment(Qt::AlignCenter);

    setAutoFillBackground(true);

    p->coverLabel = new QLabel(this);

    p->coverLabel->setMinimumSize(100, 100);

    p->layout->addWidget(p->coverLabel);

    p->layout->setContentsMargins(0, 0, 0, 0);
}

void CoverWidget::resizeEvent(QResizeEvent* e)
{
    if(p->hasCover) {
        p->coverLabel->setPixmap(p->cover.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QWidget::resizeEvent(e);
}

void CoverWidget::reloadCover()
{
    QString coverPath = "";
    if(!p->library->tracks().empty()) {
        const Player::PlayState state = p->playerManager->playState();
        if(state == Player::PlayState::Playing || state == Player::PlayState::Paused) {
            Track* track = p->playerManager->currentTrack();
            if(track) {
                coverPath = track->coverPath();
            }
        }
        else if(p->library->selectedTracks().empty()) {
            Track* track = p->library->tracks().front();
            if(track) {
                coverPath = track->coverPath();
            }
        }
        else {
            Track* track = p->library->selectedTracks().front();
            if(track) {
                coverPath = track->coverPath();
            }
        }
    }

    if(coverPath.isEmpty()) {
        coverPath = "://images/nocover.png";
        //        setAutoFillBackground(true);
        //        QPalette palette = p->coverLabel->palette();
        //        palette.setColor(p->coverLabel->backgroundRole(), palette.base().color());
        //        p->coverLabel->setPalette(palette);
    }

    if(coverPath != p->coverPath) {
        p->coverPath = coverPath;
        p->cover.load(coverPath);
        if(!p->cover.isNull()) {
            p->coverLabel->setPixmap(p->cover.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            p->hasCover = true;
        }
    }
}
