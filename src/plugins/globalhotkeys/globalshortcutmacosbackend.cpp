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

#include "globalshortcutmacosbackend.h"

#include <climits>

constexpr OSType HotKeySignature = (static_cast<OSType>('F') << 24) | (static_cast<OSType>('y') << 16)
                                 | (static_cast<OSType>('H') << 8) | static_cast<OSType>('K');

namespace Fooyin::GlobalHotkeys {
namespace {
std::uint32_t nativeKey(Qt::Key key)
{
    switch(key) {
        case Qt::Key_A:
            return kVK_ANSI_A;
        case Qt::Key_B:
            return kVK_ANSI_B;
        case Qt::Key_C:
            return kVK_ANSI_C;
        case Qt::Key_D:
            return kVK_ANSI_D;
        case Qt::Key_E:
            return kVK_ANSI_E;
        case Qt::Key_F:
            return kVK_ANSI_F;
        case Qt::Key_G:
            return kVK_ANSI_G;
        case Qt::Key_H:
            return kVK_ANSI_H;
        case Qt::Key_I:
            return kVK_ANSI_I;
        case Qt::Key_J:
            return kVK_ANSI_J;
        case Qt::Key_K:
            return kVK_ANSI_K;
        case Qt::Key_L:
            return kVK_ANSI_L;
        case Qt::Key_M:
            return kVK_ANSI_M;
        case Qt::Key_N:
            return kVK_ANSI_N;
        case Qt::Key_O:
            return kVK_ANSI_O;
        case Qt::Key_P:
            return kVK_ANSI_P;
        case Qt::Key_Q:
            return kVK_ANSI_Q;
        case Qt::Key_R:
            return kVK_ANSI_R;
        case Qt::Key_S:
            return kVK_ANSI_S;
        case Qt::Key_T:
            return kVK_ANSI_T;
        case Qt::Key_U:
            return kVK_ANSI_U;
        case Qt::Key_V:
            return kVK_ANSI_V;
        case Qt::Key_W:
            return kVK_ANSI_W;
        case Qt::Key_X:
            return kVK_ANSI_X;
        case Qt::Key_Y:
            return kVK_ANSI_Y;
        case Qt::Key_Z:
            return kVK_ANSI_Z;
        case Qt::Key_0:
            return kVK_ANSI_0;
        case Qt::Key_1:
            return kVK_ANSI_1;
        case Qt::Key_2:
            return kVK_ANSI_2;
        case Qt::Key_3:
            return kVK_ANSI_3;
        case Qt::Key_4:
            return kVK_ANSI_4;
        case Qt::Key_5:
            return kVK_ANSI_5;
        case Qt::Key_6:
            return kVK_ANSI_6;
        case Qt::Key_7:
            return kVK_ANSI_7;
        case Qt::Key_8:
            return kVK_ANSI_8;
        case Qt::Key_9:
            return kVK_ANSI_9;
        case Qt::Key_Equal:
        case Qt::Key_Plus:
            return kVK_ANSI_Equal;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            return kVK_ANSI_Minus;
        case Qt::Key_BracketLeft:
        case Qt::Key_BraceLeft:
            return kVK_ANSI_LeftBracket;
        case Qt::Key_BracketRight:
        case Qt::Key_BraceRight:
            return kVK_ANSI_RightBracket;
        case Qt::Key_Apostrophe:
        case Qt::Key_QuoteDbl:
            return kVK_ANSI_Quote;
        case Qt::Key_Semicolon:
        case Qt::Key_Colon:
            return kVK_ANSI_Semicolon;
        case Qt::Key_Backslash:
        case Qt::Key_Bar:
            return kVK_ANSI_Backslash;
        case Qt::Key_Comma:
        case Qt::Key_Less:
            return kVK_ANSI_Comma;
        case Qt::Key_Slash:
        case Qt::Key_Question:
            return kVK_ANSI_Slash;
        case Qt::Key_Period:
        case Qt::Key_Greater:
            return kVK_ANSI_Period;
        case Qt::Key_QuoteLeft:
        case Qt::Key_AsciiTilde:
            return kVK_ANSI_Grave;
        case Qt::Key_Return:
            return kVK_Return;
        case Qt::Key_Enter:
            return kVK_ANSI_KeypadEnter;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            return kVK_Tab;
        case Qt::Key_Space:
            return kVK_Space;
        case Qt::Key_Backspace:
            return kVK_Delete;
        case Qt::Key_Escape:
            return kVK_Escape;
        case Qt::Key_Meta:
            return kVK_Command;
        case Qt::Key_Shift:
            return kVK_Shift;
        case Qt::Key_Alt:
            return kVK_Option;
        case Qt::Key_Control:
            return kVK_Control;
        case Qt::Key_F1:
            return kVK_F1;
        case Qt::Key_F2:
            return kVK_F2;
        case Qt::Key_F3:
            return kVK_F3;
        case Qt::Key_F4:
            return kVK_F4;
        case Qt::Key_F5:
            return kVK_F5;
        case Qt::Key_F6:
            return kVK_F6;
        case Qt::Key_F7:
            return kVK_F7;
        case Qt::Key_F8:
            return kVK_F8;
        case Qt::Key_F9:
            return kVK_F9;
        case Qt::Key_F10:
            return kVK_F10;
        case Qt::Key_F11:
            return kVK_F11;
        case Qt::Key_F12:
            return kVK_F12;
        case Qt::Key_F13:
            return kVK_F13;
        case Qt::Key_F14:
            return kVK_F14;
        case Qt::Key_F15:
            return kVK_F15;
        case Qt::Key_F16:
            return kVK_F16;
        case Qt::Key_F17:
            return kVK_F17;
        case Qt::Key_F18:
            return kVK_F18;
        case Qt::Key_F19:
            return kVK_F19;
        case Qt::Key_F20:
            return kVK_F20;
        case Qt::Key_Home:
            return kVK_Home;
        case Qt::Key_End:
            return kVK_End;
        case Qt::Key_PageUp:
            return kVK_PageUp;
        case Qt::Key_PageDown:
            return kVK_PageDown;
        case Qt::Key_Left:
            return kVK_LeftArrow;
        case Qt::Key_Right:
            return kVK_RightArrow;
        case Qt::Key_Up:
            return kVK_UpArrow;
        case Qt::Key_Down:
            return kVK_DownArrow;
        case Qt::Key_Delete:
            return kVK_ForwardDelete;
        default:
            return UINT_MAX;
    }
}

std::uint32_t nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    std::uint32_t native{0};
    if(modifiers.testFlag(Qt::ShiftModifier)) {
        native |= shiftKey;
    }
    if(modifiers.testFlag(Qt::ControlModifier)) {
        native |= controlKey;
    }
    if(modifiers.testFlag(Qt::AltModifier)) {
        native |= optionKey;
    }
    if(modifiers.testFlag(Qt::MetaModifier)) {
        native |= cmdKey;
    }
    return native;
}
} // namespace

GlobalShortcutMacosBackend::GlobalShortcutMacosBackend(QObject* parent)
    : GlobalShortcutBackend{parent}
    , m_eventHandler{nullptr}
    , m_nextNativeId{1}
{
    const EventTypeSpec eventTypes[]{
        {kEventClassKeyboard, kEventHotKeyPressed},
        {kEventClassKeyboard, kEventHotKeyReleased},
    };
    InstallApplicationEventHandler(&GlobalShortcutMacosBackend::handleEvent, 2, eventTypes, this, &m_eventHandler);
}

GlobalShortcutMacosBackend::~GlobalShortcutMacosBackend()
{
    clearBindings();

    if(m_eventHandler) {
        RemoveEventHandler(m_eventHandler);
    }
}

GlobalShortcutAvailability GlobalShortcutMacosBackend::availability() const
{
    return m_eventHandler ? GlobalShortcutAvailability::Available : GlobalShortcutAvailability::Unavailable;
}

void GlobalShortcutMacosBackend::applyBindings(const GlobalShortcutDescriptorList& bindings)
{
    clearBindings();

    if(!m_eventHandler) {
        return;
    }

    for(const auto& binding : bindings) {
        if(binding.shortcut.count() != 1) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("macOS global shortcuts must contain one key combination"));
            continue;
        }

        const QKeyCombination combination = binding.shortcut[0];
        const std::uint32_t key           = nativeKey(combination.key());
        if(key == UINT_MAX) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("The shortcut cannot be represented on macOS"));
            continue;
        }

        const std::uint32_t nativeId = m_nextNativeId++;
        const EventHotKeyID hotKeyId{HotKeySignature, nativeId};
        EventHotKeyRef hotKey{nullptr};

        const OSStatus status = RegisterEventHotKey(key, nativeModifiers(combination.keyboardModifiers()), hotKeyId,
                                                    GetApplicationEventTarget(), 0, &hotKey);
        if(status != noErr) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("macOS rejected this shortcut (error %1)").arg(status));
            continue;
        }

        m_hotKeys.emplace(nativeId, hotKey);
        m_bindings.emplace(nativeId, binding);
    }
}

void GlobalShortcutMacosBackend::clearBindings()
{
    for(const auto& hotKey : m_hotKeys) {
        UnregisterEventHotKey(hotKey.second);
    }
    m_hotKeys.clear();
    m_bindings.clear();
    m_pressedHotKeys.clear();
}

OSStatus GlobalShortcutMacosBackend::handleEvent(EventHandlerCallRef nextHandler, EventRef event, void* context)
{
    auto* self = static_cast<GlobalShortcutMacosBackend*>(context);

    EventHotKeyID hotKeyId{};
    const OSStatus status = GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                                              sizeof(hotKeyId), nullptr, &hotKeyId);
    if(status == noErr && hotKeyId.signature == HotKeySignature && self->m_bindings.contains(hotKeyId.id)) {
        if(GetEventKind(event) == kEventHotKeyReleased) {
            self->m_pressedHotKeys.erase(hotKeyId.id);
        }
        else if(!self->m_pressedHotKeys.contains(hotKeyId.id)) {
            self->m_pressedHotKeys.insert(hotKeyId.id);
            Q_EMIT self->activated(self->m_bindings.at(hotKeyId.id).commandId);
        }
        return noErr;
    }

    return CallNextEventHandler(nextHandler, event);
}
} // namespace Fooyin::GlobalHotkeys
