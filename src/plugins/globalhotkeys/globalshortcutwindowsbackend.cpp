/*
 * Fooyin
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

#include "globalshortcutwindowsbackend.h"

#include <QCoreApplication>

#include <windows.h>

namespace Fooyin::GlobalHotkeys {
namespace {
struct NativeKey
{
    UINT key{0};
    UINT modifiers{0};
};

NativeKey nativeKey(Qt::Key key)
{
    if(key >= Qt::Key_0 && key <= Qt::Key_9) {
        return {.key = static_cast<UINT>('0' + (key - Qt::Key_0))};
    }
    if(key >= Qt::Key_A && key <= Qt::Key_Z) {
        return {.key = static_cast<UINT>('A' + (key - Qt::Key_A))};
    }
    if(key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return {.key = VK_F1 + static_cast<UINT>(key - Qt::Key_F1)};
    }
    if(key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
        const SHORT keyAndModifiers = VkKeyScanW(static_cast<WCHAR>(key));
        if(keyAndModifiers == -1) {
            return {};
        }

        UINT modifiers{0};
        const BYTE shiftState = HIBYTE(keyAndModifiers);
        if((shiftState & 1) != 0) {
            modifiers |= MOD_SHIFT;
        }
        if((shiftState & 2) != 0) {
            modifiers |= MOD_CONTROL;
        }
        if((shiftState & 4) != 0) {
            modifiers |= MOD_ALT;
        }
        return {.key = LOBYTE(keyAndModifiers), .modifiers = modifiers};
    }

    switch(key) {
        case Qt::Key_Backspace:
            return {.key = VK_BACK};
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            return {.key = VK_TAB};
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return {.key = VK_RETURN};
        case Qt::Key_Pause:
            return {.key = VK_PAUSE};
        case Qt::Key_Escape:
            return {.key = VK_ESCAPE};
        case Qt::Key_PageUp:
            return {.key = VK_PRIOR};
        case Qt::Key_PageDown:
            return {.key = VK_NEXT};
        case Qt::Key_End:
            return {.key = VK_END};
        case Qt::Key_Home:
            return {.key = VK_HOME};
        case Qt::Key_Left:
            return {.key = VK_LEFT};
        case Qt::Key_Up:
            return {.key = VK_UP};
        case Qt::Key_Right:
            return {.key = VK_RIGHT};
        case Qt::Key_Down:
            return {.key = VK_DOWN};
        case Qt::Key_Print:
            return {.key = VK_SNAPSHOT};
        case Qt::Key_Insert:
            return {.key = VK_INSERT};
        case Qt::Key_Delete:
            return {.key = VK_DELETE};
        case Qt::Key_VolumeMute:
            return {.key = VK_VOLUME_MUTE};
        case Qt::Key_VolumeDown:
            return {.key = VK_VOLUME_DOWN};
        case Qt::Key_VolumeUp:
            return {.key = VK_VOLUME_UP};
        case Qt::Key_MediaNext:
            return {.key = VK_MEDIA_NEXT_TRACK};
        case Qt::Key_MediaPrevious:
            return {.key = VK_MEDIA_PREV_TRACK};
        case Qt::Key_MediaStop:
            return {.key = VK_MEDIA_STOP};
        case Qt::Key_MediaPlay:
        case Qt::Key_MediaPause:
        case Qt::Key_MediaTogglePlayPause:
            return {.key = VK_MEDIA_PLAY_PAUSE};
        default:
            return {};
    }
}

UINT nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    UINT native{MOD_NOREPEAT};
    if(modifiers.testFlag(Qt::AltModifier)) {
        native |= MOD_ALT;
    }
    if(modifiers.testFlag(Qt::ControlModifier)) {
        native |= MOD_CONTROL;
    }
    if(modifiers.testFlag(Qt::ShiftModifier)) {
        native |= MOD_SHIFT;
    }
    if(modifiers.testFlag(Qt::MetaModifier)) {
        native |= MOD_WIN;
    }
    return native;
}
} // namespace

GlobalShortcutWindowsBackend::GlobalShortcutWindowsBackend(QObject* parent)
    : GlobalShortcutBackend{parent}
    , m_nextNativeId{1}
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalShortcutWindowsBackend::~GlobalShortcutWindowsBackend()
{
    clearBindings();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

GlobalShortcutAvailability GlobalShortcutWindowsBackend::availability() const
{
    return GlobalShortcutAvailability::Available;
}

void GlobalShortcutWindowsBackend::applyBindings(const GlobalShortcutDescriptorList& bindings)
{
    clearBindings();

    for(const auto& binding : bindings) {
        if(binding.shortcut.count() != 1) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("Windows global shortcuts must contain one key combination"));
            continue;
        }

        const QKeyCombination combination = binding.shortcut[0];
        const NativeKey key               = nativeKey(combination.key());
        if(key.key == 0) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("The shortcut cannot be represented on Windows"));
            continue;
        }

        if(m_nextNativeId > 0xBFFF) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("Windows cannot register any more global shortcuts"));
            continue;
        }

        const int nativeId   = m_nextNativeId++;
        const UINT modifiers = nativeModifiers(combination.keyboardModifiers()) | key.modifiers;
        if(!RegisterHotKey(nullptr, nativeId, modifiers, key.key)) {
            Q_EMIT registrationFailed(
                binding.commandId, binding.shortcut,
                tr("Windows rejected this shortcut (error %1)").arg(static_cast<qulonglong>(GetLastError())));
            continue;
        }

        m_bindings.emplace(nativeId, binding);
    }
}

void GlobalShortcutWindowsBackend::clearBindings()
{
    for(const auto& binding : m_bindings) {
        UnregisterHotKey(nullptr, binding.first);
    }
    m_bindings.clear();
}

bool GlobalShortcutWindowsBackend::nativeEventFilter(const QByteArray& /*eventType*/, void* message,
                                                     qintptr* /*result*/)
{
    const auto* nativeMessage = static_cast<MSG*>(message);
    if(nativeMessage->message == WM_HOTKEY) {
        const int nativeId = static_cast<int>(nativeMessage->wParam);

        if(const auto binding = m_bindings.find(nativeId); binding != m_bindings.cend()) {
            Q_EMIT activated(binding->second.commandId);
        }
    }

    return false;
}
} // namespace Fooyin::GlobalHotkeys
