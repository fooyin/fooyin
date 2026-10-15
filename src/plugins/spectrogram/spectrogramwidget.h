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

#include "spectrogramconfig.h"

#include <gui/fywidget.h>

#include <QPointer>

#include <memory>

class QJsonObject;
class QVBoxLayout;

namespace Fooyin {
class AudioLoader;
class EngineController;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

namespace Spectrogram {
class SpectrogramView;
class SpectrogramWidget : public FyWidget
{
    Q_OBJECT
public:
    SpectrogramWidget(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                      EngineController* engine, TrackSelectionController* trackSelection, SettingsManager* settings,
                      QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] SpectrogramConfig factoryConfig() const;
    [[nodiscard]] SpectrogramConfig defaultConfig() const;
    [[nodiscard]] const SpectrogramConfig& currentConfig() const;
    void saveDefaults(const SpectrogramConfig& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const SpectrogramConfig& config);

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void configChanged();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void openConfigDialog() override;

private:
    [[nodiscard]] SpectrogramConfig configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const SpectrogramConfig& config, QJsonObject& layout) const;

    std::shared_ptr<AudioLoader> m_audioLoader;
    PlayerController* m_playerController;
    EngineController* m_engine;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;
    QVBoxLayout* m_layout;
    QPointer<SpectrogramView> m_view;
    SpectrogramConfig m_config;
};
} // namespace Spectrogram
} // namespace Fooyin