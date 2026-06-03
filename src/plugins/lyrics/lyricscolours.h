/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QApplication>
#include <QColor>
#include <QDataStream>
#include <QMetaType>
#include <QPalette>

namespace Fooyin::Lyrics {
struct Colours
{
    enum class Type : uint8_t
    {
        Background = 0,
        LineUnplayed,
        LinePlayed,
        LineSynced,
        WordLineSynced,
        WordSynced,
        LineUnsynced
    };

    QMap<Type, QColor> lyricsColours;

    [[nodiscard]] static QColor blendColours(const QColor& base, const QColor& accent, qreal accentRatio)
    {
        const auto blend = [accentRatio](int baseChannel, int accentChannel) {
            return static_cast<int>((baseChannel * (1.0 - accentRatio)) + (accentChannel * accentRatio));
        };

        return QColor{blend(base.red(), accent.red()), blend(base.green(), accent.green()),
                      blend(base.blue(), accent.blue()), blend(base.alpha(), accent.alpha())};
    }

    [[nodiscard]] static QColor emphasisedWordColour(const QColor& lineColour,
                                                     const QPalette& palette = QApplication::palette())
    {
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);

        float hue{0.0};
        float saturation{0.0};
        float lightness{0.0};
        float alpha{0.0};

        const QColor currentLine{lineColour};
        currentLine.getHslF(&hue, &saturation, &lightness, &alpha);

        if(saturation > 0.08) {
            saturation = std::min(1.0F, saturation + 0.25F);

            if(lightness >= 0.5) {
                lightness = std::max(0.0F, lightness - 0.18F);
            }
            else {
                lightness = std::min(1.0F, lightness + 0.18F);
            }

            return QColor::fromHslF(hue, saturation, lightness, alpha);
        }

        return blendColours(currentLine, highlight, 0.45);
    }

    [[nodiscard]] static QColor dimmedTextColour(const QPalette& palette = QApplication::palette())
    {
        QColor colour = palette.color(QPalette::Active, QPalette::Text);
        colour.setAlpha(std::max(1, colour.alpha() / 2));
        return colour;
    }

    [[nodiscard]] static QColor defaultColour(Type type, const QPalette& palette = QApplication::palette())
    {
        switch(type) {
            case Type::Background:
                return palette.color(QPalette::Active, QPalette::Base);
            case Type::LineUnplayed:
                return dimmedTextColour(palette);
            case Type::LinePlayed:
                return dimmedTextColour(palette);
            case Type::LineSynced:
                return palette.color(QPalette::Active, QPalette::Text);
            case Type::WordLineSynced:
                return palette.color(QPalette::Active, QPalette::Text);
            case Type::WordSynced:
                return emphasisedWordColour(defaultColour(Type::WordLineSynced, palette), palette);
            case Type::LineUnsynced:
                return palette.color(QPalette::Active, QPalette::Text);
            default:
                return {};
        }
    }

    [[nodiscard]] QColor colour(Type type, const QPalette& palette = QApplication::palette()) const
    {
        const QColor override = lyricsColours.value(type);
        if(override.isValid()) {
            return override;
        }

        if(type == Type::WordSynced) {
            return emphasisedWordColour(colour(Type::WordLineSynced, palette), palette);
        }

        return defaultColour(type, palette);
    }

    [[nodiscard]] bool hasOverride(Type type) const
    {
        return lyricsColours.contains(type);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return lyricsColours.isEmpty();
    }

    void setColour(Type type, const QColor& colour)
    {
        if(colour.isValid()) {
            lyricsColours[type] = colour;
        }
        else {
            lyricsColours.remove(type);
        }
    }

    bool operator==(const Colours& other) const
    {
        return std::tie(lyricsColours) == std::tie(other.lyricsColours);
    };

    bool operator!=(const Colours& other) const
    {
        return !(*this == other);
    };

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << colours.lyricsColours;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        stream >> colours.lyricsColours;
        return stream;
    }
};
} // namespace Fooyin::Lyrics
