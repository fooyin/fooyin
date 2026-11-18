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

#include <gui/brushsource.h>
#include <utils/datastream.h>

#include <QColor>
#include <QDataStream>
#include <QList>
#include <QMetaType>
#include <QPalette>

#include <optional>

namespace Fooyin::Spectrum {
class Colours
{
public:
    static constexpr qint32 Magic   = 0x5350434F;
    static constexpr qint32 Version = 2;

    enum class Type : uint8_t
    {
        Background = 0,
        BarGradient,
        Peaks,
        Text,
        HorizontalGrid,
        VerticalGrid,
        OctaveGrid,
        WhiteKey,
        BlackKey
    };

    void setColour(Type type, const QColor& colour)
    {
        switch(type) {
            case Type::Background:
                m_background = colour;
                break;
            case Type::BarGradient:
                if(colour.isValid()) {
                    setGradient(BrushSource{colour});
                }
                else {
                    m_barGradient.reset();
                }
                break;
            case Type::Peaks:
                m_peaks = colour;
                break;
            case Type::Text:
                m_text = colour;
                break;
            case Type::HorizontalGrid:
                m_horizontalGrid = colour;
                break;
            case Type::VerticalGrid:
                m_verticalGrid = colour;
                break;
            case Type::OctaveGrid:
                m_octaveGrid = colour;
                break;
            case Type::WhiteKey:
                m_whiteKey = colour;
                break;
            case Type::BlackKey:
                m_blackKey = colour;
                break;
        }
    }

    void setGradient(const BrushSource& brush)
    {
        m_barGradient = brush;
    }

    void setLegacyGradient(const std::vector<QColor>& colours)
    {
        m_barGradient.reset();

        LinearGradientSource gradient;
        gradient.stops.clear();

        for(const auto& colour : colours) {
            if(colour.isValid()) {
                gradient.stops.emplace_back(0.0, colour);
            }
        }
        if(gradient.stops.size() == 1) {
            m_barGradient = std::get<QColor>(gradient.stops.front().colour);
        }
        else if(!gradient.stops.empty()) {
            for(size_t index{0}; index < gradient.stops.size(); ++index) {
                gradient.stops[index].position
                    = static_cast<double>(index) / static_cast<double>(gradient.stops.size() - 1);
            }
            m_barGradient = gradient;
        }
    }

    static BrushSource defaultGradient(const QPalette& palette)
    {
        const QColor highlight       = palette.color(QPalette::Active, QPalette::Highlight);
        const QColor highlightedText = palette.color(QPalette::Active, QPalette::HighlightedText);
        const QColor window          = palette.color(QPalette::Active, QPalette::Window);
        const QColor low             = blendColours(highlight, window, 0.55);

        return LinearGradientSource{LinearGradientSource::Direction::AlongControl,
                                    {{.position = 0.0, .colour = blendColours(highlight, highlightedText, 0.25)},
                                     {.position = 0.5, .colour = QPalette::Highlight},
                                     {.position = 1.0, .colour = low}}};
    }

    [[nodiscard]] BrushSource gradient(const QPalette& palette) const
    {
        return m_barGradient ? *m_barGradient : defaultGradient(palette);
    }

    [[nodiscard]] const std::optional<BrushSource>& customGradient() const
    {
        return m_barGradient;
    }

    [[nodiscard]] QColor customColour(Type type) const
    {
        switch(type) {
            case Type::Background:
                return m_background;
            case Type::BarGradient:
                return m_barGradient ? colour(type, QPalette{}) : QColor{};
            case Type::Peaks:
                return m_peaks;
            case Type::Text:
                return m_text;
            case Type::HorizontalGrid:
                return m_horizontalGrid;
            case Type::VerticalGrid:
                return m_verticalGrid;
            case Type::OctaveGrid:
                return m_octaveGrid;
            case Type::WhiteKey:
                return m_whiteKey;
            case Type::BlackKey:
                return m_blackKey;
        }

        return {};
    }

    [[nodiscard]] QColor defaultWhiteKeyColour(const QPalette& palette) const
    {
        const QColor window = palette.color(QPalette::Active, QPalette::Window);
        const QColor light  = palette.color(QPalette::Active, QPalette::Light);
        return blendColours(window, light, 0.75);
    }

    [[nodiscard]] QColor defaultBlackKeyColour(const QPalette& palette) const
    {
        const QColor window = palette.color(QPalette::Active, QPalette::Window);
        const QColor dark   = palette.color(QPalette::Active, QPalette::Dark);
        return blendColours(window, dark, 0.75);
    }

    static QColor defaultGridColour(const QPalette& palette)
    {
        QColor colour = palette.color(QPalette::Active, QPalette::Text);
        colour.setAlpha(65);
        return colour;
    }

    [[nodiscard]] QColor colour(Type type, const QPalette& palette) const
    {
        switch(type) {
            case Type::Background:
                return m_background.isValid() ? m_background : palette.color(QPalette::Window);
            case Type::BarGradient: {
                const auto brush = resolveBrush(gradient(palette), palette, QPalette::Active, QRectF{0, 0, 1, 1});
                return brush.gradient() ? brush.gradient()->stops().front().second : brush.color();
            }
            case Type::Peaks:
                return m_peaks.isValid() ? m_peaks : QColor{190, 40, 10};
            case Type::Text:
                return m_text.isValid() ? m_text : palette.color(QPalette::Text);
            case Type::HorizontalGrid:
                return m_horizontalGrid.isValid() ? m_horizontalGrid : defaultGridColour(palette);
            case Type::VerticalGrid:
                return m_verticalGrid.isValid() ? m_verticalGrid : defaultGridColour(palette);
            case Type::OctaveGrid:
                return m_octaveGrid.isValid() ? m_octaveGrid : defaultGridColour(palette);
            case Type::WhiteKey:
                return m_whiteKey.isValid() ? m_whiteKey : defaultWhiteKeyColour(palette);
            case Type::BlackKey:
                return m_blackKey.isValid() ? m_blackKey : defaultBlackKeyColour(palette);
        }

        return {};
    }

    [[nodiscard]] bool isEmpty() const
    {
        return !m_background.isValid() && !m_barGradient.has_value() && !m_peaks.isValid() && !m_text.isValid()
            && !m_horizontalGrid.isValid() && !m_verticalGrid.isValid() && !m_octaveGrid.isValid()
            && !m_whiteKey.isValid() && !m_blackKey.isValid();
    }

    friend QDataStream& operator<<(QDataStream& stream, const Colours& colours)
    {
        stream << Magic;
        stream << Version;
        stream << colours.m_background;
        stream << (colours.m_barGradient ? brushSourceToJson(*colours.m_barGradient) : QJsonObject{});
        stream << colours.m_peaks;
        stream << colours.m_text;
        stream << colours.m_horizontalGrid;
        stream << colours.m_verticalGrid;
        stream << colours.m_octaveGrid;
        stream << colours.m_whiteKey;
        stream << colours.m_blackKey;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, Colours& colours)
    {
        qint32 magic{0};
        qint32 version{0};

        stream.startTransaction();
        stream >> magic;
        stream >> version;

        if(magic != Magic || (version != 1 && version != Version)) {
            stream.rollbackTransaction();
            colours = {};
            return stream;
        }

        stream >> colours.m_background;

        if(version == 1) {
            std::vector<QColor> legacy;
            DataStream::readContainer(stream, legacy);
            colours.setLegacyGradient(legacy);
        }
        else {
            QJsonObject source;
            stream >> source;

            colours.m_barGradient.reset();

            if(!source.isEmpty()) {
                if(const auto brush = readBrushSource(source, QString{})) {
                    colours.m_barGradient = *brush;
                }
                else {
                    stream.setStatus(QDataStream::ReadCorruptData);
                }
            }
        }
        stream >> colours.m_peaks;
        stream >> colours.m_text;
        stream >> colours.m_horizontalGrid;
        stream >> colours.m_verticalGrid;
        stream >> colours.m_octaveGrid;
        stream >> colours.m_whiteKey;
        stream >> colours.m_blackKey;

        if(!stream.commitTransaction()) {
            colours = {};
        }

        return stream;
    }

private:
    [[nodiscard]] static QColor blendColours(const QColor& base, const QColor& accent, qreal accentRatio)
    {
        const auto blend = [accentRatio](int baseChannel, int accentChannel) {
            return static_cast<int>((baseChannel * (1.0 - accentRatio)) + (accentChannel * accentRatio));
        };

        return QColor{blend(base.red(), accent.red()), blend(base.green(), accent.green()),
                      blend(base.blue(), accent.blue()), blend(base.alpha(), accent.alpha())};
    }

    QColor m_background;
    std::optional<BrushSource> m_barGradient;
    QColor m_peaks;
    QColor m_text;
    QColor m_horizontalGrid;
    QColor m_verticalGrid;
    QColor m_octaveGrid;
    QColor m_whiteKey;
    QColor m_blackKey;
};
} // namespace Fooyin::Spectrum

Q_DECLARE_METATYPE(Fooyin::Spectrum::Colours)
