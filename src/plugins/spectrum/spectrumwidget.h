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

#include "spectrumsettings.h"

#include <gui/fywidget.h>

#include <QString>
#include <QVariant>

class QJsonObject;

namespace Fooyin {
class EngineController;
class PlayerController;
class SettingsManager;

namespace Spectrum {
class SpectrumView;
class SpectrumWidget : public FyWidget
{
    Q_OBJECT

public:
    struct ConfigData
    {
        int bandCount{DefaultBandCount};
        int barSpacing{DefaultBarSpacing};
        int barSections{DefaultBarSections};
        int sectionSpacing{DefaultSectionSpacing};
        int minFrequencyHz{DefaultMinFrequencyHz};
        int maxFrequencyHz{DefaultMaxFrequencyHz};
        int minNote{DefaultMinNote};
        int maxNote{DefaultMaxNote};
        int pitchHz{DefaultPitchHz};
        int transpose{DefaultTranspose};
        int amplitudeMinDb{DefaultAmplitudeMinDb};
        int amplitudeMaxDb{DefaultAmplitudeMaxDb};
        bool amplitudesEnabled{DefaultAmplitudesEnabled};
        int amplitudeHoldTimeMs{DefaultAmplitudeHoldTimeMs};
        int amplitudeGravity{DefaultAmplitudeGravity};
        bool peaksEnabled{DefaultPeaksEnabled};
        int peakHoldTimeMs{DefaultPeakHoldTimeMs};
        int peakGravity{DefaultPeakGravity};
        int updateFps{DefaultUpdateFps};
        int fftSize{DefaultFftSize};
        WindowFunction windowFunction{WindowFunction::BlackmanHarris};
        GradientOrientation gradientOrientation{GradientOrientation::Vertical};
        LabelMode labelMode{LabelMode::Frequency};
        DrawStyle drawStyle{DrawStyle::Bars};
        bool showTopLabels{false};
        bool showBottomLabels{true};
        bool showLeftLabels{false};
        bool showRightLabels{true};
        bool showHorizontalGrid{true};
        bool showVerticalGrid{false};
        bool showWhiteKeys{false};
        bool showBlackKeys{false};
        bool showTooltip{false};
        bool fillSpectrum{true};
        bool interpolate{true};
        QString axisFont;
        QVariant colours;
    };

    explicit SpectrumWidget(EngineController* engine, PlayerController* playerController, SettingsManager* settings,
                            QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void openConfigDialog() override;

private:
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;
    void showContextMenu(const QPoint& globalPos);

    SettingsManager* m_settings;
    SpectrumView* m_view;
    ConfigData m_config;
};
} // namespace Spectrum
} // namespace Fooyin
