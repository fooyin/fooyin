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

#include "statuswidget.h"

#include "internalguisettings.h"

#include <core/player/playercontroller.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/elidedlabel.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>

constexpr int IconSize = 50;

namespace Fooyin {
struct StatusWidget::Private
{
    StatusWidget* self;

    PlayerController* playerController;
    TrackSelectionController* selectionController;

    SettingsManager* settings;

    ScriptRegistry scriptRegistry;
    ScriptParser scriptParser;

    ClickableLabel* iconLabel;
    ElidedLabel* statusText;
    ElidedLabel* messageText;
    ClickableLabel* selectionText;

    QString playingScript;
    QString selectionScript;

    QTimer clearTimer;
    QString tempText;

    Private(StatusWidget* self_, PlayerController* playerController_, TrackSelectionController* selectionController_,
            SettingsManager* settings_)
        : self{self_}
        , playerController{playerController_}
        , selectionController{selectionController_}
        , settings{settings_}
        , scriptRegistry{playerController}
        , scriptParser{&scriptRegistry}
        , iconLabel{new ClickableLabel(self)}
        , statusText{new ElidedLabel(self)}
        , messageText{new ElidedLabel(self)}
        , selectionText{new ClickableLabel(self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(5, 0, 5, 0);

        iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize));
        iconLabel->setScaledContents(true);

        iconLabel->setMaximumHeight(22);
        iconLabel->setMaximumWidth(22);

        layout->addWidget(iconLabel);
        layout->addWidget(messageText, 1);
        layout->addWidget(statusText, 1);
        layout->addWidget(selectionText);

        messageText->hide();

        iconLabel->setHidden(!settings->value<Settings::Gui::Internal::StatusShowIcon>());
        selectionText->setHidden(!settings->value<Settings::Gui::Internal::StatusShowSelection>());

        updateScripts();
        updatePlayingText();

        QObject::connect(playerController, &PlayerController::playStateChanged, self,
                         [this](PlayState state) { stateChanged(state); });
        QObject::connect(playerController, &PlayerController::positionChanged, self,
                         [this](uint64_t /*pos*/) { updatePlayingText(); });
        QObject::connect(selectionController, &TrackSelectionController::selectionChanged, self,
                         [this]() { updateSelectionText(); });

        clearTimer.setSingleShot(true);
        QObject::connect(&clearTimer, &QTimer::timeout, self, [this]() { clearMessage(); });
    }

    void clearMessage()
    {
        clearTimer.stop();

        messageText->clear();
        messageText->hide();
        statusText->show();
    }

    void showMessage(const QString& message, int timeout = 0)
    {
        messageText->setText(message);
        statusText->hide();
        messageText->show();

        if(timeout > 0) {
            clearTimer.start(timeout);
        }
    }

    void updateScripts()
    {
        playingScript   = settings->value<Settings::Gui::Internal::StatusPlayingScript>();
        selectionScript = settings->value<Settings::Gui::Internal::StatusSelectionScript>();
    }

    void updatePlayingText()
    {
        const PlayState ps = playerController->playState();
        if(ps == PlayState::Playing || ps == PlayState::Paused) {
            statusText->setText(scriptParser.evaluate(playingScript, playerController->currentTrack()));
        }
    }

    void updateScanText(int progress)
    {
        const auto scanText = QStringLiteral("Scanning library: %1%").arg(progress);
        showMessage(scanText, 5000);
    }

    void updateSelectionText()
    {
        selectionText->setText(scriptParser.evaluate(selectionScript, selectionController->selectedTracks()));
    }

    void stateChanged(const PlayState state)
    {
        switch(state) {
            case(PlayState::Stopped):
                clearMessage();
                statusText->clear();
                break;
            case(PlayState::Playing):
                updatePlayingText();
                break;
            case(PlayState::Paused):
                break;
        }
    }
};

StatusWidget::StatusWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerController, selectionController, settings)}
{
    setObjectName(StatusWidget::name());

    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(playerController, &PlayerController::positionChanged, this,
                     [this](uint64_t /*pos*/) { p->updatePlayingText(); });
    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { p->updateSelectionText(); });

    settings->subscribe<Settings::Gui::IconTheme>(
        this, [this]() { p->iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize)); });

    settings->subscribe<Settings::Gui::Internal::StatusShowIcon>(this,
                                                                 [this](bool show) { p->iconLabel->setHidden(!show); });
    settings->subscribe<Settings::Gui::Internal::StatusShowSelection>(
        this, [this](bool show) { p->selectionText->setHidden(!show); });
    settings->subscribe<Settings::Gui::Internal::StatusPlayingScript>(this, [this](const QString& script) {
        p->playingScript = script;
        p->updatePlayingText();
    });
    settings->subscribe<Settings::Gui::Internal::StatusSelectionScript>(this, [this](const QString& script) {
        p->selectionScript = script;
        p->updateSelectionText();
    });
}

StatusWidget::~StatusWidget() = default;

QString StatusWidget::name() const
{
    return tr("Status Bar");
}

QString StatusWidget::layoutName() const
{
    return QStringLiteral("StatusBar");
}

void StatusWidget::libraryScanProgress(int /*id*/, int progress)
{
    p->updateScanText(progress);
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

    auto* showSelection = new QAction(tr("Show Track Selection"), this);
    showSelection->setCheckable(true);
    showSelection->setChecked(p->settings->value<Settings::Gui::Internal::StatusShowSelection>());
    QObject::connect(showSelection, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::StatusShowSelection>(checked); });
    menu->addAction(showSelection);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_statuswidget.cpp"
