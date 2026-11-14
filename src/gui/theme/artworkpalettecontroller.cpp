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

#include "artworkpalettecontroller.h"

#include <core/player/playercontroller.h>
#include <gui/coverrepository.h>
#include <gui/guiutils.h>
#include <gui/internalguisettings.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <array>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <numeric>
#include <vector>

namespace Fooyin {
namespace {
ArtworkColourMode colourMode(int value)
{
    switch(value) {
        case static_cast<int>(ArtworkColourMode::Accent):
            return ArtworkColourMode::Accent;
        case static_cast<int>(ArtworkColourMode::Tonal):
            return ArtworkColourMode::Tonal;
        default:
            return ArtworkColourMode::Off;
    }
}

QColor artworkColour(const QImage& image)
{
    struct Swatch
    {
        int count{0};
        int red{0};
        int green{0};
        int blue{0};
    };

    std::array<Swatch, 512> swatches{};
    int samples{0};
    for(int y{0}; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for(int x{0}; x < image.width(); ++x) {
            const auto pixel = pixels[x];
            if(qAlpha(pixel) < 128) {
                continue;
            }

            const int red   = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue  = qBlue(pixel);

            auto& swatch = swatches[((red >> 5) << 6) | ((green >> 5) << 3) | (blue >> 5)];
            ++swatch.count;
            swatch.red += red;
            swatch.green += green;
            swatch.blue += blue;
            ++samples;
        }
    }

    QColor result;
    double bestScore{-1.0};
    for(int red{0}; red < 8; ++red) {
        for(int green{0}; green < 8; ++green) {
            for(int blue{0}; blue < 8; ++blue) {
                if(swatches[(red << 6) | (green << 3) | blue].count == 0) {
                    continue;
                }

                Swatch candidate;
                for(int r{std::max(0, red - 1)}; r <= std::min(7, red + 1); ++r) {
                    for(int g{std::max(0, green - 1)}; g <= std::min(7, green + 1); ++g) {
                        for(int b{std::max(0, blue - 1)}; b <= std::min(7, blue + 1); ++b) {
                            const auto& swatch = swatches[(r << 6) | (g << 3) | b];
                            candidate.count += swatch.count;
                            candidate.red += swatch.red;
                            candidate.green += swatch.green;
                            candidate.blue += swatch.blue;
                        }
                    }
                }

                if(candidate.count < std::max(1, samples / 100)) {
                    continue;
                }

                const QColor colour{candidate.red / candidate.count, candidate.green / candidate.count,
                                    candidate.blue / candidate.count};
                const double chroma = std::max({colour.redF(), colour.greenF(), colour.blueF()})
                                    - std::min({colour.redF(), colour.greenF(), colour.blueF()});
                const double tone   = 0.5 + (0.5 * std::sin(std::numbers::pi * colour.lightnessF()));
                const double score  = std::sqrt(static_cast<double>(candidate.count)) * (0.15 + chroma) * tone;
                if(score > bestScore) {
                    bestScore = score;
                    result    = colour;
                }
            }
        }
    }

    return result;
}

double luminance(const QColor& colour)
{
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return (0.2126 * linear(colour.redF())) + (0.7152 * linear(colour.greenF())) + (0.0722 * linear(colour.blueF()));
}

double minimumContrast(const QColor& colour, std::initializer_list<QColor> backgrounds)
{
    const double foreground = luminance(colour);
    double contrast{21.0};
    for(const auto& background : backgrounds) {
        const double value = luminance(background);
        contrast = std::min(contrast, (std::max(foreground, value) + 0.05) / (std::min(foreground, value) + 0.05));
    }
    return contrast;
}

QColor mix(const QColor& first, const QColor& second, double amount)
{
    return QColor::fromRgbF(std::lerp(first.redF(), second.redF(), amount),
                            std::lerp(first.greenF(), second.greenF(), amount),
                            std::lerp(first.blueF(), second.blueF(), amount), first.alphaF());
}

QColor readableColour(const QColor& colour, std::initializer_list<QColor> backgrounds, double minimum)
{
    if(minimumContrast(colour, backgrounds) >= minimum) {
        return colour;
    }

    const QColor black{Qt::black};
    const QColor white{Qt::white};
    const auto& target = minimumContrast(black, backgrounds) > minimumContrast(white, backgrounds) ? black : white;

    for(int step{1}; step <= 100; ++step) {
        const auto candidate = mix(colour, target, step / 100.0);
        if(minimumContrast(candidate, backgrounds) >= minimum) {
            return candidate;
        }
    }

    return target;
}

std::vector<QColor> selectionBackgrounds(const QStyle* style, const QPalette& palette, QPalette::ColorGroup group)
{
    QImage image{96, 32, QImage::Format_RGB32};

    QStyleOptionViewItem opt;
    opt.rect                   = image.rect();
    opt.palette                = palette;
    opt.showDecorationSelected = true;
    opt.viewItemPosition       = QStyleOptionViewItem::OnlyOne;
    opt.state                  = QStyle::State_Enabled | QStyle::State_Selected;
    opt.palette.setCurrentColorGroup(group);
    if(group == QPalette::Active) {
        opt.state |= QStyle::State_Active | QStyle::State_HasFocus;
    }

    std::vector colours{palette.color(group, QPalette::Highlight).rgb()};
    for(const bool hovered : {false, true}) {
        opt.state.setFlag(QStyle::State_MouseOver, hovered);
        image.fill(palette.color(group, QPalette::Base));
        {
            QPainter painter{&image};
            style->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &painter);
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, &painter);
        }

        for(int y{4}; y < image.height() - 4; ++y) {
            const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x{8}; x < image.width() - 8; ++x) {
                if(x == 8 || pixels[x] != pixels[x - 1]) {
                    colours.push_back(pixels[x]);
                }
            }
        }
    }
    std::ranges::sort(colours);
    colours.erase(std::ranges::unique(colours).begin(), colours.end());

    std::vector<QColor> backgrounds;
    backgrounds.reserve(colours.size());
    for(const auto colour : colours) {
        backgrounds.emplace_back(colour);
    }
    return backgrounds;
}

double selectionContrast(const QColor& foreground, const std::vector<QColor>& backgrounds)
{
    const double fgLuminance = luminance(foreground);
    double contrast{21.0};
    for(const auto& background : backgrounds) {
        const double bgLuminance = luminance(background);
        const double textLuminance
            = foreground.alpha() == 255 ? fgLuminance : luminance(mix(background, foreground, foreground.alphaF()));
        contrast = std::min(contrast, (std::max(textLuminance, bgLuminance) + 0.05)
                                          / (std::min(textLuminance, bgLuminance) + 0.05));
    }
    return contrast;
}

void setSelectionColours(const QStyle* style, const QPalette& base, QPalette& palette, QPalette::ColorGroup group,
                         const QColor& highlight)
{
    palette.setColor(group, QPalette::Highlight, highlight);

    auto backgrounds = selectionBackgrounds(style, palette, group);
    // Keep selected text at full strength when the view loses focus
    const auto fgGroup = group == QPalette::Inactive ? QPalette::Active : group;
    const std::array foregrounds{base.color(fgGroup, QPalette::HighlightedText), base.color(fgGroup, QPalette::Text),
                                 base.color(fgGroup, QPalette::WindowText), base.color(fgGroup, QPalette::ButtonText)};
    auto foreground     = foregrounds.front();
    double bestContrast = selectionContrast(foreground, backgrounds);
    if(bestContrast >= 4.5) {
        palette.setColor(group, QPalette::HighlightedText, foreground);
        return;
    }

    for(size_t i{1}; i < foregrounds.size(); ++i) {
        const double contrast = selectionContrast(foregrounds[i], backgrounds);
        if(contrast > bestContrast) {
            foreground   = foregrounds[i];
            bestContrast = contrast;
        }
    }
    if(bestContrast >= 4.5) {
        palette.setColor(group, QPalette::HighlightedText, foreground);
        return;
    }

    const QColor black{Qt::black};
    const QColor white{Qt::white};
    const bool darkText          = selectionContrast(black, backgrounds) >= selectionContrast(white, backgrounds);
    const auto& textTarget       = darkText ? black : white;
    const auto& backgroundTarget = darkText ? white : black;
    palette.setColor(group, QPalette::HighlightedText, textTarget);

    for(int step{0}; step <= 100; ++step) {
        palette.setColor(group, QPalette::Highlight, mix(highlight, backgroundTarget, step / 100.0));
        backgrounds = selectionBackgrounds(style, palette, group);
        if(selectionContrast(textTarget, backgrounds) < 4.5) {
            continue;
        }

        foreground = darkText ? *std::ranges::min_element(foregrounds, {}, luminance)
                              : *std::ranges::max_element(foregrounds, {}, luminance);
        if(selectionContrast(foreground, backgrounds) >= 4.5) {
            palette.setColor(group, QPalette::HighlightedText, foreground);
            return;
        }

        foreground.setAlpha(255);
        if(selectionContrast(foreground, backgrounds) >= 4.5) {
            palette.setColor(group, QPalette::HighlightedText, foreground);
            return;
        }

        double lower{0.0};
        double upper{1.0};
        for(int i{0}; i < 8; ++i) {
            const double amount = std::midpoint(lower, upper);
            if(selectionContrast(mix(foreground, textTarget, amount), backgrounds) >= 4.5) {
                upper = amount;
            }
            else {
                lower = amount;
            }
        }
        palette.setColor(group, QPalette::HighlightedText, mix(foreground, textTarget, upper));
        return;
    }

    // Retain the original accent if the style doesn't honour highlight changes
    palette.setColor(group, QPalette::Highlight, highlight);
}
} // namespace

ArtworkPaletteController::ArtworkPaletteController(CoverRepository* covers, PlayerController* player,
                                                   SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_covers{covers}
    , m_player{player}
    , m_mode{colourMode(settings->value<Settings::Gui::Internal::ArtworkColours>())}
    , m_requestId{0}
{
    QObject::connect(player, &PlayerController::currentTrackChanged, this, &ArtworkPaletteController::reloadCover);
    QObject::connect(player, &PlayerController::currentTrackUpdated, this, &ArtworkPaletteController::reloadCover);
    QObject::connect(player, &PlayerController::playStateChanged, this, &ArtworkPaletteController::reloadCover);
    QObject::connect(covers, &CoverRepository::coverAdded, this, [this](const Track& track) {
        if(track.sameIdentityAs(m_player->currentTrack())) {
            reloadCover();
        }
    });

    settings->subscribe<Settings::Gui::Internal::ArtworkColours>(this, [this](int value) {
        m_mode = colourMode(value);
        if(m_mode != ArtworkColourMode::Off && m_colour.isValid()) {
            Q_EMIT paletteChanged();
        }
        else {
            reloadCover();
        }
    });
    settings->subscribe<Settings::Gui::Internal::TrackCoverSourcePreference>(this,
                                                                             &ArtworkPaletteController::reloadCover);
    settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(this, &ArtworkPaletteController::reloadCover);

    QMetaObject::invokeMethod(this, &ArtworkPaletteController::reloadCover, Qt::QueuedConnection);
}

QPalette ArtworkPaletteController::palette(const QPalette& base) const
{
    const auto* style = QApplication::style();
    if(m_mode == ArtworkColourMode::Off || !m_colour.isValid() || !style
       || !Gui::styleSupportsCustomPalette(style->name())) {
        return base;
    }

    auto palette{base};
    const float hue = std::max(0.0F, m_colour.hslHueF());

    for(const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        if(m_mode == ArtworkColourMode::Tonal) {
            for(const auto role :
                {QPalette::Window, QPalette::Base, QPalette::AlternateBase, QPalette::Button, QPalette::ToolTipBase,
                 QPalette::Light, QPalette::Midlight, QPalette::Mid, QPalette::Dark, QPalette::Shadow}) {
                const auto original = base.color(group, role);
                palette.setColor(group, role,
                                 QColor::fromHslF(hue, std::min(0.16F, m_colour.hslSaturationF() * 0.25F),
                                                  original.lightnessF(), original.alphaF()));
            }

            if(group != QPalette::Disabled) {
                for(const auto& [foreground, background] : {
                        std::pair{QPalette::WindowText, QPalette::Window},
                        std::pair{QPalette::ButtonText, QPalette::Button},
                        std::pair{QPalette::ToolTipText, QPalette::ToolTipBase},
                    }) {
                    palette.setColor(
                        group, foreground,
                        readableColour(base.color(group, foreground), {palette.color(group, background)}, 4.5));
                }
                palette.setColor(group, QPalette::Text,
                                 readableColour(base.color(group, QPalette::Text),
                                                {palette.color(group, QPalette::Base),
                                                 palette.color(group, QPalette::AlternateBase)},
                                                4.5));
                palette.setColor(group, QPalette::PlaceholderText,
                                 readableColour(base.color(group, QPalette::PlaceholderText),
                                                {palette.color(group, QPalette::Base)}, 4.5));
            }
        }

        const auto window    = palette.color(group, QPalette::Window);
        const auto surface   = palette.color(group, QPalette::Base);
        const auto alternate = palette.color(group, QPalette::AlternateBase);

        auto highlight = readableColour(m_colour, {window, surface, alternate}, 3.0);
        if(group == QPalette::Disabled) {
            highlight = mix(highlight, window, 0.6);
            palette.setColor(group, QPalette::Highlight, highlight);
            palette.setColor(group, QPalette::HighlightedText,
                             readableColour(base.color(group, QPalette::HighlightedText), {highlight}, 4.5));
        }
        else {
            setSelectionColours(style, base, palette, group, highlight);
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        palette.setColor(group, QPalette::Accent, palette.color(group, QPalette::Highlight));
#endif
        palette.setColor(group, QPalette::Link, readableColour(m_colour, {window, surface, alternate}, 4.5));
        const auto visited = QColor::fromHslF(hue, m_colour.hslSaturationF() * 0.7F, m_colour.lightnessF());
        palette.setColor(group, QPalette::LinkVisited, readableColour(visited, {window, surface, alternate}, 4.5));
    }
    return palette;
}

void ArtworkPaletteController::reloadCover()
{
    const auto requestId = ++m_requestId;

    const auto track = m_player->currentTrack();
    if(m_mode == ArtworkColourMode::Off || m_player->playState() == Player::PlayState::Stopped || !track.isValid()) {
        setColour({});
        return;
    }

    m_covers->trackCoverFull(track).then(this, [this, requestId](const QPixmap& cover) {
        if(requestId != m_requestId) {
            return;
        }
        if(cover.isNull()) {
            setColour({});
            return;
        }

        const auto image = cover.toImage()
                               .scaled(96, 96, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                               .convertToFormat(QImage::Format_ARGB32);
        const auto key   = QCryptographicHash::hash(
            QByteArrayView{reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()},
            QCryptographicHash::Sha256);
        if(const auto it = m_colourCache.find(key); it != m_colourCache.end()) {
            setColour(it->second);
            return;
        }

        Utils::asyncExec([image]() {
            return artworkColour(image);
        }).then(this, [this, requestId, key](const QColor& colour) {
            if(requestId != m_requestId) {
                return;
            }

            if(m_colourCache.size() >= 64) {
                m_colourCache.clear();
            }
            m_colourCache.emplace(key, colour);
            setColour(colour);
        });
    });
}

void ArtworkPaletteController::setColour(const QColor& colour)
{
    if(std::exchange(m_colour, colour) != colour) {
        Q_EMIT paletteChanged();
    }
}
} // namespace Fooyin

#include "moc_artworkpalettecontroller.cpp"
