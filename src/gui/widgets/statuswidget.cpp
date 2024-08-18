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
class StatusWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    StatusWidgetPrivate(StatusWidget* self, PlayerController* playerController,
                        TrackSelectionController* selectionController, SettingsManager* settings);

    void setupConnections();

    void clearMessage();
    void showMessage(const QString& message, int timeout = 0);
    void showStatusMessage(const QString& message) const;

    void updateScripts();
    void updatePlayingText();
    void updateSelectionText();

    void stateChanged(Player::PlayState state);

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
};

StatusWidgetPrivate::StatusWidgetPrivate(StatusWidget* self, PlayerController* playerController,
                                         TrackSelectionController* selectionController, SettingsManager* settings)
    : QObject{self}
    , m_self{self}
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

    m_statusText->hide();
    m_clearTimer.setSingleShot(true);

    m_iconLabel->setHidden(!m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    m_selectionText->setHidden(!m_settings->value<Settings::Gui::Internal::StatusShowSelection>());

    setupConnections();
    updateScripts();
    updatePlayingText();
}

void StatusWidgetPrivate::setupConnections()
{
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &StatusWidgetPrivate::stateChanged);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this,
                     &StatusWidgetPrivate::updatePlayingText);
    QObject::connect(m_selectionController, &TrackSelectionController::selectionChanged, this,
                     &StatusWidgetPrivate::updateSelectionText);
    QObject::connect(&m_clearTimer, &QTimer::timeout, this, &StatusWidgetPrivate::clearMessage);

    m_settings->subscribe<Settings::Gui::IconTheme>(
        this, [this]() { m_iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize)); });
    m_settings->subscribe<Settings::Gui::Internal::StatusShowIcon>(
        this, [this](bool show) { m_iconLabel->setHidden(!show); });
    m_settings->subscribe<Settings::Gui::Internal::StatusShowSelection>(
        this, [this](bool show) { m_selectionText->setHidden(!show); });
    m_settings->subscribe<Settings::Gui::Internal::StatusPlayingScript>(this, [this](const QString& script) {
        m_playingScript = script;
        updatePlayingText();
    });
    m_settings->subscribe<Settings::Gui::Internal::StatusSelectionScript>(this, [this](const QString& script) {
        m_selectionScript = script;
        updateSelectionText();
    });
}

void StatusWidgetPrivate::clearMessage()
{
    m_clearTimer.stop();

    m_messageText->clear();
}

void StatusWidgetPrivate::showMessage(const QString& message, int timeout)
{
    if(m_clearTimer.isActive() && timeout == 0) {
        return;
    }

    if(m_statusText->isVisible()) {
        return;
    }

    m_messageText->setText(message);
    m_statusText->hide();
    m_messageText->show();

    if(timeout > 0) {
        m_clearTimer.start(timeout);
    }
}

void StatusWidgetPrivate::showStatusMessage(const QString& message) const
{
    m_statusText->setText(message);

    if(message.isEmpty()) {
        m_messageText->show();
        m_statusText->hide();
    }
    else {
        m_statusText->show();
        m_messageText->hide();
    }
}

void StatusWidgetPrivate::updateScripts()
{
    m_playingScript   = m_settings->value<Settings::Gui::Internal::StatusPlayingScript>();
    m_selectionScript = m_settings->value<Settings::Gui::Internal::StatusSelectionScript>();
}

void StatusWidgetPrivate::updatePlayingText()
{
    const auto ps = m_playerController->playState();
    if(ps == Player::PlayState::Playing || ps == Player::PlayState::Paused) {
        showMessage(m_scriptParser.evaluate(m_playingScript, m_playerController->currentTrack()));
    }
}

void StatusWidgetPrivate::updateSelectionText()
{
    m_selectionText->setText(m_scriptParser.evaluate(m_selectionScript, m_selectionController->selectedTracks()));
}

void StatusWidgetPrivate::stateChanged(const Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Stopped):
            clearMessage();
            m_statusText->clear();
            break;
        case(Player::PlayState::Playing):
            updatePlayingText();
            break;
        case(Player::PlayState::Paused):
            break;
    }
}

StatusWidget::StatusWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<StatusWidgetPrivate>(this, playerController, selectionController, settings)}
{
    setObjectName(StatusWidget::name());
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

void StatusWidget::showMessage(const QString& message)
{
    p->showMessage(message, 0);
}

void StatusWidget::showTempMessage(const QString& message)
{
    p->showMessage(message, 3000);
}

void StatusWidget::showTempMessage(const QString& message, int timeout)
{
    p->showMessage(message, timeout);
}

void StatusWidget::showStatusTip(const QString& message)
{
    p->showStatusMessage(message);
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
#include "statuswidget.moc"
