/*
 * Fooyin
 * Copyright © 2025, Carter Li <zhangsongcui@live.cn>
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "thumbnailtoolbarplugin.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QEvent>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QPixmap>
#include <QStyle>

#include <windows.h>
#include <wrl/client.h>

Q_LOGGING_CATEGORY(THUMBNAIL_TOOLBAR, "fy.thumbnailtoolbar")

namespace Fooyin::ThumbnailToolbar {

constexpr int PREVIOUS_BUTTON_ID  = 0;
constexpr int PLAYPAUSE_BUTTON_ID = 1;
constexpr int NEXT_BUTTON_ID      = 2;

HICON QIcon2HICON(const QIcon& icon)
{
    if(icon.isNull()) {
        return nullptr;
    }

    return icon.pixmap(16, 16).toImage().toHICON();
}

ThumbnailToolbarPlugin::ThumbnailToolbarPlugin() = default;

ThumbnailToolbarPlugin::~ThumbnailToolbarPlugin() = default;

void ThumbnailToolbarPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;

    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this,
                     &ThumbnailToolbarPlugin::trackChanged);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &ThumbnailToolbarPlugin::playStateChanged);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this,
                     &ThumbnailToolbarPlugin::updatePosition);
}

void ThumbnailToolbarPlugin::initialise(const GuiPluginContext& context)
{
    m_windowController = context.windowController;
    QObject::connect(m_windowController, &WindowController::windowShown, this, &ThumbnailToolbarPlugin::setupToolbar);
    QObject::connect(m_windowController, &WindowController::windowHidden, this, &ThumbnailToolbarPlugin::cleanupToobar);

    // Install native event filter for WM_COMMAND messages
    m_nativeEventFilter = std::make_unique<ThumbnailToolbarNativeEventFilter>(this);
    QGuiApplication::instance()->installNativeEventFilter(m_nativeEventFilter.get());

    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_taskbarList));
    if(FAILED(hr)) {
        qCWarning(THUMBNAIL_TOOLBAR) << "Failed to create ITaskbarList3 instance.";
        m_taskbarList.Reset();
    }
}

void ThumbnailToolbarPlugin::shutdown()
{
    if(m_nativeEventFilter) {
        QGuiApplication::instance()->removeNativeEventFilter(m_nativeEventFilter.get());
        m_nativeEventFilter.reset();
    }
}

void ThumbnailToolbarPlugin::handleThumbnailClick(int buttonId)
{
    switch(buttonId) {
        case PREVIOUS_BUTTON_ID:
            m_playerController->previous();
            break;
        case PLAYPAUSE_BUTTON_ID:
            m_playerController->playPause();
            break;
        case NEXT_BUTTON_ID:
            m_playerController->next();
            break;
    }
}

void ThumbnailToolbarPlugin::trackChanged(const PlaylistTrack& /*playlistTrack*/)
{
    updateToolbarButtons();
}

void ThumbnailToolbarPlugin::playStateChanged()
{
    if(!m_taskbarList) {
        return;
    }

    TBPFLAG state{TBPF_NORMAL};

    switch(m_playerController->playState()) {
        case(Player::PlayState::Playing):
            state = TBPF_NORMAL;
            break;
        case(Player::PlayState::Paused):
            state = TBPF_PAUSED;
            break;
        case(Player::PlayState::Stopped):
            state = TBPF_ERROR;
            break;
        default:
            state = TBPF_INDETERMINATE;
            break;
    }

    m_taskbarList->SetProgressState(reinterpret_cast<HWND>(m_windowController->mainWindow()->winId()), state);

    updateToolbarButtons();
}

void ThumbnailToolbarPlugin::updateToolbarButtons()
{
    if(!m_taskbarList || !m_toolbarReady) {
        return;
    }

    HWND hWnd = reinterpret_cast<HWND>(m_windowController->mainWindow()->winId());
    if(!hWnd) {
        return;
    }

    THUMBBUTTON buttons[3] = {};
    for(int i = 0; i < 3; ++i) {
        buttons[i].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[i].iId    = i;
    }

    // Previous button
    buttons[PREVIOUS_BUTTON_ID].hIcon = QIcon2HICON(Utils::iconFromTheme(Constants::Icons::Prev));
    wcscpy_s(buttons[PREVIOUS_BUTTON_ID].szTip,
             reinterpret_cast<LPCWSTR>(tr("Previous").utf16())); // On Windows, wchar_t is UTF-16
    buttons[PREVIOUS_BUTTON_ID].dwFlags = m_playlistHandler->previousTrack().isValid() ? THBF_ENABLED : THBF_DISABLED;

    // Play/Pause button
    bool isPlaying = m_playerController->playState() == Player::PlayState::Playing;
    buttons[PLAYPAUSE_BUTTON_ID].hIcon
        = QIcon2HICON(Utils::iconFromTheme(isPlaying ? Constants::Icons::Pause : Constants::Icons::Play));
    wcscpy_s(buttons[PLAYPAUSE_BUTTON_ID].szTip, reinterpret_cast<LPCWSTR>(tr(isPlaying ? "Pause" : "Play").utf16()));
    buttons[PLAYPAUSE_BUTTON_ID].dwFlags
        = m_playerController->currentPlaylistTrack().isValid() ? THBF_ENABLED : THBF_DISABLED;

    // Next button
    buttons[NEXT_BUTTON_ID].hIcon = QIcon2HICON(Utils::iconFromTheme(Constants::Icons::Next));
    wcscpy_s(buttons[NEXT_BUTTON_ID].szTip, reinterpret_cast<LPCWSTR>(tr("Next").utf16()));
    buttons[NEXT_BUTTON_ID].dwFlags = m_playlistHandler->nextTrack().isValid() ? THBF_ENABLED : THBF_DISABLED;

    m_taskbarList->ThumbBarUpdateButtons(hWnd, 3, buttons);

    for(int i = 0; i < 3; ++i) {
        if(buttons[i].hIcon) {
            DestroyIcon(buttons[i].hIcon);
        }
    }
}

void ThumbnailToolbarPlugin::setupToolbar()
{
    if(!m_taskbarList || m_toolbarReady) {
        return;
    }

    HWND hWnd = reinterpret_cast<HWND>(m_windowController->mainWindow()->winId());
    if(!hWnd) {
        return;
    }

    THUMBBUTTON buttons[3] = {};
    for(int i = 0; i < 3; ++i) {
        buttons[i].dwMask  = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[i].iId     = i;
        buttons[i].dwFlags = THBF_DISABLED;
    }

    buttons[PREVIOUS_BUTTON_ID].hIcon = QIcon2HICON(Utils::iconFromTheme(Constants::Icons::Prev));
    wcscpy_s(buttons[PREVIOUS_BUTTON_ID].szTip,
             reinterpret_cast<LPCWSTR>(tr("Previous").utf16())); // On Windows, wchar_t is UTF-16

    buttons[PLAYPAUSE_BUTTON_ID].hIcon = QIcon2HICON(Utils::iconFromTheme(Constants::Icons::Play));
    wcscpy_s(buttons[PLAYPAUSE_BUTTON_ID].szTip, reinterpret_cast<LPCWSTR>(tr("Play").utf16()));

    buttons[NEXT_BUTTON_ID].hIcon = QIcon2HICON(Utils::iconFromTheme(Constants::Icons::Next));
    wcscpy_s(buttons[NEXT_BUTTON_ID].szTip, reinterpret_cast<LPCWSTR>(tr("Next").utf16()));

    HRESULT hr = m_taskbarList->ThumbBarAddButtons(hWnd, 3, buttons);

    for(int i = 0; i < 3; ++i) {
        if(buttons[i].hIcon) {
            DestroyIcon(buttons[i].hIcon);
        }
    }

    if(SUCCEEDED(hr)) {
        m_toolbarReady = true;
        updateToolbarButtons();
    }
    else {
        qCWarning(THUMBNAIL_TOOLBAR) << "ThumbBarAddButtons failed.";
    }
}

void ThumbnailToolbarPlugin::cleanupToobar()
{
    if(m_taskbarList && m_toolbarReady) {
        THUMBBUTTON buttons[3] = {};
        m_taskbarList->ThumbBarUpdateButtons(reinterpret_cast<HWND>(m_windowController->mainWindow()->winId()), 0,
                                             buttons);
    }
    m_taskbarList.Reset();
    m_toolbarReady = false;
}

void ThumbnailToolbarPlugin::updatePosition()
{
    if(m_taskbarList && m_toolbarReady) {
        m_taskbarList->SetProgressValue(reinterpret_cast<HWND>(m_windowController->mainWindow()->winId()),
                                        m_playerController->currentPosition(),
                                        m_playerController->currentTrack().duration());
    }
}

} // namespace Fooyin::ThumbnailToolbar

#include "moc_thumbnailtoolbarplugin.cpp"
