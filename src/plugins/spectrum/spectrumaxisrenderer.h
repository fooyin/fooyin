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

#include "spectrumwidget.h"

#include <QRect>
#include <QRectF>
#include <QString>

#include <span>
#include <vector>

class QFont;
class QFontMetrics;
class QPainter;
class QPalette;
class QSize;

namespace Fooyin::Spectrum {
struct SpectrumHorizontalLabel
{
    QString text;
    int centerX{0};
};

struct SpectrumPlotGeometry
{
    QRect spectrumRect;
    QRect plotRect;
    std::vector<QRectF> barRects;
    std::vector<int> sourceBands;
    std::vector<SpectrumHorizontalLabel> horizontalLabels;
    bool drawAmplitudeLabels{false};
};

class SpectrumAxisRenderer
{
public:
    void setConfig(const SpectrumWidget::ConfigData& config);

    [[nodiscard]] SpectrumPlotGeometry layout(const QRect& rect, const QFont& axisFont, int bandCount, qreal dpr) const;
    void paint(QPainter& painter, const SpectrumPlotGeometry& geometry, const QSize& size, const QPalette& palette,
               const QFont& axisFont) const;

    static QFont defaultAxisFont(const QFont& baseFont);
    [[nodiscard]] float effectiveMinFrequencyHz() const;
    [[nodiscard]] float effectiveMaxFrequencyHz() const;
    [[nodiscard]] float frequencyForBand(int band, int bandCount) const;
    [[nodiscard]] int noteForBand(int band, int bandCount) const;

    [[nodiscard]] static QString formatFrequencyLabel(float frequencyHz);
    [[nodiscard]] static QString formatTooltipFrequency(float frequencyHz);
    [[nodiscard]] static QString formatNoteLabel(int noteIndex);
    [[nodiscard]] static float frequencyForNote(int noteIndex, int pitchHz, int transpose);

private:
    struct AmplitudeLabel;

    [[nodiscard]] int noteCenterX(int note, const QRect& plotRect) const;
    [[nodiscard]] int frequencyToX(float frequencyHz, const QRect& plotRect) const;
    [[nodiscard]] std::vector<float> verticalGridFrequencies() const;
    [[nodiscard]] QRect plotRectForSpectrumRect(const QRect& spectrumRect) const;
    [[nodiscard]] SpectrumPlotGeometry barGeometryForPlotRect(const QRect& plotRect, int bandCount, qreal dpr) const;
    [[nodiscard]] std::vector<SpectrumHorizontalLabel> horizontalLabelCandidates(const SpectrumPlotGeometry& geometry,
                                                                                 const QRect& plotRect, int bandCount,
                                                                                 int maxPriority) const;
    [[nodiscard]] std::vector<SpectrumHorizontalLabel> horizontalLabels(const QFontMetrics& metrics,
                                                                        const QRect& spectrumRect,
                                                                        const QRect& plotRect, int bandCount,
                                                                        int maxPriority, qreal dpr) const;
    [[nodiscard]] std::vector<AmplitudeLabel> amplitudeLabels(const QFontMetrics& metrics, int height,
                                                              const QRect& plotRect) const;
    void drawKeyBackgrounds(QPainter& painter, const SpectrumPlotGeometry& geometry, const QPalette& palette) const;

    SpectrumWidget::ConfigData m_config;
};
} // namespace Fooyin::Spectrum
