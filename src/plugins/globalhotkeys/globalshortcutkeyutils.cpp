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

#include "globalshortcutkeyutils.h"

#include <QStringList>

#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin::GlobalHotkeys {
QString xkbKeyName(Qt::Key key)
{
    if(key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QChar{static_cast<char16_t>(u'a' + (key - Qt::Key_A))};
    }
    if(key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QChar{static_cast<char16_t>(u'0' + (key - Qt::Key_0))};
    }
    if(key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        return u"F"_s + QString::number(key - Qt::Key_F1 + 1);
    }

    switch(key) {
        case Qt::Key_Space:
            return u"space"_s;
        case Qt::Key_Escape:
            return u"Escape"_s;
        case Qt::Key_Tab:
            return u"Tab"_s;
        case Qt::Key_Backtab:
            return u"ISO_Left_Tab"_s;
        case Qt::Key_Backspace:
            return u"BackSpace"_s;
        case Qt::Key_Return:
            return u"Return"_s;
        case Qt::Key_Enter:
            return u"KP_Enter"_s;
        case Qt::Key_Insert:
            return u"Insert"_s;
        case Qt::Key_Delete:
            return u"Delete"_s;
        case Qt::Key_Pause:
            return u"Pause"_s;
        case Qt::Key_Print:
            return u"Print"_s;
        case Qt::Key_Home:
            return u"Home"_s;
        case Qt::Key_End:
            return u"End"_s;
        case Qt::Key_Left:
            return u"Left"_s;
        case Qt::Key_Up:
            return u"Up"_s;
        case Qt::Key_Right:
            return u"Right"_s;
        case Qt::Key_Down:
            return u"Down"_s;
        case Qt::Key_PageUp:
            return u"Prior"_s;
        case Qt::Key_PageDown:
            return u"Next"_s;
        case Qt::Key_VolumeDown:
            return u"XF86AudioLowerVolume"_s;
        case Qt::Key_VolumeMute:
            return u"XF86AudioMute"_s;
        case Qt::Key_VolumeUp:
            return u"XF86AudioRaiseVolume"_s;
        case Qt::Key_MediaPlay:
        case Qt::Key_MediaTogglePlayPause:
            return u"XF86AudioPlay"_s;
        case Qt::Key_MediaPause:
            return u"XF86AudioPause"_s;
        case Qt::Key_MediaStop:
            return u"XF86AudioStop"_s;
        case Qt::Key_MediaPrevious:
            return u"XF86AudioPrev"_s;
        case Qt::Key_MediaNext:
            return u"XF86AudioNext"_s;
        case Qt::Key_MicMute:
            return u"XF86AudioMicMute"_s;
        default:
            break;
    }

    static const std::map<Qt::Key, QString> punctuation{
        {Qt::Key_Exclam, u"exclam"_s},
        {Qt::Key_QuoteDbl, u"quotedbl"_s},
        {Qt::Key_NumberSign, u"numbersign"_s},
        {Qt::Key_Dollar, u"dollar"_s},
        {Qt::Key_Percent, u"percent"_s},
        {Qt::Key_Ampersand, u"ampersand"_s},
        {Qt::Key_Apostrophe, u"apostrophe"_s},
        {Qt::Key_ParenLeft, u"parenleft"_s},
        {Qt::Key_ParenRight, u"parenright"_s},
        {Qt::Key_Asterisk, u"asterisk"_s},
        {Qt::Key_Plus, u"plus"_s},
        {Qt::Key_Comma, u"comma"_s},
        {Qt::Key_Minus, u"minus"_s},
        {Qt::Key_Period, u"period"_s},
        {Qt::Key_Slash, u"slash"_s},
        {Qt::Key_Colon, u"colon"_s},
        {Qt::Key_Semicolon, u"semicolon"_s},
        {Qt::Key_Less, u"less"_s},
        {Qt::Key_Equal, u"equal"_s},
        {Qt::Key_Greater, u"greater"_s},
        {Qt::Key_Question, u"question"_s},
        {Qt::Key_At, u"at"_s},
        {Qt::Key_BracketLeft, u"bracketleft"_s},
        {Qt::Key_Backslash, u"backslash"_s},
        {Qt::Key_BracketRight, u"bracketright"_s},
        {Qt::Key_AsciiCircum, u"asciicircum"_s},
        {Qt::Key_Underscore, u"underscore"_s},
        {Qt::Key_QuoteLeft, u"grave"_s},
        {Qt::Key_BraceLeft, u"braceleft"_s},
        {Qt::Key_Bar, u"bar"_s},
        {Qt::Key_BraceRight, u"braceright"_s},
        {Qt::Key_AsciiTilde, u"asciitilde"_s},
    };

    const auto punctuationKey = punctuation.find(key);
    return punctuationKey != punctuation.cend() ? punctuationKey->second : QString{};
}

QString xdgShortcutTrigger(const QKeySequence& shortcut)
{
    if(shortcut.count() != 1) {
        return {};
    }

    const QKeyCombination combination = shortcut[0];
    const QString keyName             = xkbKeyName(combination.key());
    if(keyName.isEmpty()) {
        return {};
    }

    QStringList parts;

    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    if(modifiers.testFlag(Qt::ControlModifier)) {
        parts.append(u"CTRL"_s);
    }
    if(modifiers.testFlag(Qt::AltModifier)) {
        parts.append(u"ALT"_s);
    }
    if(modifiers.testFlag(Qt::ShiftModifier)) {
        parts.append(u"SHIFT"_s);
    }
    if(modifiers.testFlag(Qt::MetaModifier)) {
        parts.append(u"LOGO"_s);
    }
    if(modifiers.testFlag(Qt::KeypadModifier)) {
        parts.append(u"NUM"_s);
    }

    parts.append(keyName);
    return parts.join(u'+');
}
} // namespace Fooyin::GlobalHotkeys
