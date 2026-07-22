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

#include "convertersettingsstore.h"
#include "encoderprofiletablemodel.h"

#include <core/engine/audioencoder.h>
#include <core/engine/conversion/conversiondefs.h>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QModelIndex;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableView;

namespace Fooyin {
class AudioEncoderRegistry;
class DspChainStore;
class DspSettingsRegistry;
class DspDelegate;
class DspModel;
class ExtendableTableView;
class SettingsManager;

class ConverterSetupDialog : public QDialog
{
    Q_OBJECT

public:
    ConverterSetupDialog(AudioEncoderRegistry* registry, DspChainStore* dspChainStore, SettingsManager* settings,
                         TrackList tracks, QWidget* parent = nullptr,
                         DspSettingsRegistry* dspSettingsRegistry = nullptr);

    [[nodiscard]] ConversionJob job() const;
    [[nodiscard]] QString askFolder() const;
    [[nodiscard]] bool showReport() const;

    void accept() override;

private:
    QWidget* createPresetsSidebar();
    QWidget* createSummaryBar();
    QWidget* createOutputPage();
    QWidget* createDestinationPage();
    QWidget* createProcessingPage();
    QWidget* createOtherPage();

    void populateProfiles();
    void refreshProfileTable(int selectedRow = -1);
    void addProfile();
    void editProfile();
    void saveProfiles() const;
    void refreshDspModels();
    void addDsp(const QModelIndex& index);
    void configureDsp(const QModelIndex& index);
    void removeDsp(const QModelIndex& index);
    void syncDspChainFromModel();
    void populatePresets();
    void loadPreset();
    void savePreset();
    void removePreset();
    void importPresets();
    void exportPreset();
    void applyPreset(const StoredConversionPreset& stored);
    void applyDefaultPreset();
    void savePresets() const;
    void browseForFolder();
    void updateState();
    void updatePreview();
    void updateOverview();

    [[nodiscard]] const AudioEncoderInfo* selectedEncoder() const;
    [[nodiscard]] DestinationMode destinationMode() const;

    AudioEncoderRegistry* m_registry;
    DspChainStore* m_dspChainStore;
    DspSettingsRegistry* m_dspSettingsRegistry;
    SettingsManager* m_settings;
    TrackList m_tracks;
    std::vector<AudioEncoderInfo> m_runtimeEncoders;

    QList<EncoderProfileEntry> m_profiles;
    Engine::DspChain m_availableDsps;
    Engine::DspChain m_dspChain;
    std::vector<StoredConversionPreset> m_presets;
    QString m_askFolder;

    QTabWidget* m_pages;
    QListWidget* m_presetList;
    QPushButton* m_loadPreset;
    QPushButton* m_savePreset;
    QPushButton* m_removePreset;
    QPushButton* m_importPresets;
    QPushButton* m_exportPreset;
    ExtendableTableView* m_profileTable;
    EncoderProfileTableModel* m_profileModel;
    QPushButton* m_editProfile;
    QComboBox* m_sampleFormat;
    QComboBox* m_dither;
    QComboBox* m_destinationMode;
    QLineEdit* m_folder;
    QPushButton* m_browse;
    QLineEdit* m_filenamePattern;
    QComboBox* m_existingFileMode;
    QComboBox* m_outputStyle;
    QListWidget* m_preview;
    QCheckBox* m_transferMetadata;
    QCheckBox* m_transferRating;
    QCheckBox* m_transferPlaycount;
    QCheckBox* m_transferPictures;
    QComboBox* m_replayGainMode;
    QCheckBox* m_replayGainPreventClipping;
    QDoubleSpinBox* m_replayGainPreamp;
    QDoubleSpinBox* m_replayGainWithoutInfoPreamp;
    QCheckBox* m_preserveDspState;
    QTableView* m_activeDsps;
    QListView* m_availableDspList;
    DspModel* m_activeDspModel;
    DspModel* m_availableDspModel;
    DspDelegate* m_activeDspDelegate;
    DspDelegate* m_availableDspDelegate;
    QCheckBox* m_generatePreview;
    QSpinBox* m_previewPercentage;
    QCheckBox* m_showReport;
    QLineEdit* m_copyFilesPattern;
    QCheckBox* m_verifyOutput;
    QLabel* m_outputSummary;
    QLabel* m_destinationSummary;
    QLabel* m_processingSummary;
    QLabel* m_otherSummary;
    QPushButton* m_convert;
};
} // namespace Fooyin
