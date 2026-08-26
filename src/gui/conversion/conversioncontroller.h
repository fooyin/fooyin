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

#include <core/engine/conversion/conversiondefs.h>
#include <gui/conversion/conversionservice.h>

#include <QObject>

#include <memory>

class QWidget;

namespace Fooyin {
class AudioEncoderRegistry;
class AudioLoader;
class DspChainStore;
class DspRegistry;
class DspSettingsRegistry;
class SettingsManager;

class ConversionController : public QObject,
                             public ConversionService
{
    Q_OBJECT

public:
    ConversionController(std::shared_ptr<AudioLoader> audioLoader, AudioEncoderRegistry* encoderRegistry,
                         DspRegistry* dspRegistry, DspChainStore* dspChainStore,
                         DspSettingsRegistry* dspSettingsRegistry, SettingsManager* settings, QWidget* parentWindow,
                         QObject* parent = nullptr);

    void showSetup(const TrackList& tracks) override;
    void showSetup(const TrackList& tracks, const QString& suggestedFilenamePattern,
                   std::shared_ptr<ConversionInputObserver> sourceObserver,
                   std::function<void(const std::vector<ConversionTrackResult>&)> completion) override;

    [[nodiscard]] std::vector<ConversionPresetInfo> presets() const override;
    bool startPreset(const QString& presetId, const TrackList& tracks) override;
    bool startPreset(const QString& presetId, const TrackList& tracks, const QString& suggestedFilenamePattern,
                     std::shared_ptr<ConversionInputObserver> sourceObserver,
                     std::function<void(const std::vector<ConversionTrackResult>&)> completion) override;

    void start(ConversionJob job, QString askFolder, bool showReport);
    void start(ConversionJob job, QString askFolder, bool showReport,
               std::shared_ptr<ConversionInputObserver> sourceObserver,
               std::function<void(const std::vector<ConversionTrackResult>&)> completion);

Q_SIGNALS:
    void conversionPresetsChanged();

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
    AudioEncoderRegistry* m_encoderRegistry;
    DspRegistry* m_dspRegistry;
    DspChainStore* m_dspChainStore;
    DspSettingsRegistry* m_dspSettingsRegistry;
    SettingsManager* m_settings;
    QWidget* m_parentWindow;
};
} // namespace Fooyin
