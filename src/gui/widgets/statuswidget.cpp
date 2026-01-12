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
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/playlist/playlistcontroller.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/clickablelabel.h>
#include <gui/widgets/elidedlabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QBasicTimer>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>

using namespace Qt::StringLiterals;

constexpr int IconSize = 50;

namespace Fooyin {
class StatusLabel : public ElidedLabel
{
    Q_OBJECT

public:
    using ElidedLabel::ElidedLabel;
};

class StatusWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    StatusWidgetPrivate(StatusWidget* self, PlayerController* playerController, PlaylistHandler* playlistHandler,
                        PlaylistController* playlistController, TrackSelectionController* selectionController,
                        SettingsManager* settings);

    void setupConnections();

    void clearMessage();
    void updateMessageVisibility() const;

    void showMessage(const QString& message, int timeout = 0);
    void showStatusMessage(const QString& message) const;
    void showPlayingMessage(const QString& message) const;
    void showLayoutEditing() const;

    void updateScripts();
    void updatePlayingText();
    void updateSelectionText();

    void stateChanged(Player::PlayState state);

    StatusWidget* m_self;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    PlaylistController* m_playlistController;
    TrackSelectionController* m_selectionController;

    SettingsManager* m_settings;

    ScriptParser m_scriptParser;

    ClickableLabel* m_iconLabel;
    StatusLabel* m_playingText;
    StatusLabel* m_statusText;
    StatusLabel* m_messageText;
    StatusLabel* m_selectionText;

    QString m_playingScript;
    QString m_selectionScript;

    QBasicTimer m_clearTimer;
};

StatusWidgetPrivate::StatusWidgetPrivate(StatusWidget* self, PlayerController* playerController,
                                         PlaylistHandler* playlistHandler, PlaylistController* playlistController,
                                         TrackSelectionController* selectionController, SettingsManager* settings)
    : QObject{self}
    , m_self{self}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_playlistController{playlistController}
    , m_selectionController{selectionController}
    , m_settings{settings}
    , m_scriptParser{new ScriptRegistry(m_playerController)}
    , m_iconLabel{new ClickableLabel(m_self)}
    , m_playingText{new StatusLabel(m_self)}
    , m_statusText{new StatusLabel(m_self)}
    , m_messageText{new StatusLabel(m_self)}
    , m_selectionText{new StatusLabel(m_self)}
{
    auto* layout = new QHBoxLayout(m_self);
    layout->setContentsMargins(5, 0, 5, 0);

    m_iconLabel->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize));
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setMaximumSize(22, 22);

    layout->addWidget(m_iconLabel, 0, Qt::AlignLeft);
    layout->addWidget(m_messageText, 1);
    layout->addWidget(m_statusText, 1);
    layout->addWidget(m_playingText, 1);
    layout->addWidget(m_selectionText, 0, Qt::AlignRight);

    m_statusText->hide();
    m_playingText->hide();
    m_messageText->hide();

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
    m_settings->subscribe<Settings::Gui::LayoutEditing>(this, [this]() { showLayoutEditing(); });
}

void StatusWidgetPrivate::clearMessage()
{
    m_clearTimer.stop();
    showLayoutEditing();
    updateMessageVisibility();
}

void StatusWidgetPrivate::updateMessageVisibility() const
{
    // Show first non-empty message with the highest priority

    // Status tip
    if(!m_statusText->text().isEmpty()) {
        m_statusText->show();
        m_messageText->hide();
        m_playingText->hide();
    }
    // Message (temp or perm)
    else if(!m_messageText->text().isEmpty()) {
        m_messageText->show();
        m_statusText->hide();
        m_playingText->hide();
    }
    // Playing text
    else if(!m_playingText->text().isEmpty()) {
        m_playingText->show();
        m_messageText->hide();
        m_statusText->hide();
    }
    else {
        m_playingText->hide();
        m_messageText->hide();
        m_statusText->hide();
    }
}

void StatusWidgetPrivate::showMessage(const QString& message, int timeout)
{
    m_messageText->setText(message);
    updateMessageVisibility();

    if(timeout > 0) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        m_clearTimer.start(static_cast<std::chrono::milliseconds>(timeout), m_self);
#else
        m_clearTimer.start(timeout, m_self);
#endif
    }
    else {
        m_clearTimer.stop();
    }
}

void StatusWidgetPrivate::showStatusMessage(const QString& message) const
{
    m_statusText->setText(message);
    updateMessageVisibility();
}

void StatusWidgetPrivate::showPlayingMessage(const QString& message) const
{
    m_playingText->setText(message);
    updateMessageVisibility();
}

void StatusWidgetPrivate::showLayoutEditing() const
{
    m_messageText->setText(m_settings->value<Settings::Gui::LayoutEditing>() ? tr("Layout Editing Mode") : QString{});
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
        QString playingText;
        if(auto* playlist = m_playlistHandler->activePlaylist(); playlist && playlist->currentTrack().isValid()) {
            playingText = m_scriptParser.evaluate(m_playingScript, *playlist);
        }
        else {
            playingText = m_scriptParser.evaluate(m_playingScript, m_playerController->currentTrack());
        }
        showPlayingMessage(playingText);
    }
    else {
        m_playingText->clear();
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
            updatePlayingText();
            clearMessage();
            break;
        case(Player::PlayState::Playing):
            updatePlayingText();
            break;
        case(Player::PlayState::Paused):
            break;
    }
}

StatusWidget::StatusWidget(PlayerController* playerController, PlaylistHandler* playlistHandler,
                           PlaylistController* playlistController, TrackSelectionController* selectionController,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<StatusWidgetPrivate>(this, playerController, playlistHandler, playlistController,
                                              selectionController, settings)}
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
    return u"StatusBar"_s;
}

QString StatusWidget::defaultPlayingScript()
{
    return u"[%codec% | ][%bitrate% kbps | ][%samplerate% Hz | ][%channels% | ]%playback_time%[ / %duration%]"_s;
}

QString StatusWidget::defaultSelectionScript()
{
    return u"[%trackcount% $ifequal(%trackcount%,1,Track,Tracks) | %playtime%]"_s;
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

QSize StatusWidget::sizeHint() const
{
    QSize hint;

    hint = hint.expandedTo(p->m_playingText->sizeHint());
    hint = hint.expandedTo(p->m_statusText->sizeHint());
    hint = hint.expandedTo(p->m_messageText->sizeHint());
    hint = hint.expandedTo(p->m_selectionText->sizeHint());

    return hint;
}

QSize StatusWidget::minimumSizeHint() const
{
    QSize hint{0, 22};

    hint = hint.expandedTo(p->m_playingText->minimumSizeHint());
    hint = hint.expandedTo(p->m_statusText->minimumSizeHint());
    hint = hint.expandedTo(p->m_messageText->minimumSizeHint());
    hint = hint.expandedTo(p->m_selectionText->minimumSizeHint());

    return hint;
}

void StatusWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showIcon = new QAction(tr("Show icon"), menu);
    showIcon->setCheckable(true);
    showIcon->setChecked(p->m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    QObject::connect(showIcon, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<Settings::Gui::Internal::StatusShowIcon>(checked); });

    auto* showSelection = new QAction(tr("Show track selection"), menu);
    showSelection->setCheckable(true);
    showSelection->setChecked(p->m_settings->value<Settings::Gui::Internal::StatusShowSelection>());
    QObject::connect(showSelection, &QAction::triggered, this, [this](bool checked) {
        p->m_settings->set<Settings::Gui::Internal::StatusShowSelection>(checked);
    });

    auto* showTips = new QAction(tr("Show action tips"), menu);
    showTips->setCheckable(true);
    showTips->setChecked(p->m_settings->value<Settings::Gui::ShowStatusTips>());
    QObject::connect(showTips, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<Settings::Gui::ShowStatusTips>(checked); });

    menu->addAction(showIcon);
    menu->addAction(showSelection);
    menu->addAction(showTips);

    menu->popup(event->globalPos());
}

void StatusWidget::mouseDoubleClickEvent([[maybe_unused]] QMouseEvent* event)
{
    p->m_playlistController->showNowPlaying();
}

void StatusWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_clearTimer.timerId()) {
        p->clearMessage();
    }
    FyWidget::timerEvent(event);
}
} // namespace Fooyin

#include "moc_statuswidget.cpp"
#include "statuswidget.moc"
