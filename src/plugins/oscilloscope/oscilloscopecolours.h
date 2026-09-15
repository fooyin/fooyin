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

#pragma once

#include <QColor>
#include <QDataStream>
#include <QMetaType>
#include <QPalette>

namespace Fooyin::Oscilloscope {
class Colours
{
public:
    static constexpr qint32 Magic   = 0x4F53434F;
    static constexpr qint32 Version = 1;

    enum class Type : uint8_t
    {
        Background = 0,
        Waveform,
        ZeroLine
    };

    void setColour(Type type, const QColor& colour)
    {
        switch(type) {
            case Type::Background:
                m_background = colour;
                break;
            case Type::Waveform:
                m_waveform = colour;
                break;
            case Type::ZeroLine:
                m_zeroLine = colour;
                break;
        }
    }

    [[nodiscard]] QColor customColour(Type type) const
    {
        switch(type) {
            case Type::Background:
                return m_background;
            case Type::Waveform:
                return m_waveform;
            case Type::ZeroLine:
                return m_zeroLine;
        }

        return {};
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return customColour(type).isValid();
    }

    [[nodiscard]] QColor colour(Type type, const QPalette& palette) const
    {
        switch(type) {
            case Type::Background:
                return m_background.isValid() ? m_background : palette.color(QPalette::Window);
            case Type::Waveform:
                return m_waveform.isValid() ? m_waveform : palette.color(QPalette::Text);
            case Type::ZeroLine: {
                if(m_zeroLine.isValid()) {
                    return m_zeroLine;
                }
                QColor zeroLine{colour(Type::Waveform, palette)};
                zeroLine.setAlpha(96);
                return zeroLine;
            }
        }

        return {};
    }

    [[nodiscard]] bool isEmpty() const
    {
        return !m_background.isValid() && !m_waveform.isValid() && !m_zeroLine.isValid();
    }

    bool operator==(const Colours& other) const = default;

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << Magic;
        stream << Version;
        stream << colours.m_background;
        stream << colours.m_waveform;
        stream << colours.m_zeroLine;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        qint32 magic{0};
        qint32 version{0};

        stream.startTransaction();
        stream >> magic;
        stream >> version;

        if(magic != Magic || version != Version) {
            stream.rollbackTransaction();
            colours = {};
            return stream;
        }

        stream >> colours.m_background;
        stream >> colours.m_waveform;
        stream >> colours.m_zeroLine;

        if(!stream.commitTransaction()) {
            colours = {};
        }

        return stream;
    }

private:
    QColor m_background;
    QColor m_waveform;
    QColor m_zeroLine;
};
} // namespace Fooyin::Oscilloscope

Q_DECLARE_METATYPE(Fooyin::Oscilloscope::Colours)
