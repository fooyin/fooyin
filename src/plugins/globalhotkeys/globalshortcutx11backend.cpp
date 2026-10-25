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

#include <QLoggingCategory>
#include <QSocketNotifier>

#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

Q_LOGGING_CATEGORY(X11_SHORTCUTS, "fy.globalshortcuts.x11")

namespace Fooyin::GlobalHotkeys {
namespace {
using NativeShortcut    = std::pair<KeyCode, unsigned int>;
using NativeShortcutSet = std::set<NativeShortcut>;

uint64_t shortcutKey(unsigned int keycode, unsigned int modifiers)
{
    return (static_cast<uint64_t>(modifiers) << 32U) | keycode;
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

unsigned int modifierMaskForKeysym(XkbDescPtr keymap, KeySym keysym)
{
    unsigned int result{0};
    for(int code{keymap->min_key_code}; std::cmp_less_equal(code, keymap->max_key_code); ++code) {
        const std::span<const KeySym> symbols{XkbKeySymsPtr(keymap, code),
                                              static_cast<size_t>(XkbKeyNumSyms(keymap, code))};
        if(std::ranges::find(symbols, keysym) != symbols.end()) {
            result |= keymap->map->modmap[code];
        }
    }
    return result;
}

unsigned int virtualModifierMask(Display* display, XkbDescPtr keymap, const char* name)
{
    if(!keymap->names || !keymap->server) {
        return 0;
    }

    const Atom atom = XInternAtom(display, name, True);
    for(int modifier{0}; atom != None && modifier < XkbNumVirtualMods; ++modifier) {
        if(keymap->names->vmods[modifier] == atom) {
            return keymap->server->vmods[modifier];
        }
    }

    return 0;
}

bool matchesKeysym(KeySym actual, KeySym expected)
{
    KeySym lowerActual{NoSymbol};
    KeySym lowerExpected{NoSymbol};
    KeySym upper{NoSymbol};
    XConvertCase(actual, &lowerActual, &upper);
    XConvertCase(expected, &lowerExpected, &upper);
    return lowerActual == lowerExpected;
}
} // namespace

class GlobalShortcutX11BackendPrivate
{
public:
    explicit GlobalShortcutX11BackendPrivate(GlobalShortcutX11Backend* self);
    ~GlobalShortcutX11BackendPrivate();

    void clear();

    [[nodiscard]] NativeShortcutSet resolve(QKeyCombination combination, XkbDescPtr keymap, unsigned int group) const;
    bool grab(const NativeShortcutSet& nativeShortcuts);
    void registerBindings(bool reportFailures);

    void scheduleRefresh();
    void processEvents();

    GlobalShortcutX11Backend* m_self;
    Display* display{nullptr};
    xcb_connection_t* connection{nullptr};
    Window rootWindow{0};
    int xkbEventBase{0};
    int activeGroup{0};
    unsigned int numLockMask{0};
    unsigned int scrollMask{0};
    unsigned int level3Mask{0};
    bool refreshScheduled{false};
    GlobalShortcutDescriptorList bindings;
    std::unique_ptr<QSocketNotifier> notifier;
    std::unordered_map<std::uint64_t, GlobalShortcutDescriptor> shortcuts;
};

GlobalShortcutX11BackendPrivate::GlobalShortcutX11BackendPrivate(GlobalShortcutX11Backend* self)
    : m_self{self}
    , display{XOpenDisplay(nullptr)}
{
    if(!display) {
        return;
    }

    int opcode{0};
    int errorBase{0};
    int major{XkbMajorVersion};
    int minor{XkbMinorVersion};

    connection = XGetXCBConnection(display);

    if(!connection || !XkbQueryExtension(display, &opcode, &xkbEventBase, &errorBase, &major, &minor)) {
        XCloseDisplay(display);
        display = nullptr;
        return;
    }

    rootWindow                           = DefaultRootWindow(display);
    static constexpr unsigned int Events = XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask;
    XkbSelectEvents(display, XkbUseCoreKbd, Events, Events);
    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask, XkbGroupStateMask);

    int supported{False};
    XkbSetDetectableAutoRepeat(display, True, &supported);

    notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(display), QSocketNotifier::Read, m_self);
    QObject::connect(notifier.get(), &QSocketNotifier::activated, m_self, [this] { processEvents(); });
}

GlobalShortcutX11BackendPrivate::~GlobalShortcutX11BackendPrivate()
{
    clear();
    notifier.reset();

    if(display) {
        XCloseDisplay(display);
    }
}

void GlobalShortcutX11BackendPrivate::clear()
{
    if(display) {
        XUngrabKey(display, AnyKey, AnyModifier, rootWindow);
        XSync(display, False);
    }
    shortcuts.clear();
}

NativeShortcutSet GlobalShortcutX11BackendPrivate::resolve(QKeyCombination combination, XkbDescPtr keymap,
                                                           unsigned int group) const
{
    const QByteArray keyName    = xkbKeyName(combination.key()).toLatin1();
    KeySym keysym               = XStringToKeysym(keyName.constData());
    const QByteArray keypadName = xkbKeypadName(combination.key()).toLatin1();
    const KeySym keypad         = XStringToKeysym(keypadName.constData());

    if(combination.keyboardModifiers().testFlag(Qt::KeypadModifier) || keysym == keypad) {
        keysym = NoSymbol;
    }

    const std::array<unsigned int, 3> locks{LockMask, numLockMask, scrollMask};
    std::set<unsigned int> masks{0};

    for(const unsigned int lock : locks) {
        if(lock == 0) {
            continue;
        }

        const auto currentMasks{masks};
        for(const unsigned int mask : currentMasks) {
            masks.insert(mask | lock);
        }
    }

    NativeShortcutSet nativeShortcuts;
    const auto modifiers = nativeModifiers(combination.keyboardModifiers());
    const std::array<unsigned int, 4> characterModifiers{0, ShiftMask, level3Mask, ShiftMask | level3Mask};
    for(int code{keymap->min_key_code}; std::cmp_less_equal(code, keymap->max_key_code); ++code) {
        const auto keycode = static_cast<KeyCode>(code);

        for(const unsigned int mask : masks) {
            KeySym translated{NoSymbol};
            unsigned int consumed{0};
            if(keysym != NoSymbol) {
                for(const auto characterModifier : characterModifiers) {
                    if(XkbTranslateKeyCode(keymap, keycode, XkbBuildCoreState(mask | characterModifier, group),
                                           &consumed, &translated)
                       && matchesKeysym(translated, keysym)) {
                        nativeShortcuts.emplace(keycode, modifiers | mask | characterModifier);
                        break;
                    }
                }
            }

            // Keypad digits and navigation share keycodes, but depend on Num Lock and Shift
            const bool keypadMatch = keypad != NoSymbol
                                  && XkbTranslateKeyCode(keymap, keycode, XkbBuildCoreState(modifiers | mask, group),
                                                         &consumed, &translated)
                                  && translated == keypad;
            if(keypadMatch) {
                nativeShortcuts.emplace(keycode, modifiers | mask);
            }
        }
    }
    return nativeShortcuts;
}

bool GlobalShortcutX11BackendPrivate::grab(const NativeShortcutSet& nativeShortcuts)
{
    std::vector<xcb_void_cookie_t> requests;
    requests.reserve(nativeShortcuts.size());

    XFlush(display);

    for(const auto& [keycode, modifiers] : nativeShortcuts) {
        requests.push_back(xcb_grab_key_checked(connection, False, rootWindow, modifiers, keycode, XCB_GRAB_MODE_ASYNC,
                                                XCB_GRAB_MODE_ASYNC));
    }

    bool failed{false};
    for(const auto request : requests) {
        if(auto* error = xcb_request_check(connection, request)) {
            failed = true;
            std::free(error);
        }
    }
    if(failed) {
        for(const auto& [keycode, modifiers] : nativeShortcuts) {
            XUngrabKey(display, keycode, modifiers, rootWindow);
        }
        XSync(display, False);
        return false;
    }
    return true;
}

void GlobalShortcutX11BackendPrivate::registerBindings(bool reportFailures)
{
    clear();

    if(!display || bindings.empty()) {
        return;
    }

    const auto freeKeymap = [](XkbDescPtr keymap) {
        XkbFreeKeyboard(keymap, XkbAllComponentsMask, True);
    };
    const std::unique_ptr<XkbDescRec, decltype(freeKeymap)> keymap{
        XkbGetMap(display, XkbAllClientInfoMask | XkbVirtualModsMask, XkbUseCoreKbd)};
    XkbStateRec state{};
    const bool haveKeymap = keymap && XkbGetState(display, XkbUseCoreKbd, &state) == Success;

    if(haveKeymap) {
        activeGroup = state.group;
        numLockMask = modifierMaskForKeysym(keymap.get(), XK_Num_Lock);
        scrollMask  = modifierMaskForKeysym(keymap.get(), XK_Scroll_Lock);
        XkbGetNames(display, XkbVirtualModNamesMask, keymap.get());
        level3Mask = virtualModifierMask(display, keymap.get(), "LevelThree");
        if(level3Mask == 0) {
            level3Mask = modifierMaskForKeysym(keymap.get(), XK_ISO_Level3_Shift);
        }
    }

    const auto fail = [this, reportFailures](const GlobalShortcutDescriptor& binding, const QString& error) {
        if(reportFailures) {
            Q_EMIT m_self->registrationFailed(binding.commandId, binding.shortcut, error);
        }
        else {
            qCWarning(X11_SHORTCUTS) << "Failed to refresh" << binding.shortcut.toString() << ':' << error;
        }
    };

    for(const auto& binding : bindings) {
        if(binding.shortcut.count() != 1) {
            fail(binding, GlobalShortcutX11Backend::tr("X11 global shortcuts must contain one key combination"));
            continue;
        }

        const auto nativeShortcuts
            = haveKeymap ? resolve(binding.shortcut[0], keymap.get(), state.group) : NativeShortcutSet{};

        if(nativeShortcuts.empty()) {
            fail(binding, GlobalShortcutX11Backend::tr("The shortcut cannot be represented on X11"));
        }
        else if(std::ranges::any_of(nativeShortcuts, [this](const auto& shortcut) {
                    return shortcuts.contains(shortcutKey(shortcut.first, shortcut.second));
                })) {
            fail(binding, GlobalShortcutX11Backend::tr("The shortcut is already registered"));
        }
        else if(!grab(nativeShortcuts)) {
            fail(binding, GlobalShortcutX11Backend::tr("Another application has already registered this shortcut"));
        }
        else {
            for(const auto& [keycode, modifiers] : nativeShortcuts) {
                shortcuts.emplace(shortcutKey(keycode, modifiers), binding);
            }
        }
    }

    QMetaObject::invokeMethod(m_self, [this] { processEvents(); }, Qt::QueuedConnection);
}

void GlobalShortcutX11BackendPrivate::scheduleRefresh()
{
    if(std::exchange(refreshScheduled, true)) {
        return;
    }

    QMetaObject::invokeMethod(
        m_self,
        [this] {
            if(std::exchange(refreshScheduled, false)) {
                registerBindings(false);
            }
        },
        Qt::QueuedConnection);
}

void GlobalShortcutX11BackendPrivate::processEvents()
{
    static constexpr unsigned int CoreModifiers
        = ShiftMask | LockMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

    while(display && XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);

        if(event.type == xkbEventBase) {
            const auto* xkbEvent = reinterpret_cast<XkbEvent*>(&event);
            if(xkbEvent->any.xkb_type == XkbMapNotify || xkbEvent->any.xkb_type == XkbNewKeyboardNotify
               || (xkbEvent->any.xkb_type == XkbStateNotify && xkbEvent->state.group != activeGroup)) {
                scheduleRefresh();
            }
            continue;
        }
        if(event.type == MappingNotify) {
            XRefreshKeyboardMapping(&event.xmapping);
            if(event.xmapping.request == MappingKeyboard || event.xmapping.request == MappingModifier) {
                scheduleRefresh();
            }
            continue;
        }
        if(event.type != KeyPress) {
            continue;
        }
        if(std::exchange(refreshScheduled, false)) {
            registerBindings(false);
        }

        const unsigned int keycode   = event.xkey.keycode;
        const unsigned int modifiers = event.xkey.state & CoreModifiers;
        const uint64_t key           = shortcutKey(keycode, modifiers);

        if(!shortcuts.contains(key)) {
            continue;
        }

        Q_EMIT m_self->activated(shortcuts.at(key).commandId);
    }
}

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
    p->bindings = bindings;
    p->registerBindings(true);
}

void GlobalShortcutX11Backend::clearBindings()
{
    p->bindings.clear();
    p->clear();
}
} // namespace Fooyin::GlobalHotkeys
