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

#include "spectrumaxisrenderer.h"

#include "spectrumcolours.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto LabelPadding                   = 6;
constexpr auto GridLineCount                  = 6;
constexpr auto PlotHeadroomPx                 = 12;
constexpr auto A4NoteIndex                    = 57;
constexpr auto HighestHorizontalLabelPriority = 2;

namespace Fooyin::Spectrum {
namespace {
QString noteNameForSemitone(int semitone)
{
    static constexpr std::array<const char*, 12> NoteNames
        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QString::fromLatin1(NoteNames[std::clamp(semitone, 0, 11)]);
}

bool isFullStep(int semitone)
{
    switch(semitone) {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
        case 9:
        case 11:
            return true;
        default:
            return false;
    }
}

bool isBlackKey(int semitone)
{
    return !isFullStep(semitone);
}

int noteEdgeX(const QRect& plotRect, int noteIndex, int noteCount)
{
    noteCount = std::max(1, noteCount);
    return plotRect.left()
         + static_cast<int>(std::round(static_cast<double>(noteIndex) * static_cast<double>(plotRect.width())
                                       / static_cast<double>(noteCount)));
}

QRect noteCellRect(const QRect& rect, int noteIndex, int noteCount)
{
    const int left  = noteEdgeX(rect, noteIndex, noteCount);
    const int right = noteEdgeX(rect, noteIndex + 1, noteCount);
    return {left, rect.top(), std::max(1, right - left), rect.height()};
}

struct DevicePlotRect
{
    qreal dpr{1.0};
    int left{0};
    int width{0};
    int top{0};
    int height{0};
};

int logicalToDevicePixel(qreal value, qreal dpr)
{
    return static_cast<int>(std::round(value * dpr));
}

qreal devicePixelToLogical(int value, qreal dpr)
{
    return static_cast<qreal>(value) / dpr;
}

DevicePlotRect toDevicePlotRect(const QRect& plotRect, qreal dpr)
{
    const int left   = logicalToDevicePixel(plotRect.left(), dpr);
    const int right  = logicalToDevicePixel(plotRect.left() + plotRect.width(), dpr);
    const int top    = logicalToDevicePixel(plotRect.top(), dpr);
    const int bottom = logicalToDevicePixel(plotRect.top() + plotRect.height(), dpr);

    const int width  = std::max(1, right - left);
    const int height = std::max(1, bottom - top);

    return {.dpr = dpr, .left = left, .width = width, .top = top, .height = height};
}

QRectF logicalBarRect(const DevicePlotRect& plot, int left, int width)
{
    return {devicePixelToLogical(left, plot.dpr), devicePixelToLogical(plot.top, plot.dpr),
            devicePixelToLogical(width, plot.dpr), devicePixelToLogical(plot.height, plot.dpr)};
}

int equalSectionPlotHeight(int height, int sectionCount, int spacing)
{
    sectionCount = std::max(1, sectionCount);
    spacing      = std::max(0, spacing);

    while(sectionCount > 1 && (height - (spacing * (sectionCount - 1))) < sectionCount) {
        --sectionCount;
    }

    if(sectionCount <= 1) {
        return height;
    }

    const int sectionHeight = std::max(1, (height - (spacing * (sectionCount - 1))) / sectionCount);
    return (sectionHeight * sectionCount) + (spacing * (sectionCount - 1));
}

bool horizontalLabelsFit(const QFontMetrics& metrics, const QRect& spectrumRect,
                         std::span<const SpectrumHorizontalLabel> labels, int stride, qreal dpr)
{
    qreal previousRight{static_cast<qreal>(spectrumRect.left()) - 1.0};

    for(int index{0}; std::cmp_less(index, labels.size()); index += stride) {
        const auto& label = labels[static_cast<size_t>(index)];
        const qreal width = static_cast<qreal>(metrics.horizontalAdvance(label.text)) / dpr;
        const qreal left
            = std::clamp(static_cast<qreal>(label.centerX) - (width / 2.0), static_cast<qreal>(spectrumRect.left()),
                         static_cast<qreal>(spectrumRect.right()) - width + 1.0);
        const qreal right = left + width - 1.0;
        const qreal requiredGap
            = previousRight < static_cast<qreal>(spectrumRect.left()) ? 0.0 : static_cast<qreal>(LabelPadding) / dpr;

        if(right > static_cast<qreal>(spectrumRect.right()) || left <= previousRight + requiredGap) {
            return false;
        }

        previousRight = right;
    }

    return true;
}
} // namespace

struct SpectrumAxisRenderer::AmplitudeLabel
{
    QString text;
    int baseline{0};
};

void SpectrumAxisRenderer::setConfig(const SpectrumWidget::ConfigData& config)
{
    m_config = config;
}

SpectrumPlotGeometry SpectrumAxisRenderer::layout(const QRect& widgetRect, const QFont& axisFont, int bandCount,
                                                  qreal dpr) const
{
    SpectrumPlotGeometry geometry;
    bandCount = std::max(1, bandCount);
    if(widgetRect.width() <= 0 || widgetRect.height() <= 0 || bandCount <= 0) {
        return geometry;
    }

    const QFontMetrics metrics{axisFont};
    const int labelHeight = metrics.height() + LabelPadding;
    const int dbWidth     = metrics.horizontalAdvance(u"-120 dB"_s) + LabelPadding;

    const bool requestedAmplitudeLabels  = m_config.showLeftLabels || m_config.showRightLabels;
    const bool requestedHorizontalLabels = m_config.showTopLabels || m_config.showBottomLabels;
    geometry.drawAmplitudeLabels         = requestedAmplitudeLabels;
    bool drawHorizontalLabels            = requestedHorizontalLabels;

    for(int pass{0}; pass < 3; ++pass) {
        QRect fitRect = widgetRect;
        fitRect.adjust(m_config.showLeftLabels && geometry.drawAmplitudeLabels ? dbWidth : 0,
                       m_config.showTopLabels && drawHorizontalLabels ? labelHeight : 0,
                       m_config.showRightLabels && geometry.drawAmplitudeLabels ? -dbWidth : 0,
                       m_config.showBottomLabels && drawHorizontalLabels ? -labelHeight : 0);
        const QRect fitPlotRect = plotRectForSpectrumRect(fitRect);

        const bool drawAmplitudeLabels
            = requestedAmplitudeLabels && !amplitudeLabels(metrics, widgetRect.height(), fitPlotRect).empty();
        const bool nextDrawHorizontalLabels
            = requestedHorizontalLabels
           && !horizontalLabels(metrics, fitRect, fitPlotRect, bandCount, HighestHorizontalLabelPriority, dpr).empty();

        if(drawAmplitudeLabels == geometry.drawAmplitudeLabels && nextDrawHorizontalLabels == drawHorizontalLabels) {
            break;
        }

        geometry.drawAmplitudeLabels = drawAmplitudeLabels;
        drawHorizontalLabels         = nextDrawHorizontalLabels;
    }

    geometry.spectrumRect = widgetRect;
    geometry.spectrumRect.adjust(m_config.showLeftLabels && geometry.drawAmplitudeLabels ? dbWidth : 0,
                                 m_config.showTopLabels && drawHorizontalLabels ? labelHeight : 0,
                                 m_config.showRightLabels && geometry.drawAmplitudeLabels ? -dbWidth : 0,
                                 m_config.showBottomLabels && drawHorizontalLabels ? -labelHeight : 0);
    geometry.plotRect = plotRectForSpectrumRect(geometry.spectrumRect);

    if(geometry.plotRect.width() <= 0 || geometry.plotRect.height() <= 0) {
        geometry.drawAmplitudeLabels = false;
        return geometry;
    }

    SpectrumPlotGeometry barGeometry = barGeometryForPlotRect(geometry.plotRect, bandCount, dpr);
    geometry.barRects                = std::move(barGeometry.barRects);
    geometry.sourceBands             = std::move(barGeometry.sourceBands);
    geometry.horizontalLabels = drawHorizontalLabels
                                  ? horizontalLabels(metrics, geometry.spectrumRect, geometry.plotRect, bandCount,
                                                     HighestHorizontalLabelPriority, dpr)
                                  : std::vector<SpectrumHorizontalLabel>{};
    return geometry;
}

void SpectrumAxisRenderer::paint(QPainter& painter, const SpectrumPlotGeometry& geometry, const QSize& size,
                                 const QPalette& palette, const QFont& axisFont) const
{
    const Colours customColours = m_config.colours.isValid() && m_config.colours.canConvert<Colours>()
                                    ? m_config.colours.value<Colours>()
                                    : Colours{};
    painter.fillRect(QRect{QPoint{0, 0}, size}, customColours.colour(Colours::Type::Background, palette));

    if(geometry.spectrumRect.isEmpty() || geometry.plotRect.isEmpty()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setFont(axisFont);

    const QColor textColour         = customColours.colour(Colours::Type::Text, palette);
    const QColor gridColour         = customColours.colour(Colours::Type::HorizontalGrid, palette);
    const QColor verticalGridColour = m_config.labelMode == LabelMode::Notes
                                        ? customColours.colour(Colours::Type::OctaveGrid, palette)
                                        : customColours.colour(Colours::Type::VerticalGrid, palette);

    drawKeyBackgrounds(painter, geometry, palette);

    if(m_config.showHorizontalGrid) {
        painter.setPen(gridColour);
        for(int index{0}; index <= GridLineCount; ++index) {
            const int y
                = geometry.plotRect.top() + ((geometry.plotRect.height() - 1) * index / std::max(1, GridLineCount));
            painter.drawLine(geometry.plotRect.left(), y, geometry.plotRect.right(), y);
        }
    }

    if(m_config.showVerticalGrid) {
        painter.setPen(verticalGridColour);
        if(m_config.labelMode == LabelMode::Notes) {
            for(int note{m_config.minNote}; note <= m_config.maxNote; ++note) {
                const int semitone = ((note % 12) + 12) % 12;
                if(note != m_config.minNote && semitone != 0) {
                    continue;
                }

                const int x = noteCenterX(note, geometry.plotRect);
                painter.drawLine(x, geometry.plotRect.top(), x, geometry.plotRect.bottom());
            }
        }
        else if(!geometry.horizontalLabels.empty()) {
            for(const auto& label : geometry.horizontalLabels) {
                painter.drawLine(label.centerX, geometry.plotRect.top(), label.centerX, geometry.plotRect.bottom());
            }
        }
        else {
            const auto frequencies = verticalGridFrequencies();
            for(const auto& frequency : frequencies) {
                const int x = frequencyToX(frequency, geometry.plotRect);
                painter.drawLine(x, geometry.plotRect.top(), x, geometry.plotRect.bottom());
            }
        }
    }

    const QFontMetrics metrics{painter.font()};
    painter.setPen(textColour);

    if(!geometry.horizontalLabels.empty()) {
        for(const auto& label : geometry.horizontalLabels) {
            const int textWidth = metrics.horizontalAdvance(label.text);
            const int x         = std::clamp(label.centerX - (textWidth / 2), geometry.spectrumRect.left(),
                                             geometry.spectrumRect.right() - textWidth + 1);

            if(m_config.showTopLabels) {
                painter.drawText(x, std::max(metrics.ascent(), geometry.spectrumRect.top() - (LabelPadding / 2)),
                                 label.text);
            }
            if(m_config.showBottomLabels) {
                painter.drawText(x, geometry.plotRect.bottom() + LabelPadding + metrics.ascent(), label.text);
            }
        }
    }

    if(geometry.drawAmplitudeLabels) {
        for(const auto& label : amplitudeLabels(metrics, size.height(), geometry.plotRect)) {
            if(m_config.showLeftLabels) {
                painter.drawText(0, label.baseline, label.text);
            }
            if(m_config.showRightLabels) {
                painter.drawText(geometry.spectrumRect.right() + LabelPadding, label.baseline, label.text);
            }
        }
    }
}

QFont SpectrumAxisRenderer::defaultAxisFont(const QFont& baseFont)
{
    QFont font{baseFont};
    if(font.pointSize() > 0) {
        font.setPointSize(std::max(7, font.pointSize() - 3));
    }
    else if(font.pixelSize() > 0) {
        font.setPixelSize(std::max(7, font.pixelSize() - 3));
    }
    return font;
}

float SpectrumAxisRenderer::effectiveMinFrequencyHz() const
{
    if(m_config.labelMode == LabelMode::Notes) {
        return frequencyForNote(m_config.minNote, m_config.pitchHz, m_config.transpose);
    }
    return static_cast<float>(m_config.minFrequencyHz);
}

float SpectrumAxisRenderer::effectiveMaxFrequencyHz() const
{
    if(m_config.labelMode == LabelMode::Notes) {
        return frequencyForNote(m_config.maxNote, m_config.pitchHz, m_config.transpose);
    }
    return static_cast<float>(m_config.maxFrequencyHz);
}

float SpectrumAxisRenderer::frequencyForBand(int band, int bandCount) const
{
    bandCount = std::max(1, bandCount);
    if(m_config.labelMode == LabelMode::Notes) {
        const double noteCount = std::max(1, m_config.maxNote - m_config.minNote + 1);
        const double note = bandCount == static_cast<int>(noteCount)
                              ? static_cast<double>(m_config.minNote + band)
                              : static_cast<double>(m_config.minNote)
                                    + ((static_cast<double>(band) + 0.5) * noteCount / static_cast<double>(bandCount));
        return frequencyForNote(static_cast<int>(std::round(note)), m_config.pitchHz, m_config.transpose);
    }

    const float minFreq = effectiveMinFrequencyHz();
    const float maxFreq = std::max(minFreq + 1.0F, effectiveMaxFrequencyHz());
    const double t      = bandCount > 1 ? static_cast<double>(band) / static_cast<double>(bandCount - 1) : 0.0;
    return minFreq * std::pow(maxFreq / minFreq, static_cast<float>(t));
}

int SpectrumAxisRenderer::noteForBand(int band, int bandCount) const
{
    bandCount              = std::max(1, bandCount);
    const double noteCount = std::max(1, m_config.maxNote - m_config.minNote + 1);
    const double note      = bandCount == static_cast<int>(noteCount)
                               ? static_cast<double>(m_config.minNote + band)
                               : static_cast<double>(m_config.minNote)
                                     + ((static_cast<double>(band) + 0.5) * noteCount / static_cast<double>(bandCount));
    return static_cast<int>(std::round(note));
}

QString SpectrumAxisRenderer::formatFrequencyLabel(float frequencyHz)
{
    if(frequencyHz >= 1000.0F) {
        const float khz = frequencyHz / 1000.0F;
        return khz >= 10.0F ? QString::number(std::round(khz)) + u"k"_s : QString::number(khz, 'f', 1) + u"k"_s;
    }

    return QString::number(std::round(frequencyHz));
}

QString SpectrumAxisRenderer::formatTooltipFrequency(float frequencyHz)
{
    return QString::number(std::round(frequencyHz)) + u" Hz"_s;
}

QString SpectrumAxisRenderer::formatNoteLabel(int noteIndex)
{
    const int octave   = noteIndex / 12;
    const int semitone = ((noteIndex % 12) + 12) % 12;
    return noteNameForSemitone(semitone) + QString::number(octave);
}

float SpectrumAxisRenderer::frequencyForNote(int noteIndex, int pitchHz, int transpose)
{
    return static_cast<float>(pitchHz)
         * std::pow(2.0F, (static_cast<float>(noteIndex) - static_cast<float>(A4NoteIndex + transpose)) / 12.0F);
}

int SpectrumAxisRenderer::noteCenterX(int note, const QRect& plotRect) const
{
    const int startNote  = m_config.minNote;
    const int endNote    = m_config.maxNote;
    const int noteCount  = std::max(1, endNote - startNote + 1);
    const QRect cellRect = noteCellRect(plotRect, note - startNote, noteCount);
    return std::clamp(cellRect.center().x(), plotRect.left(), plotRect.right());
}

int SpectrumAxisRenderer::frequencyToX(float frequencyHz, const QRect& plotRect) const
{
    if(plotRect.width() <= 0) {
        return 0;
    }

    const float minFreq  = effectiveMinFrequencyHz();
    const float maxFreq  = std::max(minFreq + 1.0F, effectiveMaxFrequencyHz());
    const float clamped  = std::clamp(frequencyHz, minFreq, maxFreq);
    const float position = (std::log(clamped) - std::log(minFreq)) / (std::log(maxFreq) - std::log(minFreq));
    return plotRect.left()
         + static_cast<int>(std::round(position * static_cast<float>(std::max(0, plotRect.width() - 1))));
}

std::vector<float> SpectrumAxisRenderer::verticalGridFrequencies() const
{
    const float minFreq = effectiveMinFrequencyHz();
    const float maxFreq = std::max(minFreq + 1.0F, effectiveMaxFrequencyHz());

    static constexpr std::array FrequencyMarkers
        = {20.0F, 31.25F, 62.5F, 125.0F, 250.0F, 500.0F, 1000.0F, 2000.0F, 4000.0F, 8000.0F, 16000.0F, 20000.0F};
    std::vector<float> markers;
    markers.reserve(FrequencyMarkers.size());
    for(const float frequencyHz : FrequencyMarkers) {
        if(frequencyHz >= minFreq && frequencyHz <= maxFreq) {
            markers.emplace_back(frequencyHz);
        }
    }

    return markers;
}

QRect SpectrumAxisRenderer::plotRectForSpectrumRect(const QRect& spectrumRect) const
{
    QRect plotRect = spectrumRect.adjusted(0, PlotHeadroomPx, 0, 0);
    if(plotRect.width() <= 0 || plotRect.height() <= 0) {
        return plotRect;
    }

    const int plotHeight
        = equalSectionPlotHeight(plotRect.height(), std::clamp(m_config.barSections, MinBarSections, MaxBarSections),
                                 std::clamp(m_config.sectionSpacing, MinSectionSpacing, MaxSectionSpacing));
    if(plotHeight < plotRect.height()) {
        plotRect.setTop(plotRect.bottom() - plotHeight + 1);
    }

    return plotRect;
}

SpectrumPlotGeometry SpectrumAxisRenderer::barGeometryForPlotRect(const QRect& plotRect, int bandCount, qreal dpr) const
{
    SpectrumPlotGeometry geometry;
    if(plotRect.width() <= 0 || plotRect.height() <= 0 || bandCount <= 0) {
        return geometry;
    }

    if(m_config.labelMode == LabelMode::Notes) {
        const int visibleBarCount = std::max(1, bandCount);
        const DevicePlotRect plot = toDevicePlotRect(plotRect, dpr);
        const double noteWidth    = static_cast<double>(plot.width) / static_cast<double>(visibleBarCount);
        const int requestedGap
            = logicalToDevicePixel(std::clamp(m_config.barSpacing, MinBarSpacing, MaxBarSpacing), plot.dpr);
        const int gap = (noteWidth <= 1.0) ? 0 : std::min(requestedGap, static_cast<int>(std::floor(noteWidth - 1.0)));
        geometry.barRects.reserve(visibleBarCount);
        geometry.sourceBands.reserve(visibleBarCount);

        for(int index{0}; index < visibleBarCount; ++index) {
            const int cellLeft
                = plot.left
                + static_cast<int>(std::round(static_cast<double>(index) * static_cast<double>(plot.width)
                                              / static_cast<double>(visibleBarCount)));
            const int cellRight
                = plot.left
                + static_cast<int>(std::round(static_cast<double>(index + 1) * static_cast<double>(plot.width)
                                              / static_cast<double>(visibleBarCount)));
            const int cellWidth = std::max(1, cellRight - cellLeft);
            const int cellGap   = std::min(gap, std::max(0, cellWidth - 1));
            const int leftGap   = (cellGap + 1) / 2;
            const int rightGap  = cellGap - leftGap;
            const int x         = cellLeft + leftGap;
            const int width     = std::max(1, cellWidth - leftGap - rightGap);

            geometry.barRects.emplace_back(logicalBarRect(plot, x, width));
            geometry.sourceBands.emplace_back(index);
        }

        return geometry;
    }

    const DevicePlotRect plot   = toDevicePlotRect(plotRect, dpr);
    const int requestedBarCount = std::clamp(bandCount, 1, std::max(1, plot.width));
    int gap = logicalToDevicePixel(std::clamp(m_config.barSpacing, MinBarSpacing, MaxBarSpacing), plot.dpr);
    if((requestedBarCount + (gap * std::max(0, requestedBarCount - 1))) > plot.width) {
        gap = 0;
    }

    const auto desiredCellWidth = static_cast<double>(plot.width) / static_cast<double>(requestedBarCount);
    const int cellWidth         = std::max(gap + 1, static_cast<int>(std::round(desiredCellWidth)));
    const int visibleBarCount   = std::clamp(plot.width / cellWidth, 1, std::max(1, plot.width));
    const int barWidth          = std::max(1, cellWidth - gap);
    const int usedWidth         = ((visibleBarCount - 1) * cellWidth) + barWidth;
    const int left              = plot.left + (std::max(0, plot.width - usedWidth) / 2);
    geometry.barRects.reserve(visibleBarCount);
    geometry.sourceBands.reserve(visibleBarCount);
    if(visibleBarCount == 1) {
        geometry.barRects.emplace_back(plotRect);
        geometry.sourceBands.emplace_back(0);
        return geometry;
    }

    for(int index{0}; index < visibleBarCount; ++index) {
        const int x = left + (index * (barWidth + gap));
        geometry.barRects.emplace_back(logicalBarRect(plot, x, barWidth));
        const double sourcePosition = static_cast<double>(index) * static_cast<double>(bandCount - 1)
                                    / static_cast<double>(visibleBarCount - 1);
        geometry.sourceBands.emplace_back(std::clamp(static_cast<int>(std::round(sourcePosition)), 0, bandCount - 1));
    }

    return geometry;
}

std::vector<SpectrumHorizontalLabel>
SpectrumAxisRenderer::horizontalLabelCandidates(const SpectrumPlotGeometry& geometry, const QRect& plotRect,
                                                int bandCount, int maxPriority) const
{
    std::vector<SpectrumHorizontalLabel> labels;

    if(m_config.labelMode == LabelMode::Notes) {
        const int startNote = m_config.minNote;
        const int endNote   = m_config.maxNote;
        const int noteCount = std::max(1, endNote - startNote + 1);
        labels.reserve(static_cast<size_t>(noteCount));

        for(int note = startNote; note <= endNote; ++note) {
            const int semitone = ((note % 12) + 12) % 12;
            QString text;
            int priority{0};
            if(note == m_config.minNote || semitone == 0) {
                text = formatNoteLabel(note);
            }
            else if(isFullStep(semitone)) {
                text     = noteNameForSemitone(semitone);
                priority = 1;
            }
            else {
                text     = noteNameForSemitone(semitone);
                priority = 2;
            }
            if(!text.isEmpty() && priority <= maxPriority) {
                labels.push_back({.text = text, .centerX = noteCenterX(note, plotRect)});
            }
        }

        return labels;
    }

    const auto count = static_cast<int>(std::min(geometry.barRects.size(), geometry.sourceBands.size()));
    labels.reserve(static_cast<size_t>(count));

    for(int index{0}; index < count; ++index) {
        const int band     = geometry.sourceBands[index];
        const QString text = formatFrequencyLabel(frequencyForBand(band, bandCount));

        if(!text.isEmpty()) {
            labels.push_back(
                {.text = text, .centerX = static_cast<int>(std::round(geometry.barRects[index].center().x()))});
        }
    }

    return labels;
}

std::vector<SpectrumHorizontalLabel> SpectrumAxisRenderer::horizontalLabels(const QFontMetrics& metrics,
                                                                            const QRect& spectrumRect,
                                                                            const QRect& plotRect, int bandCount,
                                                                            int maxPriority, qreal dpr) const
{
    const SpectrumPlotGeometry geometry = barGeometryForPlotRect(plotRect, bandCount, dpr);
    if(geometry.barRects.empty() || geometry.sourceBands.empty()) {
        return {};
    }

    if(m_config.labelMode == LabelMode::Notes) {
        for(int priority = maxPriority; priority >= 0; --priority) {
            const std::vector<SpectrumHorizontalLabel> candidates
                = horizontalLabelCandidates(geometry, plotRect, bandCount, priority);
            if(!candidates.empty() && horizontalLabelsFit(metrics, spectrumRect, candidates, 1, dpr)) {
                return candidates;
            }
        }

        const std::vector<SpectrumHorizontalLabel> octaveCandidates
            = horizontalLabelCandidates(geometry, plotRect, bandCount, 0);
        for(int stride{2}; std::cmp_less_equal(stride, octaveCandidates.size()); ++stride) {
            if(!horizontalLabelsFit(metrics, spectrumRect, octaveCandidates, stride, dpr)) {
                continue;
            }

            std::vector<SpectrumHorizontalLabel> labels;
            labels.reserve((octaveCandidates.size() + static_cast<size_t>(stride) - 1) / static_cast<size_t>(stride));
            for(int index{0}; std::cmp_less(index, octaveCandidates.size()); index += stride) {
                labels.push_back(octaveCandidates[static_cast<size_t>(index)]);
            }
            return labels;
        }

        return {};
    }

    while(maxPriority >= 0) {
        const std::vector<SpectrumHorizontalLabel> candidates
            = horizontalLabelCandidates(geometry, plotRect, bandCount, maxPriority);
        if(candidates.empty()) {
            --maxPriority;
            continue;
        }

        for(int stride{1}; std::cmp_less_equal(stride, candidates.size()); ++stride) {
            for(int offset{0}; offset < stride; ++offset) {
                const std::span offsetCandidates{candidates.data() + offset,
                                                 candidates.size() - static_cast<size_t>(offset)};
                if(!horizontalLabelsFit(metrics, spectrumRect, offsetCandidates, stride, 1.0)) {
                    continue;
                }

                std::vector<SpectrumHorizontalLabel> labels;
                labels.reserve((offsetCandidates.size() + static_cast<size_t>(stride) - 1)
                               / static_cast<size_t>(stride));
                for(int index{0}; std::cmp_less(index, offsetCandidates.size()); index += stride) {
                    labels.push_back(offsetCandidates[static_cast<size_t>(index)]);
                }
                return labels;
            }
        }

        --maxPriority;
    }

    return {};
}

std::vector<SpectrumAxisRenderer::AmplitudeLabel>
SpectrumAxisRenderer::amplitudeLabels(const QFontMetrics& metrics, int height, const QRect& plotRect) const
{
    if(height <= 0 || plotRect.isEmpty()) {
        return {};
    }

    std::vector<AmplitudeLabel> labels;
    labels.reserve(GridLineCount + 1);

    int previousBottom{-1};

    const int minBaseline = metrics.ascent();
    const int maxBaseline = height - metrics.descent();
    if(maxBaseline < minBaseline) {
        return {};
    }

    for(int index{0}; index <= GridLineCount; ++index) {
        const float t     = static_cast<float>(index) / static_cast<float>(std::max(1, GridLineCount));
        const int dbValue = static_cast<int>(
            std::round(static_cast<float>(m_config.amplitudeMaxDb)
                       - (static_cast<float>(m_config.amplitudeMaxDb - m_config.amplitudeMinDb) * t)));
        const int gridY    = plotRect.top() + ((plotRect.height() - 1) * index / std::max(1, GridLineCount));
        const int baseline = std::clamp(gridY + ((metrics.ascent() - metrics.descent()) / 2), minBaseline, maxBaseline);
        const int top      = baseline - metrics.ascent();
        const int bottom   = baseline + metrics.descent();
        if(top <= previousBottom || top < 0 || bottom > height) {
            return {};
        }

        labels.push_back({.text = QString::number(dbValue) + u" dB"_s, .baseline = baseline});
        previousBottom = bottom;
    }

    return labels;
}

void SpectrumAxisRenderer::drawKeyBackgrounds(QPainter& painter, const SpectrumPlotGeometry& geometry,
                                              const QPalette& palette) const
{
    if(m_config.labelMode != LabelMode::Notes || geometry.plotRect.isEmpty()) {
        return;
    }

    const bool showWhiteKeys = m_config.showWhiteKeys;
    const bool showBlackKeys = m_config.showBlackKeys;
    if(!showWhiteKeys && !showBlackKeys) {
        return;
    }

    const Colours customColours  = m_config.colours.isValid() && m_config.colours.canConvert<Colours>()
                                     ? m_config.colours.value<Colours>()
                                     : Colours{};
    const QColor whiteKeyColour  = customColours.colour(Colours::Type::WhiteKey, palette);
    const QColor blackKeyColour  = customColours.colour(Colours::Type::BlackKey, palette);
    const QColor separatorColour = customColours.colour(Colours::Type::Background, palette).darker(130);

    const int startNote    = m_config.minNote;
    const int endNote      = m_config.maxNote;
    const int noteCount    = std::max(1, endNote - startNote + 1);
    const double noteWidth = static_cast<double>(geometry.plotRect.width()) / static_cast<double>(noteCount);
    const int top          = geometry.spectrumRect.top();

    const QRect keyRect{geometry.plotRect.left(), top, geometry.plotRect.width(), geometry.plotRect.bottom() - top + 1};
    if(keyRect.isEmpty()) {
        return;
    }

    for(int note = startNote; note <= endNote; ++note) {
        const int semitone = ((note % 12) + 12) % 12;
        const bool black   = isBlackKey(semitone);
        if((black && !showBlackKeys) || (!black && !showWhiteKeys)) {
            continue;
        }

        const QRect rect = noteCellRect(keyRect, note - startNote, noteCount);
        painter.fillRect(rect, black ? blackKeyColour : whiteKeyColour);
    }

    if(noteWidth > 2.0) {
        painter.setPen(separatorColour);
        for(int note = startNote + 1; note <= endNote; ++note) {
            const int x = noteEdgeX(geometry.plotRect, note - startNote, noteCount);
            painter.drawLine(x, keyRect.top(), x, keyRect.bottom());
        }
    }
}
} // namespace Fooyin::Spectrum
