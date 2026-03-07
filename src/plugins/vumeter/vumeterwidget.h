/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audioformat.h>
#include <core/engine/levelframe.h>
#include <gui/fywidget.h>

#include <QVariant>

class QJsonObject;

namespace Fooyin {
class PlayerController;
class SettingsManager;

namespace VuMeter {
class VuMeterWidgetPrivate;

class VuMeterWidget : public FyWidget
{
    Q_OBJECT

public:
    enum class Type : uint8_t
    {
        Peak = 0,
        Rms
    };

    explicit VuMeterWidget(Type type, PlayerController* playerController, SettingsManager* settings,
                           QWidget* parent = nullptr);
    ~VuMeterWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void renderLevel(const LevelFrame& frame);

    void setOrientation(Qt::Orientation orientation);
    void setShowLegend(bool show);
    void setChannelSpacing(int size);
    void setBarSize(int size);
    void setBarSpacing(int size);
    void setBarSections(int count);
    void setSectionSpacing(int size);

    [[nodiscard]] QSize minimumSizeHint() const override;

    struct ConfigData
    {
        double peakHoldTime{1.5};
        double falloffTime{13.0};
        int updateFps{40};
        int channelSpacing{1};
        int barSize{0};
        int barSpacing{1};
        int barSections{1};
        int sectionSpacing{1};
        QVariant meterColours;
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void openConfigDialog() override;
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;

    SettingsManager* m_settings;
    ConfigData m_config;

    std::unique_ptr<VuMeterWidgetPrivate> p;
};
} // namespace VuMeter
} // namespace Fooyin
