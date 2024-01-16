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

#include "statuswidget.h"

#include "internalguisettings.h"

#include <core/player/playermanager.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>

constexpr int IconSize = 50;

using namespace std::chrono_literals;

namespace Fooyin {
struct StatusWidget::Private
{
    StatusWidget* self;

    PlayerManager* playerManager;
    SettingsManager* settings;

    ScriptRegistry scriptRegistry;
    ScriptParser scriptParser;

    ClickableLabel* iconLabel;
    QPixmap icon;
    ClickableLabel* statusText;

    QString playingScript;

    QTimer clearTimer;

    Private(StatusWidget* self, PlayerManager* playerManager, SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , selectionController{selectionController}
        , settings{settings}
        , scriptParser{&scriptRegistry}
        , iconLabel{new ClickableLabel(self)}
        , icon{QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize)}
        , statusText{new ClickableLabel(self)}
        , selectionText{new ClickableLabel(self)}
    {
        clearTimer.setInterval(2s);
        clearTimer.setSingleShot(true);
        QObject::connect(&clearTimer, &QTimer::timeout, statusText, &QLabel::clear);

        updateScripts();
        updatePlayingText();
    }

    void updateScripts()
    {
        playingScript = settings->value<Settings::Gui::Internal::StatusPlayingScript>();
    }

    void labelClicked() const
    {
        const PlayState ps = playerManager->playState();
        if(ps == PlayState::Playing || ps == PlayState::Paused) {
            QMetaObject::invokeMethod(self, &StatusWidget::clicked);
        }
    }

    void updatePlayingText()
    {
        const PlayState ps = playerManager->playState();
        if(ps == PlayState::Playing || ps == PlayState::Paused) {
            statusText->setText(scriptParser.evaluate(playingScript, playerManager->currentTrack()));
        }
    }

    void stateChanged(const PlayState state)
    {
        switch(state) {
            case(PlayState::Stopped):
                statusText->setText("");
                break;
            case(PlayState::Playing): {
                updatePlayingText();
                break;
            }
            case(PlayState::Paused):
                break;
        }
    }
};

StatusWidget::StatusWidget(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    setObjectName(StatusWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 0, 0, 0);

    p->iconLabel->setPixmap(p->icon);
    p->iconLabel->setScaledContents(true);

    p->iconLabel->setMaximumHeight(22);
    p->iconLabel->setMaximumWidth(22);

    layout->addWidget(p->iconLabel);
    layout->addWidget(p->statusText);

    p->iconLabel->setHidden(!p->settings->value<Settings::Gui::Internal::StatusShowIcon>());

    QObject::connect(p->statusText, &ClickableLabel::clicked, this, [this]() { p->labelClicked(); });
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(playerManager, &PlayerManager::positionChanged, this,
                     [this](uint64_t /*pos*/) { p->updatePlayingText(); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() {
        p->icon = QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize);
        p->iconLabel->setPixmap(p->icon);
    });

    settings->subscribe<Settings::Gui::Internal::StatusShowIcon>(this,
                                                                 [this](bool show) { p->iconLabel->setHidden(!show); });
    settings->subscribe<Settings::Gui::Internal::StatusPlayingScript>(this, [this](const QString& script) {
        p->playingScript = script;
        p->updatePlayingText();
    });
}

StatusWidget::~StatusWidget() = default;

QString StatusWidget::name() const
{
    return QStringLiteral("Status Bar");
}

QString StatusWidget::layoutName() const
{
    return QStringLiteral("StatusBar");
}

void StatusWidget::libraryScanProgress(int /*id*/, int progress)
{
    if(progress == 100) {
        p->clearTimer.start();
    }
    else {
        p->clearTimer.stop();
    }

    const QString scanText = QStringLiteral("Scanning library: ") + QString::number(progress) + QStringLiteral("%");
    p->statusText->setText(scanText);
}

void StatusWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showIcon = new QAction(tr("Show Icon"), this);
    showIcon->setCheckable(true);
    showIcon->setChecked(p->settings->value<Settings::Gui::Internal::StatusShowIcon>());
    QObject::connect(showIcon, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::StatusShowIcon>(checked); });
    menu->addAction(showIcon);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_statuswidget.cpp"
