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

#include <utils/scopeguard.h>

#include <QCoreApplication>
#include <QLoggingCategory>

#include <array>
#include <climits>
#include <set>
#include <vector>

Q_LOGGING_CATEGORY(MACOS_SHORTCUTS, "fy.globalshortcuts.macos")

constexpr OSType HotKeySignature = (static_cast<OSType>('F') << 24) | (static_cast<OSType>('y') << 16)
                                 | (static_cast<OSType>('H') << 8) | static_cast<OSType>('K');

namespace Fooyin::GlobalHotkeys {
namespace {
using NativeShortcutSet = std::set<std::pair<std::uint32_t, std::uint32_t>>;

std::uint32_t nativeKey(Qt::Key key)
{
    switch(key) {
        case Qt::Key_Return:
            return kVK_Return;
        case Qt::Key_Enter:
            return kVK_ANSI_KeypadEnter;
        case Qt::Key_Clear:
            return kVK_ANSI_KeypadClear;
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
            return QCoreApplication::testAttribute(Qt::AA_MacDontSwapCtrlAndMeta) ? kVK_Command : kVK_Control;
        case Qt::Key_Shift:
            return kVK_Shift;
        case Qt::Key_Alt:
            return kVK_Option;
        case Qt::Key_Control:
            return QCoreApplication::testAttribute(Qt::AA_MacDontSwapCtrlAndMeta) ? kVK_Control : kVK_Command;
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
    const bool dontSwap = QCoreApplication::testAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
    std::uint32_t native{0};
    if(modifiers.testFlag(Qt::ShiftModifier)) {
        native |= shiftKey;
    }
    if(modifiers.testFlag(Qt::ControlModifier)) {
        native |= dontSwap ? controlKey : cmdKey;
    }
    if(modifiers.testFlag(Qt::AltModifier)) {
        native |= optionKey;
    }
    if(modifiers.testFlag(Qt::MetaModifier)) {
        native |= dontSwap ? cmdKey : controlKey;
    }
    return native;
}

bool isKeypadKey(std::uint32_t key)
{
    switch(key) {
        case kVK_ANSI_Keypad0:
        case kVK_ANSI_Keypad1:
        case kVK_ANSI_Keypad2:
        case kVK_ANSI_Keypad3:
        case kVK_ANSI_Keypad4:
        case kVK_ANSI_Keypad5:
        case kVK_ANSI_Keypad6:
        case kVK_ANSI_Keypad7:
        case kVK_ANSI_Keypad8:
        case kVK_ANSI_Keypad9:
        case kVK_ANSI_KeypadDecimal:
        case kVK_ANSI_KeypadMultiply:
        case kVK_ANSI_KeypadPlus:
        case kVK_ANSI_KeypadDivide:
        case kVK_ANSI_KeypadMinus:
        case kVK_ANSI_KeypadEquals:
        case kVK_ANSI_KeypadClear:
        case kVK_ANSI_KeypadEnter:
        case kVK_JIS_KeypadComma:
        // Qt also marks navigation keys as keypad keys on macOS
        case kVK_Home:
        case kVK_End:
        case kVK_PageUp:
        case kVK_PageDown:
        case kVK_LeftArrow:
        case kVK_RightArrow:
        case kVK_UpArrow:
        case kVK_DownArrow:
        case kVK_ForwardDelete:
            return true;
        default:
            return false;
    }
}

NativeShortcutSet nativeShortcuts(QKeyCombination combination, const UCKeyboardLayout* layout)
{
    NativeShortcutSet shortcuts;

    const auto key        = combination.key();
    const auto modifiers  = nativeModifiers(combination.keyboardModifiers());
    const bool keypadOnly = combination.keyboardModifiers().testFlag(Qt::KeypadModifier);
    if(key <= Qt::Key_Space || key > 0xFFFF) {
        const auto code = nativeKey(key);
        if(code != UINT_MAX && (!keypadOnly || isKeypadKey(code))) {
            shortcuts.emplace(code, modifiers);
        }
        return shortcuts;
    }

    if(!layout) {
        return shortcuts;
    }

    const std::array<std::uint32_t, 4> characterModifiers{0, shiftKey, optionKey, shiftKey | optionKey};
    const auto keyboardType = LMGetKbdType();
    for(UInt16 code{0}; code < 128; ++code) {
        if(keypadOnly && !isKeypadKey(code)) {
            continue;
        }

        for(const auto characterModifier : characterModifiers) {
            std::array<UniChar, 4> characters{};
            UniCharCount length{0};
            UInt32 deadKeyState{0};
            const auto lookupModifiers = ((characterModifier | (modifiers & cmdKey)) >> 8) & 0xFF;
            const OSStatus status      = UCKeyTranslate(layout, code, kUCKeyActionDown, lookupModifiers, keyboardType,
                                                        kUCKeyTranslateNoDeadKeysMask, &deadKeyState, characters.size(),
                                                        &length, characters.data());
            if(status == noErr && length == 1 && QChar{characters.front()}.toUpper().unicode() == key) {
                shortcuts.emplace(code, modifiers | characterModifier);
                break;
            }
        }
    }

    return shortcuts;
}
} // namespace

GlobalShortcutMacosBackend::GlobalShortcutMacosBackend(QObject* parent)
    : GlobalShortcutBackend{parent}
    , m_eventHandler{nullptr}
    , m_nextNativeId{1}
{
    QObject::connect(&m_repeater, &GlobalShortcutRepeater::activated, this, &GlobalShortcutBackend::activated);

    const EventTypeSpec eventTypes[]{
        {kEventClassKeyboard, kEventHotKeyPressed},
        {kEventClassKeyboard, kEventHotKeyReleased},
    };
    InstallApplicationEventHandler(&GlobalShortcutMacosBackend::handleEvent, 2, eventTypes, this, &m_eventHandler);

    CFNotificationCenterAddObserver(
        CFNotificationCenterGetDistributedCenter(), this,
        [](CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef) {
            auto* self = static_cast<GlobalShortcutMacosBackend*>(observer);
            QMetaObject::invokeMethod(self, [self] { self->registerBindings(false); }, Qt::QueuedConnection);
        },
        kTISNotifySelectedKeyboardInputSourceChanged, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
}

GlobalShortcutMacosBackend::~GlobalShortcutMacosBackend()
{
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDistributedCenter(), this,
                                       kTISNotifySelectedKeyboardInputSourceChanged, nullptr);
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
    m_requestedBindings = bindings;
    registerBindings(true);
}

void GlobalShortcutMacosBackend::clearBindings()
{
    m_requestedBindings.clear();
    clearRegistrations();
}

OSStatus GlobalShortcutMacosBackend::handleEvent(EventHandlerCallRef nextHandler, EventRef event, void* context)
{
    auto* self = static_cast<GlobalShortcutMacosBackend*>(context);

    EventHotKeyID hotKeyId{};
    const OSStatus status = GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                                              sizeof(hotKeyId), nullptr, &hotKeyId);
    if(status == noErr && hotKeyId.signature == HotKeySignature && self->m_bindings.contains(hotKeyId.id)) {
        const Id commandId       = self->m_bindings.at(hotKeyId.id).commandId;
        const QString shortcutId = QString::number(hotKeyId.id);
        if(GetEventKind(event) == kEventHotKeyReleased) {
            self->m_repeater.release(shortcutId);
        }
        else {
            self->m_repeater.press(shortcutId, commandId);
        }
        return noErr;
    }

    return CallNextEventHandler(nextHandler, event);
}

void GlobalShortcutMacosBackend::registerBindings(bool reportFailures)
{
    clearRegistrations();

    if(!m_eventHandler || m_requestedBindings.empty()) {
        return;
    }

    TISInputSourceRef source = TISCopyInputMethodKeyboardLayoutOverride();
    if(!source) {
        source = TISCopyCurrentKeyboardLayoutInputSource();
    }

    const auto releaseSource = qScopeGuard([&source] {
        if(source) {
            CFRelease(source);
        }
    });
    auto layoutData = source
                        ? static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData))
                        : nullptr;
    if(!layoutData) {
        if(source) {
            CFRelease(source);
        }
        source     = TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
        layoutData = source
                       ? static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData))
                       : nullptr;
    }
    const auto* layout = layoutData ? reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layoutData)) : nullptr;

    const auto fail = [this, reportFailures](const GlobalShortcutDescriptor& binding, const QString& error) {
        if(reportFailures) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut, error);
        }
        else {
            qCWarning(MACOS_SHORTCUTS) << "Failed to refresh" << binding.shortcut.toString() << ':' << error;
        }
    };

    NativeShortcutSet registeredShortcuts;
    for(const auto& binding : m_requestedBindings) {
        if(binding.shortcut.count() != 1) {
            fail(binding, tr("macOS global shortcuts must contain one key combination"));
            continue;
        }

        const auto shortcuts = nativeShortcuts(binding.shortcut[0], layout);
        if(shortcuts.empty()) {
            fail(binding, tr("The shortcut cannot be represented on macOS"));
            continue;
        }

        if(std::ranges::any_of(shortcuts, [&registeredShortcuts](const auto& shortcut) {
               return registeredShortcuts.contains(shortcut);
           })) {
            fail(binding, tr("The shortcut is already registered"));
            continue;
        }

        std::vector<std::pair<std::uint32_t, EventHotKeyRef>> hotKeys;
        hotKeys.reserve(shortcuts.size());

        OSStatus status{noErr};
        for(const auto& [key, modifiers] : shortcuts) {
            const std::uint32_t nativeId = m_nextNativeId++;
            const EventHotKeyID hotKeyId{HotKeySignature, nativeId};
            EventHotKeyRef hotKey{nullptr};

            status = RegisterEventHotKey(key, modifiers, hotKeyId, GetApplicationEventTarget(), 0, &hotKey);
            if(status != noErr) {
                break;
            }
            hotKeys.emplace_back(nativeId, hotKey);
        }
        if(status != noErr) {
            for(const auto& [nativeId, hotKey] : hotKeys) {
                UnregisterEventHotKey(hotKey);
            }
            fail(binding, tr("macOS rejected this shortcut (error %1)").arg(status));
            continue;
        }

        registeredShortcuts.insert(shortcuts.begin(), shortcuts.end());
        for(const auto& [nativeId, hotKey] : hotKeys) {
            m_hotKeys.emplace(nativeId, hotKey);
            m_bindings.emplace(nativeId, binding);
        }
    }
}

void GlobalShortcutMacosBackend::clearRegistrations()
{
    for(const auto& hotKey : m_hotKeys) {
        UnregisterEventHotKey(hotKey.second);
    }
    m_hotKeys.clear();
    m_bindings.clear();
    m_repeater.clear();
}
} // namespace Fooyin::GlobalHotkeys
