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
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
ArtworkColourMode colourMode(int value)
{
    switch(value) {
        case static_cast<int>(ArtworkColourMode::Accent):
            return ArtworkColourMode::Accent;
        case static_cast<int>(ArtworkColourMode::Tonal):
            return ArtworkColourMode::Tonal;
        case static_cast<int>(ArtworkColourMode::Adaptive):
            return ArtworkColourMode::Adaptive;
        default:
            return ArtworkColourMode::Off;
    }
}

struct OklabColour
{
    double lightness{0.0};
    double a{0.0};
    double b{0.0};
};

struct ColourCluster
{
    OklabColour colour;
    size_t count{0};
};

double linearSrgb(double channel)
{
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

// OKLAB transforms: https://bottosson.github.io/posts/oklab/
OklabColour toOklab(QRgb pixel)
{
    const double red   = linearSrgb(qRed(pixel) / 255.0);
    const double green = linearSrgb(qGreen(pixel) / 255.0);
    const double blue  = linearSrgb(qBlue(pixel) / 255.0);

    const double l = std::cbrt((0.4122214708 * red) + (0.5363325363 * green) + (0.0514459929 * blue));
    const double m = std::cbrt((0.2119034982 * red) + (0.6806995451 * green) + (0.1073969566 * blue));
    const double s = std::cbrt((0.0883024619 * red) + (0.2817188376 * green) + (0.6299787005 * blue));

    return {.lightness = (0.2104542553 * l) + (0.7936177850 * m) - (0.0040720468 * s),
            .a         = (1.9779984951 * l) - (2.4285922050 * m) + (0.4505937099 * s),
            .b         = (0.0259040371 * l) + (0.7827717662 * m) - (0.8086757660 * s)};
}

QColor fromOklab(const OklabColour& colour)
{
    const auto cube = [](double value) {
        return value * value * value;
    };

    const double l = cube(colour.lightness + (0.3963377774 * colour.a) + (0.2158037573 * colour.b));
    const double m = cube(colour.lightness - (0.1055613458 * colour.a) - (0.0638541728 * colour.b));
    const double s = cube(colour.lightness - (0.0894841775 * colour.a) - (1.2914855480 * colour.b));

    const auto toSrgb = [](double channel) {
        channel = std::clamp(channel, 0.0, 1.0);
        return channel <= 0.0031308 ? channel * 12.92 : (1.055 * std::pow(channel, 1.0 / 2.4)) - 0.055;
    };

    return QColor::fromRgbF(toSrgb((4.0767416621 * l) - (3.3077115913 * m) + (0.2309699292 * s)),
                            toSrgb((-1.2684380046 * l) + (2.6097574011 * m) - (0.3413193965 * s)),
                            toSrgb((-0.0041960863 * l) - (0.7034186147 * m) + (1.7076147010 * s)));
}

double colourDistanceSquared(const OklabColour& first, const OklabColour& second)
{
    const double lightness = first.lightness - second.lightness;
    const double a         = first.a - second.a;
    const double b         = first.b - second.b;
    return (lightness * lightness) + (a * a) + (b * b);
}

std::vector<ColourCluster> artworkPalette(const std::vector<OklabColour>& samples)
{
    if(samples.empty()) {
        return {};
    }

    static constexpr size_t MaxColours = 14;

    // A fixed seed keeps the extracted colours stable when the same artwork is loaded again
    std::mt19937 random{0};
    const auto first = std::uniform_int_distribution<size_t>{0, samples.size() - 1}(random);
    std::vector<ColourCluster> clusters{{.colour = samples[first]}};
    std::vector distances(samples.size(), std::numeric_limits<double>::max());

    while(clusters.size() < MaxColours) {
        double totalDistance{0.0};
        for(size_t i{0}; i < samples.size(); ++i) {
            distances[i] = std::min(distances[i], colourDistanceSquared(samples[i], clusters.back().colour));
            totalDistance += distances[i];
        }
        if(totalDistance <= 1e-12) {
            break;
        }

        const auto next = std::discrete_distribution<size_t>{distances.begin(), distances.end()}(random);
        clusters.push_back({.colour = samples[next]});
    }

    std::vector assignments(samples.size(), clusters.size());
    for(int iteration{0}; iteration < 64; ++iteration) {
        std::array<ColourCluster, MaxColours> sums{};
        bool changed{false};

        for(size_t i{0}; i < samples.size(); ++i) {
            const auto& sample = samples[i];

            size_t nearest{0};
            double distance = colourDistanceSquared(sample, clusters.front().colour);
            for(size_t j{1}; j < clusters.size(); ++j) {
                if(const double candidate = colourDistanceSquared(sample, clusters[j].colour); candidate < distance) {
                    nearest  = j;
                    distance = candidate;
                }
            }

            changed        = changed || assignments[i] != nearest;
            assignments[i] = nearest;

            auto& [colour, count] = sums[nearest];
            colour.lightness += sample.lightness;
            colour.a += sample.a;
            colour.b += sample.b;

            ++count;
        }

        for(size_t i{0}; i < clusters.size(); ++i) {
            const auto& [colour, count] = sums[i];
            clusters[i].count           = count;
            if(count > 0) {
                const auto c       = static_cast<double>(count);
                clusters[i].colour = {.lightness = colour.lightness / c, .a = colour.a / c, .b = colour.b / c};
            }
        }
        if(!changed) {
            break;
        }
    }

    return clusters;
}

ArtworkColours artworkColours(const QImage& image)
{
    std::vector<OklabColour> samples;
    samples.reserve(static_cast<size_t>(image.width()) * image.height());

    double lightness{0.0};
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

            lightness += (std::max({red, green, blue}) + std::min({red, green, blue})) / (2.0 * 255.0);
            samples.push_back(toOklab(pixel));
        }
    }

    QColor result;
    double bestScore{-1.0};
    for(const auto& cluster : artworkPalette(samples)) {
        if(cluster.count < std::max(size_t{1}, samples.size() / 100)) {
            continue;
        }

        const QColor colour = fromOklab(cluster.colour);
        const double chroma = std::max({colour.redF(), colour.greenF(), colour.blueF()})
                            - std::min({colour.redF(), colour.greenF(), colour.blueF()});
        const double tone   = 0.5 + (0.5 * std::sin(std::numbers::pi * colour.lightnessF()));
        const double score  = std::sqrt(static_cast<double>(cluster.count)) * (0.15 + chroma) * tone;
        if(score > bestScore) {
            bestScore = score;
            result    = colour;
        }
    }

    return {.accent    = result,
            .lightness = !samples.empty() ? static_cast<float>(lightness / static_cast<double>(samples.size())) : 0.0F};
}

double luminance(const QColor& colour)
{
    return (0.2126 * linearSrgb(colour.redF())) + (0.7152 * linearSrgb(colour.greenF()))
         + (0.0722 * linearSrgb(colour.blueF()));
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
    const auto font = QApplication::font();
    const QFontMetrics fontMetrics{font};
    QImage image{std::max(96, fontMetrics.horizontalAdvance(u"Selection"_s) + 16),
                 std::max(32, fontMetrics.height() + 8), QImage::Format_RGB32};

    QStyleOptionViewItem opt;
    opt.rect                   = image.rect();
    opt.palette                = palette;
    opt.font                   = font;
    opt.fontMetrics            = fontMetrics;
    opt.text                   = u"Selection"_s;
    opt.features               = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment       = Qt::AlignLeft | Qt::AlignVCenter;
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

        auto textRect    = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt) + 1;
        textRect.adjust(margin, 0, -margin, 0);
        const auto sampleRect
            = QStyle::alignedRect(opt.direction, opt.displayAlignment,
                                  {textRect.width(), std::min(fontMetrics.height(), textRect.height())}, textRect)
                  .intersected(image.rect());
        image.fill(palette.color(group, QPalette::Base));

        {
            QPainter painter{&image};
            style->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &painter);
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, &painter);
        }

        for(int y{sampleRect.top()}; y <= sampleRect.bottom(); ++y) {
            const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x{sampleRect.left()}; x <= sampleRect.right(); ++x) {
                if(x == sampleRect.left() || pixels[x] != pixels[x - 1]) {
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

    const QColor black{Qt::black};
    const QColor white{Qt::white};

    const double blackContrast = selectionContrast(black, backgrounds);
    const double whiteContrast = selectionContrast(white, backgrounds);

    static constexpr auto DarkTextPreferenceContrast = 4.0;

    const bool darkText          = blackContrast >= DarkTextPreferenceContrast || blackContrast >= whiteContrast;
    const auto& textTarget       = darkText ? black : white;
    const auto& backgroundTarget = darkText ? white : black;

    // Keep selected text at full strength when the view loses focus
    const auto fgGroup = group == QPalette::Inactive ? QPalette::Active : group;
    const std::array foregrounds{base.color(fgGroup, QPalette::HighlightedText), base.color(fgGroup, QPalette::Text),
                                 base.color(fgGroup, QPalette::WindowText), base.color(fgGroup, QPalette::ButtonText)};
    const double highlightLuminance = luminance(highlight);

    const auto themeForeground = [&](const std::vector<QColor>& samples) {
        auto foreground = textTarget;
        double bestContrast{0.0};
        for(const auto& candidate : foregrounds) {
            if((luminance(candidate) < highlightLuminance) != darkText) {
                continue;
            }
            if(const double contrast = selectionContrast(candidate, samples); contrast > bestContrast) {
                foreground   = candidate;
                bestContrast = contrast;
            }
        }
        return foreground;
    };

    static constexpr auto MinContrast = 4.5;

    auto foreground = themeForeground(backgrounds);
    if(selectionContrast(foreground, backgrounds) >= MinContrast) {
        palette.setColor(group, QPalette::HighlightedText, foreground);
        return;
    }

    palette.setColor(group, QPalette::HighlightedText, textTarget);

    for(int step{0}; step <= 100; ++step) {
        palette.setColor(group, QPalette::Highlight, mix(highlight, backgroundTarget, step / 100.0));
        backgrounds = selectionBackgrounds(style, palette, group);
        if(selectionContrast(textTarget, backgrounds) < MinContrast) {
            continue;
        }

        foreground = themeForeground(backgrounds);
        if(selectionContrast(foreground, backgrounds) >= MinContrast) {
            palette.setColor(group, QPalette::HighlightedText, foreground);
            return;
        }

        foreground.setAlpha(255);
        if(selectionContrast(foreground, backgrounds) >= MinContrast) {
            palette.setColor(group, QPalette::HighlightedText, foreground);
            return;
        }

        double lower{0.0};
        double upper{1.0};
        for(int i{0}; i < 8; ++i) {
            const double amount = std::midpoint(lower, upper);
            if(selectionContrast(mix(foreground, textTarget, amount), backgrounds) >= MinContrast) {
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
        if(m_mode != ArtworkColourMode::Off && m_colours.accent.isValid()) {
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
    if(m_mode == ArtworkColourMode::Off || !m_colours.accent.isValid() || !style
       || !Gui::styleSupportsCustomPalette(style->name())) {
        return base;
    }

    auto palette{base};
    const auto& colour          = m_colours.accent;
    const float hue             = std::max(0.0F, colour.hslHueF());
    const bool adaptive         = m_mode == ArtworkColourMode::Adaptive;
    const float targetLightness = m_colours.lightness < 0.5F
                                    ? std::lerp(0.16F, 0.32F, m_colours.lightness * 2.0F)
                                    : std::lerp(0.72F, 0.90F, (m_colours.lightness - 0.5F) * 2.0F);

    for(const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        if(m_mode == ArtworkColourMode::Tonal || adaptive) {
            for(const auto role :
                {QPalette::Window, QPalette::Base, QPalette::AlternateBase, QPalette::Button, QPalette::ToolTipBase,
                 QPalette::Light, QPalette::Midlight, QPalette::Mid, QPalette::Dark, QPalette::Shadow}) {
                const auto original = base.color(group, role);
                float lightness{original.lightnessF()};
                if(adaptive) {
                    const float window = base.color(group, QPalette::Window).lightnessF();
                    float offset       = std::clamp(lightness - window, -0.08F, 0.08F);
                    if((role == QPalette::Base || role == QPalette::AlternateBase)
                       && (window < 0.5F) != (targetLightness < 0.5F)) {
                        offset = -offset;
                    }
                    lightness = std::clamp(targetLightness + offset, 0.06F, 0.98F);
                }
                palette.setColor(group, role,
                                 QColor::fromHslF(hue, std::min(0.16F, colour.hslSaturationF() * 0.25F), lightness,
                                                  original.alphaF()));
            }

            if(adaptive) {
                for(const auto role : {QPalette::WindowText, QPalette::ButtonText, QPalette::ToolTipText,
                                       QPalette::Text, QPalette::PlaceholderText, QPalette::HighlightedText}) {
                    auto foreground = base.color(group, role);
                    foreground.setAlpha(255);
                    palette.setColor(group, role, foreground);
                }
            }

            if(group != QPalette::Disabled || adaptive) {
                const double contrast = group == QPalette::Disabled ? 3.0 : 4.5;
                for(const auto& [foreground, background] : {
                        std::pair{QPalette::WindowText, QPalette::Window},
                        std::pair{QPalette::ButtonText, QPalette::Button},
                        std::pair{QPalette::ToolTipText, QPalette::ToolTipBase},
                    }) {
                    palette.setColor(
                        group, foreground,
                        readableColour(palette.color(group, foreground), {palette.color(group, background)}, contrast));
                }
                palette.setColor(group, QPalette::Text,
                                 readableColour(palette.color(group, QPalette::Text),
                                                {palette.color(group, QPalette::Base),
                                                 palette.color(group, QPalette::AlternateBase)},
                                                contrast));
                palette.setColor(group, QPalette::PlaceholderText,
                                 readableColour(palette.color(group, QPalette::PlaceholderText),
                                                {palette.color(group, QPalette::Base)}, contrast));
            }
        }

        const auto window    = palette.color(group, QPalette::Window);
        const auto surface   = palette.color(group, QPalette::Base);
        const auto alternate = palette.color(group, QPalette::AlternateBase);

        auto highlight = readableColour(colour, {window, surface, alternate}, 3.0);
        if(group == QPalette::Disabled) {
            highlight = mix(highlight, window, 0.6);
            palette.setColor(group, QPalette::Highlight, highlight);
            palette.setColor(group, QPalette::HighlightedText,
                             readableColour(palette.color(group, QPalette::HighlightedText), {highlight}, 4.5));
        }
        else {
            setSelectionColours(style, base, palette, group, highlight);
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        palette.setColor(group, QPalette::Accent, palette.color(group, QPalette::Highlight));
#endif
        palette.setColor(group, QPalette::Link, readableColour(colour, {window, surface, alternate}, 4.5));
        const auto visited = QColor::fromHslF(hue, colour.hslSaturationF() * 0.7F, colour.lightnessF());
        palette.setColor(group, QPalette::LinkVisited, readableColour(visited, {window, surface, alternate}, 4.5));
    }
    return palette;
}

void ArtworkPaletteController::reloadCover()
{
    const auto requestId = ++m_requestId;

    const auto track = m_player->currentTrack();
    if(m_mode == ArtworkColourMode::Off || m_player->playState() == Player::PlayState::Stopped || !track.isValid()) {
        setColours({});
        return;
    }

    m_covers->trackCoverFull(track).then(this, [this, requestId](const QPixmap& cover) {
        if(requestId != m_requestId) {
            return;
        }
        if(cover.isNull()) {
            setColours({});
            return;
        }

        const auto image = cover.toImage()
                               .scaled(96, 96, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                               .convertToFormat(QImage::Format_ARGB32);
        const auto key   = QCryptographicHash::hash(
            QByteArrayView{reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()},
            QCryptographicHash::Sha256);
        if(const auto it = m_colourCache.find(key); it != m_colourCache.end()) {
            setColours(it->second);
            return;
        }

        Utils::asyncExec([image]() {
            return artworkColours(image);
        }).then(this, [this, requestId, key](const ArtworkColours& colours) {
            if(requestId != m_requestId) {
                return;
            }

            if(m_colourCache.size() >= 64) {
                m_colourCache.clear();
            }
            m_colourCache.emplace(key, colours);
            setColours(colours);
        });
    });
}

void ArtworkPaletteController::setColours(const ArtworkColours& colours)
{
    if(std::exchange(m_colours, colours) != colours) {
        Q_EMIT paletteChanged();
    }
}
} // namespace Fooyin

#include "moc_artworkpalettecontroller.cpp"
