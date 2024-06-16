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
    StatusWidget* m_self;

    PlayerController* m_playerController;
    TrackSelectionController* m_selectionController;

    SettingsManager* m_settings;

    ScriptRegistry m_scriptRegistry;
    ScriptParser m_scriptParser;

    ClickableLabel* m_iconLabel;
    ElidedLabel* m_statusText;
    ElidedLabel* m_messageText;
    ClickableLabel* m_selectionText;

    QString m_playingScript;
    QString m_selectionScript;

    QTimer m_clearTimer;
    QString m_tempText;

    Private(StatusWidget* self, PlayerController* playerController, TrackSelectionController* selectionController,
            SettingsManager* settings)
        : m_self{self}
        , m_playerController{playerController}
        , m_selectionController{selectionController}
        , m_settings{settings}
        , m_scriptRegistry{m_playerController}
        , m_scriptParser{&m_scriptRegistry}
        , m_iconLabel{new ClickableLabel(m_self)}
        , m_statusText{new ElidedLabel(m_self)}
        , m_messageText{new ElidedLabel(m_self)}
        , m_selectionText{new ClickableLabel(m_self)}
    {
        auto* layout = new QHBoxLayout(m_self);
        layout->setContentsMargins(5, 0, 5, 0);

        m_iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize));
        m_iconLabel->setScaledContents(true);

        m_iconLabel->setMaximumHeight(22);
        m_iconLabel->setMaximumWidth(22);

        layout->addWidget(m_iconLabel);
        layout->addWidget(m_messageText, 1);
        layout->addWidget(m_statusText, 1);
        layout->addWidget(m_selectionText);

        m_messageText->hide();

        m_iconLabel->setHidden(!m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
        m_selectionText->setHidden(!m_settings->value<Settings::Gui::Internal::StatusShowSelection>());

        updateScripts();
        updatePlayingText();

        QObject::connect(m_playerController, &PlayerController::playStateChanged, m_self,
                         [this](PlayState state) { stateChanged(state); });
        QObject::connect(m_playerController, &PlayerController::positionChanged, m_self,
                         [this](uint64_t /*pos*/) { updatePlayingText(); });
        QObject::connect(m_selectionController, &TrackSelectionController::selectionChanged, m_self,
                         [this]() { updateSelectionText(); });

        m_clearTimer.setSingleShot(true);
        QObject::connect(&m_clearTimer, &QTimer::timeout, m_self, [this]() { clearMessage(); });
    }

    void clearMessage()
    {
        m_clearTimer.stop();

        m_messageText->clear();
        m_messageText->hide();
        m_statusText->show();
    }

    void showMessage(const QString& message, int timeout = 0)
    {
        m_messageText->setText(message);
        m_statusText->hide();
        m_messageText->show();

        if(timeout > 0) {
            m_clearTimer.start(timeout);
        }
    }

    void updateScripts()
    {
        m_playingScript   = m_settings->value<Settings::Gui::Internal::StatusPlayingScript>();
        m_selectionScript = m_settings->value<Settings::Gui::Internal::StatusSelectionScript>();
    }

    void updatePlayingText()
    {
        const PlayState ps = m_playerController->playState();
        if(ps == PlayState::Playing || ps == PlayState::Paused) {
            m_statusText->setText(m_scriptParser.evaluate(m_playingScript, m_playerController->currentTrack()));
        }
    }

    void updateScanText(int progress)
    {
        const auto scanText = QStringLiteral("Scanning library: %1%").arg(progress);
        showMessage(scanText, 5000);
    }

    void updateSelectionText()
    {
        m_selectionText->setText(m_scriptParser.evaluate(m_selectionScript, m_selectionController->selectedTracks()));
    }

    void stateChanged(const PlayState state)
    {
        switch(state) {
            case(PlayState::Stopped):
                clearMessage();
                m_statusText->clear();
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
        this, [this]() { p->m_iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize)); });

    settings->subscribe<Settings::Gui::Internal::StatusShowIcon>(
        this, [this](bool show) { p->m_iconLabel->setHidden(!show); });
    settings->subscribe<Settings::Gui::Internal::StatusShowSelection>(
        this, [this](bool show) { p->m_selectionText->setHidden(!show); });
    settings->subscribe<Settings::Gui::Internal::StatusPlayingScript>(this, [this](const QString& script) {
        p->m_playingScript = script;
        p->updatePlayingText();
    });
    settings->subscribe<Settings::Gui::Internal::StatusSelectionScript>(this, [this](const QString& script) {
        p->m_selectionScript = script;
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
    showIcon->setChecked(p->m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    QObject::connect(showIcon, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<Settings::Gui::Internal::StatusShowIcon>(checked); });
    menu->addAction(showIcon);

    auto* showSelection = new QAction(tr("Show Track Selection"), this);
    showSelection->setCheckable(true);
    showSelection->setChecked(p->m_settings->value<Settings::Gui::Internal::StatusShowSelection>());
    QObject::connect(showSelection, &QAction::triggered, this, [this](bool checked) {
        p->m_settings->set<Settings::Gui::Internal::StatusShowSelection>(checked);
    });
    menu->addAction(showSelection);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_statuswidget.cpp"
