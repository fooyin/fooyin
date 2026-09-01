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

#include "globalshortcutx11backend.h"

#include "globalshortcutkeyutils.h"

#include <QSocketNotifier>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <array>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace Fooyin::GlobalHotkeys {
namespace {
int& lastX11Error()
{
    thread_local int lastError{0};
    return lastError;
}

int recordX11Error(Display* /*display*/, XErrorEvent* event)
{
    lastX11Error() = event->error_code;
    return 0;
}

uint64_t shortcutKey(unsigned int keycode, unsigned int modifiers)
{
    return (static_cast<uint64_t>(modifiers) << 32) | keycode;
}

unsigned int nativeModifiers(Qt::KeyboardModifiers modifiers)
{
    unsigned int native{0};
    if(modifiers.testFlag(Qt::ShiftModifier)) {
        native |= ShiftMask;
    }
    if(modifiers.testFlag(Qt::ControlModifier)) {
        native |= ControlMask;
    }
    if(modifiers.testFlag(Qt::AltModifier)) {
        native |= Mod1Mask;
    }
    if(modifiers.testFlag(Qt::MetaModifier)) {
        native |= Mod4Mask;
    }
    return native;
}

unsigned int modifierMaskForKeysym(Display* display, KeySym keysym)
{
    const KeyCode keycode = XKeysymToKeycode(display, keysym);
    if(keycode == 0) {
        return 0;
    }

    unsigned int result{0};
    if(XModifierKeymap* mapping = XGetModifierMapping(display)) {
        for(int modifier{0}; modifier < 8; ++modifier) {
            for(int key{0}; key < mapping->max_keypermod; ++key) {
                if(mapping->modifiermap[(modifier * mapping->max_keypermod) + key] == keycode) {
                    result = 1 << modifier;
                    break;
                }
            }
        }
        XFreeModifiermap(mapping);
    }
    return result;
}
} // namespace

class GlobalShortcutX11BackendPrivate
{
public:
    explicit GlobalShortcutX11BackendPrivate(GlobalShortcutX11Backend* self)
        : m_self{self}
        , display{XOpenDisplay(nullptr)}
    {
        if(!display) {
            return;
        }

        rootWindow  = DefaultRootWindow(display);
        numLockMask = modifierMaskForKeysym(display, XK_Num_Lock);
        scrollMask  = modifierMaskForKeysym(display, XK_Scroll_Lock);

        int supported{False};
        XkbSetDetectableAutoRepeat(display, True, &supported);

        notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(display), QSocketNotifier::Read, m_self);
        QObject::connect(notifier.get(), &QSocketNotifier::activated, m_self, [this] { processEvents(); });
    }

    ~GlobalShortcutX11BackendPrivate()
    {
        clear();
        notifier.reset();

        if(display) {
            XCloseDisplay(display);
        }
    }

    void clear()
    {
        if(display) {
            XUngrabKey(display, AnyKey, AnyModifier, rootWindow);
            XSync(display, False);
        }
        shortcuts.clear();
        pressedKeys.clear();
    }

    bool grab(KeyCode keycode, unsigned int modifiers)
    {
        const std::array<unsigned int, 3> locks{LockMask, numLockMask, scrollMask};
        std::set<unsigned int> masks{modifiers};

        for(const unsigned int lock : locks) {
            if(lock == 0) {
                continue;
            }

            const auto currentMasks = masks;
            for(const unsigned int mask : currentMasks) {
                masks.insert(mask | lock);
            }
        }

        for(const unsigned int mask : masks) {
            lastX11Error() = 0;

            const auto oldHandler = XSetErrorHandler(recordX11Error);
            XGrabKey(display, keycode, mask, rootWindow, False, GrabModeAsync, GrabModeAsync);
            XSync(display, False);
            XSetErrorHandler(oldHandler);

            if(lastX11Error() != 0) {
                for(const unsigned int registeredMask : masks) {
                    XUngrabKey(display, keycode, registeredMask, rootWindow);
                }
                XSync(display, False);
                return false;
            }
        }
        return true;
    }

    void processEvents()
    {
        static constexpr unsigned int RelevantModifiers = ShiftMask | ControlMask | Mod1Mask | Mod4Mask;

        while(display && XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if(event.type == KeyRelease) {
                bool autoRepeat{false};
                if(XPending(display) > 0) {
                    XEvent nextEvent;
                    XPeekEvent(display, &nextEvent);
                    autoRepeat = nextEvent.type == KeyPress && nextEvent.xkey.keycode == event.xkey.keycode
                              && nextEvent.xkey.time == event.xkey.time;
                }

                if(!autoRepeat) {
                    pressedKeys.erase(event.xkey.keycode);
                }

                continue;
            }

            if(event.type != KeyPress) {
                continue;
            }

            const unsigned int keycode   = event.xkey.keycode;
            const unsigned int modifiers = event.xkey.state & RelevantModifiers;
            const uint64_t key           = shortcutKey(keycode, modifiers);

            if(pressedKeys.contains(keycode) || !shortcuts.contains(key)) {
                continue;
            }

            pressedKeys.insert(keycode);
            Q_EMIT m_self->activated(shortcuts.at(key).commandId);
        }
    }

    GlobalShortcutX11Backend* m_self;
    Display* display{nullptr};
    Window rootWindow{0};
    unsigned int numLockMask{0};
    unsigned int scrollMask{0};
    std::unique_ptr<QSocketNotifier> notifier;
    std::unordered_map<std::uint64_t, GlobalShortcutDescriptor> shortcuts;
    std::unordered_set<unsigned int> pressedKeys;
};

GlobalShortcutX11Backend::GlobalShortcutX11Backend(QObject* parent)
    : GlobalShortcutBackend{parent}
    , p{std::make_unique<GlobalShortcutX11BackendPrivate>(this)}
{ }

GlobalShortcutX11Backend::~GlobalShortcutX11Backend() = default;

GlobalShortcutAvailability GlobalShortcutX11Backend::availability() const
{
    return p->display ? GlobalShortcutAvailability::Available : GlobalShortcutAvailability::Unavailable;
}

void GlobalShortcutX11Backend::applyBindings(const GlobalShortcutDescriptorList& bindings)
{
    p->clear();

    if(!p->display) {
        return;
    }

    for(const auto& binding : bindings) {
        if(binding.shortcut.count() != 1) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("X11 global shortcuts must contain one key combination"));
            continue;
        }

        const QKeyCombination combination = binding.shortcut[0];
        const QByteArray keyName          = xkbKeyName(combination.key()).toLatin1();
        const KeySym keysym               = XStringToKeysym(keyName.constData());
        const KeyCode keycode             = keysym == NoSymbol ? 0 : XKeysymToKeycode(p->display, keysym);
        const unsigned int modifiers      = nativeModifiers(combination.keyboardModifiers());
        const uint64_t key                = shortcutKey(keycode, modifiers);

        if(keycode == 0) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("The shortcut cannot be represented on X11"));
        }
        else if(p->shortcuts.contains(key)) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut, tr("The shortcut is already registered"));
        }
        else if(!p->grab(keycode, modifiers)) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("Another application has already registered this shortcut"));
        }
        else {
            p->shortcuts.emplace(key, binding);
        }
    }
}

void GlobalShortcutX11Backend::clearBindings()
{
    p->clear();
}
} // namespace Fooyin::GlobalHotkeys
