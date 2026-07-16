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

#include "convertersetupdialog.h"

#include "convertersettingsstore.h"
#include "dsp/dspsettingsregistry.h"
#include "encoderprofiledialog.h"
#include "encoderprofiletablemodel.h"
#include "settings/playback/dspdelegate.h"
#include "settings/playback/dspmodel.h"

#include <core/engine/audioencoderregistry.h>
#include <core/engine/conversion/conversionpathresolver.h>
#include <core/engine/dsp/dspchainstore.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QUuid>
#include <QVBoxLayout>

#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
ConverterSetupDialog::ConverterSetupDialog(AudioEncoderRegistry* registry, DspChainStore* dspChainStore,
                                           SettingsManager* settings, TrackList tracks, QWidget* parent,
                                           DspSettingsRegistry* dspSettingsRegistry)
    : QDialog{parent}
    , m_registry{registry}
    , m_dspChainStore{dspChainStore}
    , m_dspSettingsRegistry{dspSettingsRegistry}
    , m_settings{settings}
    , m_tracks{std::move(tracks)}
    , m_pages{new QTabWidget(this)}
    , m_presetList{new QListWidget(this)}
    , m_loadPreset{new QPushButton(tr("Load"), this)}
    , m_savePreset{new QPushButton(tr("Save"), this)}
    , m_removePreset{new QPushButton(tr("Remove"), this)}
    , m_importPresets{new QPushButton(tr("Import"), this)}
    , m_exportPreset{new QPushButton(tr("Export"), this)}
    , m_profileTable{new ExtendableTableView(this)}
    , m_profileModel{new EncoderProfileTableModel(&m_profiles, &m_runtimeEncoders, this)}
    , m_editProfile{new QPushButton(tr("Edit"), this)}
    , m_sampleFormat{new QComboBox(this)}
    , m_dither{new QComboBox(this)}
    , m_destinationMode{new QComboBox(this)}
    , m_folder{new QLineEdit(this)}
    , m_browse{new QPushButton(tr("Browse…"), this)}
    , m_filenamePattern{new QLineEdit(u"%title%"_s, this)}
    , m_existingFileMode{new QComboBox(this)}
    , m_outputStyle{new QComboBox(this)}
    , m_preview{new QListWidget(this)}
    , m_transferMetadata{new QCheckBox(tr("Transfer metadata (tags)"), this)}
    , m_transferRating{new QCheckBox(tr("Transfer rating"), this)}
    , m_transferPlaycount{new QCheckBox(tr("Transfer play count"), this)}
    , m_transferPictures{new QCheckBox(tr("Transfer attached pictures"), this)}
    , m_replayGainMode{new QComboBox(this)}
    , m_replayGainPreventClipping{new QCheckBox(tr("Prevent clipping"), this)}
    , m_replayGainPreamp{new QDoubleSpinBox(this)}
    , m_replayGainWithoutInfoPreamp{new QDoubleSpinBox(this)}
    , m_preserveDspState{new QCheckBox(tr("Don't reset DSP between tracks"), this)}
    , m_activeDsps{new QTableView(this)}
    , m_availableDspList{new QListView(this)}
    , m_activeDspModel{new DspModel(this)}
    , m_availableDspModel{new DspModel(this)}
    , m_activeDspDelegate{new DspDelegate(m_activeDsps, this)}
    , m_availableDspDelegate{new DspDelegate(m_availableDspList, this, DspDelegate::Mode::Available)}
    , m_generatePreview{new QCheckBox(tr("Generate short previews instead of converting entire tracks"), this)}
    , m_previewPercentage{new QSpinBox(this)}
    , m_showReport{new QCheckBox(tr("Show full status report"), this)}
    , m_copyFilesPattern{new QLineEdit(this)}
    , m_verifyOutput{new QCheckBox(tr("Verify converted output"), this)}
    , m_outputSummary{new QLabel(this)}
    , m_destinationSummary{new QLabel(this)}
    , m_processingSummary{new QLabel(this)}
    , m_otherSummary{new QLabel(this)}
    , m_convert{nullptr}
{
    setWindowTitle(tr("Converter Setup"));
    resize(900, 600);

    auto* presetsSidebar = createPresetsSidebar();
    m_pages->addTab(createOutputPage(), tr("Output"));
    m_pages->addTab(createDestinationPage(), tr("Destination"));
    m_pages->addTab(createProcessingPage(), tr("Processing"));
    m_pages->addTab(createOtherPage(), tr("Other"));
    m_pages->tabBar()->setExpanding(false);
    m_pages->setCurrentIndex(0);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->addWidget(m_pages, 1);
    contentLayout->addWidget(presetsSidebar);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_convert       = buttonBox->addButton(tr("Convert"), QDialogButtonBox::AcceptRole);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &ConverterSetupDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(contentLayout, 1);
    layout->addWidget(createSummaryBar());
    layout->addWidget(buttonBox);

    populateProfiles();
    populatePresets();
    applyDefaultPreset();
}

ConversionJob ConverterSetupDialog::job() const
{
    ConversionJob result;
    result.tracks = m_tracks;

    if(const AudioEncoderInfo* encoder = selectedEncoder()) {
        result.preset.encoder.profile = encoder->profile;
    }

    result.preset.encoder.outputSampleFormat  = static_cast<SampleFormat>(m_sampleFormat->currentData().toInt());
    result.preset.encoder.ditherMode          = static_cast<DitherMode>(m_dither->currentData().toInt());
    result.preset.destination.mode            = destinationMode();
    result.preset.destination.fixedFolder     = m_folder->text().trimmed();
    result.preset.destination.filenamePattern = m_filenamePattern->text().trimmed();
    result.preset.destination.existingFileMode
        = static_cast<ExistingFileMode>(m_existingFileMode->currentData().toInt());
    result.preset.destination.outputStyle      = static_cast<OutputStyle>(m_outputStyle->currentData().toInt());
    result.preset.processing.transferMetadata  = m_transferMetadata->isChecked();
    result.preset.processing.transferRating    = m_transferRating->isChecked();
    result.preset.processing.transferPlaycount = m_transferPlaycount->isChecked();
    result.preset.processing.transferPictures  = m_transferPictures->isChecked() && m_transferPictures->isEnabled();
    result.preset.processing.replayGainMode
        = static_cast<ConversionReplayGainMode>(m_replayGainMode->currentData().toInt());
    result.preset.processing.replayGainPreventClipping     = m_replayGainPreventClipping->isChecked();
    result.preset.processing.replayGainPreampDb            = m_replayGainPreamp->value();
    result.preset.processing.replayGainWithoutInfoPreampDb = m_replayGainWithoutInfoPreamp->value();
    result.preset.processing.dspChain                      = m_dspChain;
    result.preset.processing.preserveDspStateBetweenTracks = m_preserveDspState->isChecked();
    result.preset.preview.enabled                          = m_generatePreview->isChecked();
    result.preset.preview.lengthPercentage                 = m_previewPercentage->value();
    result.preset.other.copyFilesPattern                   = m_copyFilesPattern->text().trimmed();
    result.preset.other.verifyOutput                       = m_verifyOutput->isChecked();

    return result;
}

QString ConverterSetupDialog::askFolder() const
{
    return m_askFolder;
}

bool ConverterSetupDialog::showReport() const
{
    return m_showReport->isChecked();
}

void ConverterSetupDialog::accept()
{
    if(!selectedEncoder()) {
        QMessageBox::warning(this, tr("Converter Setup"), tr("No output encoder is available."));
        return;
    }

    if(destinationMode() == DestinationMode::Ask) {
        m_askFolder = QFileDialog::getExistingDirectory(this, tr("Choose destination"), QDir::homePath(),
                                                        QFileDialog::DontResolveSymlinks);
        if(m_askFolder.isEmpty()) {
            return;
        }
    }

    QDialog::accept();
}

QWidget* ConverterSetupDialog::createPresetsSidebar()
{
    auto* page = new QWidget(this);
    page->setFixedWidth(210);

    auto* presetButtons = new QGridLayout();

    int row{0};
    presetButtons->addWidget(m_loadPreset, row, 0);
    presetButtons->addWidget(m_savePreset, row++, 1);
    presetButtons->addWidget(m_removePreset, row++, 0, 1, 2);
    presetButtons->addWidget(m_importPresets, row, 0);
    presetButtons->addWidget(m_exportPreset, row++, 1);

    auto* presetsLayout = new QVBoxLayout(page);
    presetsLayout->addWidget(new QLabel(tr("Saved presets"), page));
    presetsLayout->addWidget(m_presetList, 1);
    presetsLayout->addLayout(presetButtons);

    QObject::connect(m_loadPreset, &QAbstractButton::clicked, this, &ConverterSetupDialog::loadPreset);
    QObject::connect(m_savePreset, &QAbstractButton::clicked, this, &ConverterSetupDialog::savePreset);
    QObject::connect(m_removePreset, &QAbstractButton::clicked, this, &ConverterSetupDialog::removePreset);
    QObject::connect(m_importPresets, &QAbstractButton::clicked, this, &ConverterSetupDialog::importPresets);
    QObject::connect(m_exportPreset, &QAbstractButton::clicked, this, &ConverterSetupDialog::exportPreset);
    QObject::connect(m_presetList, &QListWidget::itemDoubleClicked, this, &ConverterSetupDialog::loadPreset);
    QObject::connect(m_presetList, &QListWidget::currentRowChanged, this, [this](int currentRow) {
        m_loadPreset->setEnabled(currentRow >= 0);
        m_removePreset->setEnabled(currentRow > 0);
        m_exportPreset->setEnabled(currentRow > 0);
    });

    return page;
}

QWidget* ConverterSetupDialog::createSummaryBar()
{
    auto* summaryBox = new QGroupBox(tr("Current settings"), this);
    auto* grid       = new QGridLayout(summaryBox);

    const auto addSummary = [grid, summaryBox](int row, int column, const QString& title, QLabel* value) {
        auto* label = new QLabel(title + ":"_L1, summaryBox);
        QFont font  = label->font();
        font.setBold(true);
        label->setFont(font);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(label, row, column);
        grid->addWidget(value, row, column + 1);
    };

    addSummary(0, 0, tr("Output"), m_outputSummary);
    addSummary(0, 2, tr("Destination"), m_destinationSummary);
    addSummary(1, 0, tr("Processing"), m_processingSummary);
    addSummary(1, 2, tr("Other"), m_otherSummary);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    return summaryBox;
}

QWidget* ConverterSetupDialog::createOutputPage()
{
    m_profileTable->setExtendableModel(m_profileModel);
    m_profileTable->setExtendableColumn(0);
    m_profileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_profileTable->verticalHeader()->hide();
    m_profileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_profileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_profileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_profileTable->addCustomTool(m_editProfile);

    m_sampleFormat->addItem(tr("Auto"), static_cast<int>(SampleFormat::Unknown));
    m_sampleFormat->addItem(tr("8-bit integer"), static_cast<int>(SampleFormat::U8));
    m_sampleFormat->addItem(tr("16-bit integer"), static_cast<int>(SampleFormat::S16));
    m_sampleFormat->addItem(tr("24-bit integer"), static_cast<int>(SampleFormat::S24In32));
    m_sampleFormat->addItem(tr("32-bit integer"), static_cast<int>(SampleFormat::S32));
    m_sampleFormat->addItem(tr("32-bit floating point"), static_cast<int>(SampleFormat::F32));

    m_dither->addItem(tr("Never"), static_cast<int>(DitherMode::Never));
    m_dither->addItem(tr("Lossy sources only"), static_cast<int>(DitherMode::LossySourceOnly));
    m_dither->addItem(tr("Always"), static_cast<int>(DitherMode::Always));

    auto* page    = new QWidget(this);
    auto* options = new QGridLayout();

    int row{0};
    options->addWidget(new QLabel(tr("Output sample format") + ":"_L1, page), row, 0);
    options->addWidget(m_sampleFormat, row++, 1);
    options->addWidget(new QLabel(tr("Dither") + ":"_L1, page), row, 0);
    options->addWidget(m_dither, row++, 1);
    options->setColumnStretch(1, 1);

    auto* layout = new QVBoxLayout(page);
    layout->addWidget(m_profileTable, 1);
    layout->addLayout(options);

    QObject::connect(m_profileTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ConverterSetupDialog::updateState);
    QObject::connect(m_profileTable, &QAbstractItemView::doubleClicked, this, &ConverterSetupDialog::editProfile);
    QObject::connect(m_profileModel, &EncoderProfileTableModel::addProfileRequested, this,
                     &ConverterSetupDialog::addProfile);
    QObject::connect(m_profileModel, &EncoderProfileTableModel::profilesChanged, this,
                     &ConverterSetupDialog::saveProfiles);
    QObject::connect(m_profileModel, &EncoderProfileTableModel::profilesChanged, this,
                     &ConverterSetupDialog::updateState);
    QObject::connect(m_editProfile, &QAbstractButton::clicked, this, &ConverterSetupDialog::editProfile);
    QObject::connect(m_sampleFormat, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_dither, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateOverview);

    return page;
}

QWidget* ConverterSetupDialog::createDestinationPage()
{
    m_destinationMode->addItem(tr("Ask when conversion starts"), static_cast<int>(DestinationMode::Ask));
    m_destinationMode->addItem(tr("Source track folder"), static_cast<int>(DestinationMode::SourceFolder));
    m_destinationMode->addItem(tr("Specified folder"), static_cast<int>(DestinationMode::FixedFolder));
    m_existingFileMode->addItem(tr("Ask"), static_cast<int>(ExistingFileMode::Ask));
    m_existingFileMode->addItem(tr("Skip"), static_cast<int>(ExistingFileMode::Skip));
    m_existingFileMode->addItem(tr("Overwrite"), static_cast<int>(ExistingFileMode::Overwrite));
    m_outputStyle->addItem(tr("Convert each track to an individual file"),
                           static_cast<int>(OutputStyle::IndividualFiles));
    m_outputStyle->addItem(tr("Generate one multi-track file per name group"),
                           static_cast<int>(OutputStyle::MultiTrackFiles));
    m_outputStyle->addItem(tr("Merge all tracks into one output file"), static_cast<int>(OutputStyle::MergeTracks));

    auto* folderLayout = new QHBoxLayout();
    folderLayout->addWidget(m_folder, 1);
    folderLayout->addWidget(m_browse);

    auto* page = new QWidget(this);
    auto* form = new QGridLayout();

    int row{0};
    form->addWidget(new QLabel(tr("Output path") + ":"_L1, page), row, 0);
    form->addWidget(m_destinationMode, row++, 1);
    form->addWidget(new QLabel(tr("Folder") + ":"_L1, page), row, 0);
    form->addLayout(folderLayout, row++, 1);
    form->addWidget(new QLabel(tr("Output style") + ":"_L1, page), row, 0);
    form->addWidget(m_outputStyle, row++, 1);
    form->addWidget(new QLabel(tr("Name format") + ":"_L1, page), row, 0);
    form->addWidget(m_filenamePattern, row++, 1);
    form->addWidget(new QLabel(tr("If file already exists") + ":"_L1, page), row, 0);
    form->addWidget(m_existingFileMode, row++, 1);
    form->setColumnStretch(1, 1);

    auto* layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("Preview") + ":"_L1, page));
    layout->addWidget(m_preview, 1);

    QObject::connect(m_browse, &QAbstractButton::clicked, this, &ConverterSetupDialog::browseForFolder);
    QObject::connect(m_destinationMode, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_existingFileMode, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_outputStyle, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_folder, &QLineEdit::textChanged, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_filenamePattern, &QLineEdit::textChanged, this, &ConverterSetupDialog::updateState);
    return page;
}

QWidget* ConverterSetupDialog::createProcessingPage()
{
    m_transferMetadata->setChecked(true);
    m_transferRating->setChecked(true);
    m_transferPlaycount->setChecked(false);
    m_transferPictures->setChecked(true);
    m_replayGainMode->addItem(tr("None"), static_cast<int>(ConversionReplayGainMode::None));
    m_replayGainMode->addItem(tr("Track gain"), static_cast<int>(ConversionReplayGainMode::Track));
    m_replayGainMode->addItem(tr("Album gain"), static_cast<int>(ConversionReplayGainMode::Album));
    m_replayGainPreventClipping->setChecked(true);

    for(QDoubleSpinBox* preamp : {m_replayGainPreamp, m_replayGainWithoutInfoPreamp}) {
        preamp->setRange(-20.0, 20.0);
        preamp->setDecimals(1);
        preamp->setSingleStep(0.5);
        preamp->setSuffix(tr(" dB"));
    }

    auto* replayGainBox  = new QGroupBox(tr("ReplayGain processing"), this);
    auto* replayGainForm = new QGridLayout(replayGainBox);
    int row{0};
    replayGainForm->addWidget(new QLabel(tr("Mode") + ":"_L1, replayGainBox), row, 0);
    replayGainForm->addWidget(m_replayGainMode, row++, 1);
    replayGainForm->addWidget(m_replayGainPreventClipping, row++, 1);
    replayGainForm->addWidget(new QLabel(tr("Preamp") + ":"_L1, replayGainBox), row, 0);
    replayGainForm->addWidget(m_replayGainPreamp, row++, 1);
    replayGainForm->addWidget(new QLabel(tr("Without ReplayGain info") + ":"_L1, replayGainBox), row, 0);
    replayGainForm->addWidget(m_replayGainWithoutInfoPreamp, row++, 1);
    auto* replayGainWarning
        = new QLabel(tr("ReplayGain is applied permanently to the converted audio."), replayGainBox);
    replayGainWarning->setWordWrap(true);
    replayGainForm->addWidget(replayGainWarning, row++, 0, 1, 2);
    replayGainForm->setColumnStretch(1, 1);

    m_availableDspModel->setAllowInternalMoves(false);
    m_availableDspModel->setCheckable(false);

    m_activeDsps->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_activeDsps->setDragDropOverwriteMode(false);
    m_activeDsps->horizontalHeader()->hide();
    m_activeDsps->horizontalHeader()->setStretchLastSection(true);
    m_activeDsps->setMouseTracking(true);
    m_activeDsps->setModel(m_activeDspModel);
    m_activeDsps->setItemDelegate(m_activeDspDelegate);

    m_availableDspList->setMouseTracking(true);
    m_availableDspList->setModel(m_availableDspModel);
    m_availableDspList->setItemDelegate(m_availableDspDelegate);

    const auto setupList = [](QAbstractItemView* list, bool acceptDrops) {
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setDragEnabled(true);
        list->setAcceptDrops(acceptDrops);
        list->setDropIndicatorShown(acceptDrops);
        list->setDragDropMode(acceptDrops ? QAbstractItemView::DragDrop : QAbstractItemView::DragOnly);
        list->setDefaultDropAction(acceptDrops ? Qt::MoveAction : Qt::CopyAction);
    };
    setupList(m_activeDsps, true);
    setupList(m_availableDspList, false);

    auto* dspBox       = new QGroupBox(tr("DSP chain"), this);
    auto* activeLayout = new QVBoxLayout();
    activeLayout->addWidget(new QLabel(tr("Active DSPs"), this));
    activeLayout->addWidget(m_activeDsps);
    auto* availableLayout = new QVBoxLayout();
    availableLayout->addWidget(new QLabel(tr("Available DSPs"), this));
    availableLayout->addWidget(m_availableDspList);
    auto* dspListLayout = new QHBoxLayout();
    dspListLayout->addLayout(activeLayout, 1);
    dspListLayout->addLayout(availableLayout, 1);
    auto* dspLayout = new QVBoxLayout(dspBox);
    dspLayout->addLayout(dspListLayout, 1);
    dspLayout->addWidget(m_preserveDspState);

    auto* metadataBox    = new QGroupBox(tr("Metadata"), this);
    auto* metadataLayout = new QVBoxLayout(metadataBox);
    metadataLayout->addWidget(m_transferMetadata);
    metadataLayout->addWidget(m_transferRating);
    metadataLayout->addWidget(m_transferPlaycount);
    metadataLayout->addWidget(m_transferPictures);

    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(replayGainBox);
    layout->addWidget(dspBox, 1);
    layout->addWidget(metadataBox);

    QObject::connect(m_transferMetadata, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_transferRating, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_transferPlaycount, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_transferPictures, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_replayGainMode, &QComboBox::currentIndexChanged, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_replayGainPreventClipping, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_replayGainPreamp, &QDoubleSpinBox::valueChanged, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_replayGainWithoutInfoPreamp, &QDoubleSpinBox::valueChanged, this,
                     &ConverterSetupDialog::updateOverview);
    QObject::connect(m_preserveDspState, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);

    m_availableDsps = m_dspChainStore ? m_dspChainStore->availableDsps() : Engine::DspChain{};
    for(auto& dsp : m_availableDsps) {
        dsp.enabled    = true;
        dsp.instanceId = 0;
    }
    std::ranges::sort(m_availableDsps,
                      [](const auto& lhs, const auto& rhs) { return lhs.name.localeAwareCompare(rhs.name) < 0; });
    refreshDspModels();

    QObject::connect(m_activeDspDelegate, &DspDelegate::removeClicked, this, &ConverterSetupDialog::removeDsp);
    QObject::connect(m_activeDspDelegate, &DspDelegate::configureClicked, this, &ConverterSetupDialog::configureDsp);
    QObject::connect(m_availableDspDelegate, &DspDelegate::addClicked, this, &ConverterSetupDialog::addDsp);
    QObject::connect(m_availableDspList, &QListView::doubleClicked, this, &ConverterSetupDialog::addDsp);
    QObject::connect(m_activeDsps, &QTableView::doubleClicked, this, &ConverterSetupDialog::configureDsp);
    QObject::connect(m_activeDspModel, &QAbstractItemModel::dataChanged, this, [this] { syncDspChainFromModel(); });
    QObject::connect(m_activeDspModel, &QAbstractItemModel::rowsInserted, this, [this] { syncDspChainFromModel(); });
    QObject::connect(m_activeDspModel, &QAbstractItemModel::rowsRemoved, this, [this] { syncDspChainFromModel(); });
    QObject::connect(m_activeDspModel, &QAbstractItemModel::modelReset, this, [this] { syncDspChainFromModel(); });
    return page;
}

QWidget* ConverterSetupDialog::createOtherPage()
{
    m_copyFilesPattern->setPlaceholderText(u"*.jpg; *.png"_s);
    m_previewPercentage->setRange(1, 100);
    m_previewPercentage->setValue(20);
    m_previewPercentage->setSuffix(u" %"_s);
    m_showReport->setChecked(true);

    auto* previewBox    = new QGroupBox(tr("Preview generation"), this);
    auto* previewLayout = new QGridLayout(previewBox);

    int row{0};
    previewLayout->addWidget(m_generatePreview, row++, 0, 1, 2);
    previewLayout->addWidget(new QLabel(tr("Length") + ":"_L1, previewBox), row, 0);
    previewLayout->addWidget(m_previewPercentage, row++, 1);
    previewLayout->setColumnStretch(1, 1);

    auto* whenDoneBox    = new QGroupBox(tr("When done"), this);
    auto* whenDoneLayout = new QVBoxLayout(whenDoneBox);
    whenDoneLayout->addWidget(m_showReport);
    whenDoneLayout->addWidget(m_verifyOutput);

    auto* copyFilesBox    = new QGroupBox(tr("Copy other files to the destination folder"), this);
    auto* copyFilesLayout = new QVBoxLayout(copyFilesBox);
    copyFilesLayout->addWidget(m_copyFilesPattern);

    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(previewBox);
    layout->addWidget(whenDoneBox);
    layout->addWidget(copyFilesBox);
    layout->addStretch();

    QObject::connect(m_generatePreview, &QCheckBox::toggled, this, &ConverterSetupDialog::updateState);
    QObject::connect(m_previewPercentage, &QSpinBox::valueChanged, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_showReport, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_copyFilesPattern, &QLineEdit::textChanged, this, &ConverterSetupDialog::updateOverview);
    QObject::connect(m_verifyOutput, &QCheckBox::toggled, this, &ConverterSetupDialog::updateOverview);

    return page;
}

void ConverterSetupDialog::populateProfiles()
{
    m_runtimeEncoders = m_registry ? m_registry->availableEncoders() : std::vector<AudioEncoderInfo>{};
    m_profiles.clear();

    for(const AudioEncoderInfo& info : std::as_const(m_runtimeEncoders)) {
        m_profiles.push_back({.info = info, .storageId = u"builtin:%1"_s.arg(info.id), .builtIn = true});
    }

    for(const StoredEncoderProfile& stored : ConverterSettings::encoderProfiles()) {
        const auto runtime = std::ranges::find_if(
            m_runtimeEncoders, [&stored](const AudioEncoderInfo& info) { return info.id == stored.baseEncoderId; });
        if(runtime == m_runtimeEncoders.cend()) {
            continue;
        }

        AudioEncoderInfo info{*runtime};
        info.profile = ConverterSettings::applyStoredEncoderProfile(std::move(info.profile), stored);
        info.name    = info.profile.name;
        if(const QString description = EncoderProfileTableModel::profileDescription(info.profile);
           !description.isEmpty()) {
            info.description = description;
        }

        if(stored.overridesBuiltIn) {
            const auto builtIn = std::ranges::find_if(m_profiles, [&stored](const EncoderProfileEntry& entry) {
                return entry.info.id == stored.baseEncoderId;
            });
            if(builtIn != m_profiles.end()) {
                builtIn->info      = std::move(info);
                builtIn->storageId = stored.storageId;
                builtIn->persisted = true;
            }
        }
        else {
            m_profiles.push_back(
                {.info = std::move(info), .storageId = stored.storageId, .builtIn = false, .persisted = true});
        }
    }

    refreshProfileTable(0);
}

void ConverterSetupDialog::refreshProfileTable(int selectedRow)
{
    if(selectedRow < 0) {
        selectedRow = m_profileTable->currentIndex().row();
    }

    m_profileModel->resetProfiles();

    if(!m_profiles.empty()) {
        m_profileTable->selectRow(std::clamp(selectedRow, 0, static_cast<int>(m_profiles.size()) - 1));
    }
}

void ConverterSetupDialog::addProfile()
{
    if(m_runtimeEncoders.empty()) {
        return;
    }

    const AudioEncoderInfo initial = selectedEncoder() ? *selectedEncoder() : m_runtimeEncoders.front();
    m_profileModel->insertPendingProfile(initial);

    auto* dialog = new EncoderProfileDialog(m_runtimeEncoders, initial, true, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if(result == QDialog::Accepted) {
            m_profileModel->commitPendingProfile(dialog->encoderInfo());
        }
        else {
            m_profileModel->removePendingRow();
        }
    });

    dialog->open();
}

void ConverterSetupDialog::editProfile()
{
    const int row = m_profileTable->currentIndex().row();
    if(row < 0 || std::cmp_greater_equal(row, m_profiles.size())) {
        return;
    }

    auto* dialog = new EncoderProfileDialog(m_runtimeEncoders, m_profiles.at(row).info, false, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::accepted, this, [this, dialog, row]() {
        if(row < 0 || std::cmp_greater_equal(row, m_profiles.size())) {
            return;
        }

        EncoderProfileEntry& entry = m_profiles[row];
        entry.info                 = dialog->encoderInfo();

        if(const QString description = EncoderProfileTableModel::profileDescription(entry.info.profile);
           !description.isEmpty()) {
            entry.info.description = description;
        }

        entry.persisted = true;
        saveProfiles();
        m_profileModel->refreshRow(row);
        updateState();
    });

    dialog->open();
}

void ConverterSetupDialog::saveProfiles() const
{
    std::vector<StoredEncoderProfile> storedProfiles;
    for(const EncoderProfileEntry& entry : m_profiles) {
        if(entry.persisted) {
            storedProfiles.emplace_back(entry.storageId, entry.info.id, entry.info.profile, entry.builtIn);
        }
    }
    ConverterSettings::setEncoderProfiles(storedProfiles);
}

void ConverterSetupDialog::refreshDspModels()
{
    const auto resolveSettingsSupport = [this](Engine::DspChain& dsps) {
        for(auto& dsp : dsps) {
            dsp.hasSettings = m_dspSettingsRegistry && m_dspSettingsRegistry->hasProvider(dsp.id);
        }
    };

    resolveSettingsSupport(m_dspChain);
    resolveSettingsSupport(m_availableDsps);

    m_activeDspModel->setup(m_dspChain);
    m_availableDspModel->setup(m_availableDsps);
}

void ConverterSetupDialog::addDsp(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    auto dsp = index.data(DspModel::Dsp).value<Engine::DspDefinition>();
    if(dsp.id.isEmpty()) {
        return;
    }

    dsp.enabled = true;
    auto dsps   = m_activeDspModel->dsps();
    dsps.push_back(std::move(dsp));
    m_activeDspModel->setup(dsps);
}

void ConverterSetupDialog::configureDsp(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    auto dsps = m_activeDspModel->dsps();
    if(index.row() < 0 || std::cmp_greater_equal(index.row(), dsps.size())) {
        return;
    }

    auto& dsp      = dsps[static_cast<size_t>(index.row())];
    auto* provider = m_dspSettingsRegistry->providerFor(dsp.id);
    if(!provider) {
        QMessageBox::information(this, tr("DSP Settings"), tr("This DSP has no configurable settings."));
        return;
    }

    auto settingsDialog = std::unique_ptr<DspSettingsDialog>{provider->createSettingsWidget(this)};
    if(!settingsDialog) {
        QMessageBox::warning(this, tr("DSP Settings"),
                             tr("Unable to open settings for DSP \"%1\".").arg(dsp.name.isEmpty() ? dsp.id : dsp.name));
        return;
    }

    settingsDialog->setWindowTitle(dsp.name.isEmpty() ? dsp.id : dsp.name);
    settingsDialog->loadSettings(dsp.settings);
    if(settingsDialog->exec() != QDialog::Accepted) {
        return;
    }

    dsp.settings = settingsDialog->saveSettings();
    m_activeDspModel->setup(dsps);
}

void ConverterSetupDialog::removeDsp(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }
    m_activeDspModel->removeRow(index.row());
}

void ConverterSetupDialog::syncDspChainFromModel()
{
    auto dsps = m_activeDspModel->dsps();
    uint64_t nextInstanceId{1};
    for(const auto& dsp : dsps) {
        nextInstanceId = std::max(nextInstanceId, dsp.instanceId + 1);
    }

    std::set<uint64_t> instanceIds;
    bool normalised{false};
    for(auto& dsp : dsps) {
        if(dsp.instanceId == 0 || instanceIds.contains(dsp.instanceId)) {
            dsp.instanceId = nextInstanceId++;
            normalised     = true;
        }
        instanceIds.emplace(dsp.instanceId);
    }

    m_dspChain = dsps;
    if(normalised) {
        m_activeDspModel->setup(dsps);
        return;
    }

    updateOverview();
}

void ConverterSetupDialog::populatePresets()
{
    m_presets = ConverterSettings::conversionPresets();
    m_presetList->clear();
    m_presetList->addItem(tr("Default settings"));
    for(const StoredConversionPreset& stored : m_presets) {
        m_presetList->addItem(stored.name);
    }
    m_presetList->setCurrentRow(0);
}

void ConverterSetupDialog::loadPreset()
{
    const int row = m_presetList->currentRow();
    if(row < 0) {
        return;
    }
    if(row == 0) {
        applyDefaultPreset();
    }
    else if(static_cast<size_t>(row - 1) < m_presets.size()) {
        applyPreset(m_presets.at(static_cast<size_t>(row - 1)));
    }
}

void ConverterSetupDialog::savePreset()
{
    QString initialName;
    if(m_presetList->currentRow() > 0) {
        initialName = m_presetList->currentItem()->text();
    }

    bool accepted{false};
    const QString name = QInputDialog::getText(this, tr("Save Converter Preset"), tr("Name") + ":"_L1,
                                               QLineEdit::Normal, initialName, &accepted)
                             .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    StoredConversionPreset stored;
    QString presetId;

    auto existing = std::ranges::find(m_presets, name, &StoredConversionPreset::name);
    if(existing != m_presets.end()) {
        stored   = *existing;
        presetId = existing->preset.id;
    }
    else {
        presetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    stored.name        = name;
    stored.preset      = job().preset;
    stored.preset.id   = presetId;
    stored.preset.name = name;
    stored.showReport  = m_showReport->isChecked();

    if(existing != m_presets.end()) {
        *existing = std::move(stored);
    }
    else {
        m_presets.push_back(std::move(stored));
    }

    savePresets();
    populatePresets();

    for(int row{1}; row < m_presetList->count(); ++row) {
        if(m_presetList->item(row)->text() == name) {
            m_presetList->setCurrentRow(row);
            break;
        }
    }
}

void ConverterSetupDialog::removePreset()
{
    const int row = m_presetList->currentRow();
    if(row <= 0 || static_cast<size_t>(row - 1) >= m_presets.size()) {
        return;
    }

    m_presets.erase(m_presets.begin() + (row - 1));
    savePresets();
    populatePresets();
}

void ConverterSetupDialog::importPresets()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Converter Presets"), {},
                                                      tr("fooyin Converter Presets (*.fycp)"), nullptr,
                                                      QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    QFile file{path};
    if(!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import Converter Presets"), tr("The preset file could not be opened."));
        return;
    }

    auto imported = ConverterSettings::deserialiseConversionPresets(file.readAll());
    if(imported.empty()) {
        QMessageBox::warning(this, tr("Import Converter Presets"), tr("The preset file is invalid or empty."));
        return;
    }

    for(const StoredConversionPreset& stored : imported) {
        const bool encoderAvailable = std::ranges::any_of(m_runtimeEncoders, [&stored](const AudioEncoderInfo& info) {
            return info.id == stored.preset.encoder.profile.id;
        });
        if(!encoderAvailable) {
            QMessageBox::warning(this, tr("Import Converter Presets"),
                                 tr("Encoder is unavailable for preset: %1").arg(stored.name));
            return;
        }
    }

    QString selectedId;

    for(StoredConversionPreset& stored : imported) {
        if(stored.preset.id.isEmpty()) {
            stored.preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        stored.preset.name = stored.name;
        selectedId         = stored.preset.id;

        auto existing = std::ranges::find_if(m_presets, [&stored](const StoredConversionPreset& candidate) {
            return candidate.preset.id == stored.preset.id || candidate.name == stored.name;
        });
        if(existing != m_presets.end()) {
            *existing = std::move(stored);
        }
        else {
            m_presets.push_back(std::move(stored));
        }
    }

    savePresets();
    populatePresets();

    for(int row{1}; row < m_presetList->count(); ++row) {
        if(m_presets.at(row - 1).preset.id == selectedId) {
            m_presetList->setCurrentRow(row);
            break;
        }
    }
}

void ConverterSetupDialog::exportPreset()
{
    const int row = m_presetList->currentRow();
    if(row <= 0 || std::cmp_greater_equal(row - 1, m_presets.size())) {
        return;
    }

    const StoredConversionPreset& preset = m_presets.at(static_cast<size_t>(row - 1));
    const QString suggested              = QDir::home().filePath(preset.name + u".fycp"_s);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Converter Preset"), suggested,
                                                      tr("fooyin Converter Presets (*.fycp)"), nullptr,
                                                      QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    QSaveFile file{path};
    const QByteArray presetData = ConverterSettings::serialiseConversionPresets({preset});
    if(!file.open(QIODevice::WriteOnly) || file.write(presetData) != presetData.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Export Converter Preset"), tr("The preset file could not be written."));
    }
}

void ConverterSetupDialog::applyPreset(const StoredConversionPreset& stored)
{
    const EncoderProfile& profile = stored.preset.encoder.profile;

    int profileRow{-1};
    for(int row{0}; row < m_profiles.size(); ++row) {
        const EncoderProfile& candidate = m_profiles.at(row).info.profile;
        if(candidate.id == profile.id && candidate.name == profile.name && candidate.mode == profile.mode
           && candidate.bitrateKbps == profile.bitrateKbps && candidate.quality == profile.quality
           && candidate.compressionLevel == profile.compressionLevel) {
            profileRow = row;
            break;
        }
    }

    if(profileRow < 0) {
        const auto runtime = std::ranges::find_if(
            m_runtimeEncoders, [&profile](const AudioEncoderInfo& info) { return info.id == profile.id; });
        if(runtime != m_runtimeEncoders.end()) {
            AudioEncoderInfo info         = *runtime;
            info.name                     = profile.name;
            info.profile.name             = profile.name;
            info.profile.mode             = profile.mode;
            info.profile.bitrateKbps      = profile.bitrateKbps;
            info.profile.quality          = profile.quality;
            info.profile.compressionLevel = profile.compressionLevel;

            if(const QString description = EncoderProfileTableModel::profileDescription(info.profile);
               !description.isEmpty()) {
                info.description = description;
            }

            m_profiles.emplace_back(std::move(info), QString{}, false, false);
            profileRow = static_cast<int>(m_profiles.size()) - 1;
            refreshProfileTable(profileRow);
        }
    }
    else {
        m_profileTable->selectRow(profileRow);
    }

    const auto setComboValue = [](QComboBox* combo, int value) {
        const int index = combo->findData(value);
        if(index >= 0) {
            combo->setCurrentIndex(index);
        }
    };

    setComboValue(m_sampleFormat, static_cast<int>(stored.preset.encoder.outputSampleFormat));
    setComboValue(m_dither, static_cast<int>(stored.preset.encoder.ditherMode));
    setComboValue(m_destinationMode, static_cast<int>(stored.preset.destination.mode));
    setComboValue(m_existingFileMode, static_cast<int>(stored.preset.destination.existingFileMode));
    setComboValue(m_outputStyle, static_cast<int>(stored.preset.destination.outputStyle));
    setComboValue(m_replayGainMode, static_cast<int>(stored.preset.processing.replayGainMode));

    m_folder->setText(stored.preset.destination.fixedFolder);
    m_filenamePattern->setText(stored.preset.destination.filenamePattern);
    m_transferMetadata->setChecked(stored.preset.processing.transferMetadata);
    m_transferRating->setChecked(stored.preset.processing.transferRating);
    m_transferPlaycount->setChecked(stored.preset.processing.transferPlaycount);
    m_transferPictures->setChecked(stored.preset.processing.transferPictures);
    m_replayGainPreventClipping->setChecked(stored.preset.processing.replayGainPreventClipping);
    m_replayGainPreamp->setValue(stored.preset.processing.replayGainPreampDb);
    m_replayGainWithoutInfoPreamp->setValue(stored.preset.processing.replayGainWithoutInfoPreampDb);
    m_dspChain = stored.preset.processing.dspChain;
    m_preserveDspState->setChecked(stored.preset.processing.preserveDspStateBetweenTracks);
    refreshDspModels();
    m_generatePreview->setChecked(stored.preset.preview.enabled);
    m_previewPercentage->setValue(stored.preset.preview.lengthPercentage);
    m_copyFilesPattern->setText(stored.preset.other.copyFilesPattern);
    m_verifyOutput->setChecked(stored.preset.other.verifyOutput);
    m_showReport->setChecked(stored.showReport);

    updateState();
}

void ConverterSetupDialog::applyDefaultPreset()
{
    const auto defaultProfile = std::ranges::find_if(m_profiles, [](const EncoderProfileEntry& entry) {
        return entry.info.id == QLatin1StringView{ConverterSettings::DefaultEncoderProfileId};
    });
    if(defaultProfile != m_profiles.cend()) {
        m_profileTable->selectRow(static_cast<int>(std::ranges::distance(m_profiles.cbegin(), defaultProfile)));
    }
    else {
        m_profileTable->setCurrentIndex({});
    }

    m_sampleFormat->setCurrentIndex(m_sampleFormat->findData(static_cast<int>(SampleFormat::Unknown)));
    m_dither->setCurrentIndex(m_dither->findData(static_cast<int>(DitherMode::Never)));
    m_destinationMode->setCurrentIndex(m_destinationMode->findData(static_cast<int>(DestinationMode::Ask)));
    m_existingFileMode->setCurrentIndex(m_existingFileMode->findData(static_cast<int>(ExistingFileMode::Ask)));
    m_outputStyle->setCurrentIndex(m_outputStyle->findData(static_cast<int>(OutputStyle::IndividualFiles)));
    m_folder->clear();
    m_filenamePattern->setText(u"%title%"_s);
    m_transferMetadata->setChecked(true);
    m_transferRating->setChecked(true);
    m_transferPlaycount->setChecked(false);
    m_transferPictures->setChecked(true);
    m_replayGainMode->setCurrentIndex(m_replayGainMode->findData(static_cast<int>(ConversionReplayGainMode::None)));
    m_replayGainPreventClipping->setChecked(true);
    m_replayGainPreamp->setValue(0.0);
    m_replayGainWithoutInfoPreamp->setValue(0.0);
    m_preserveDspState->setChecked(false);
    m_dspChain.clear();
    refreshDspModels();
    m_generatePreview->setChecked(false);
    m_previewPercentage->setValue(20);
    m_copyFilesPattern->clear();
    m_verifyOutput->setChecked(false);
    m_showReport->setChecked(true);
    updateState();
}

void ConverterSetupDialog::savePresets() const
{
    ConverterSettings::setConversionPresets(m_presets);
}

void ConverterSetupDialog::browseForFolder()
{
    const QString start  = m_folder->text().isEmpty() ? QDir::homePath() : m_folder->text();
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Choose destination"), start);
    if(!folder.isEmpty()) {
        m_folder->setText(folder);
    }
}

void ConverterSetupDialog::updateState()
{
    const bool fixedFolder = destinationMode() == DestinationMode::FixedFolder;
    m_folder->setEnabled(fixedFolder);
    m_browse->setEnabled(fixedFolder);

    const AudioEncoderInfo* encoder = selectedEncoder();

    const bool individualOutput
        = static_cast<OutputStyle>(m_outputStyle->currentData().toInt()) == OutputStyle::IndividualFiles;
    m_transferRating->setEnabled(individualOutput);
    m_transferPlaycount->setEnabled(individualOutput);

    const bool supportsPictures = encoder && encoder->supportsPictures;
    m_transferPictures->setEnabled(supportsPictures);

    const bool applyReplayGain = static_cast<ConversionReplayGainMode>(m_replayGainMode->currentData().toInt())
                              != ConversionReplayGainMode::None;
    m_replayGainPreventClipping->setEnabled(applyReplayGain);
    m_replayGainPreamp->setEnabled(applyReplayGain);
    m_replayGainWithoutInfoPreamp->setEnabled(applyReplayGain);

    m_previewPercentage->setEnabled(m_generatePreview->isChecked());
    m_editProfile->setEnabled(encoder);
    m_profileTable->addRowAction()->setEnabled(!m_runtimeEncoders.empty());

    const int profileRow       = m_profileTable->currentIndex().row();
    const bool validProfileRow = profileRow >= 0 && std::cmp_less(profileRow, m_profiles.size());
    const bool selectedBuiltIn = validProfileRow && m_profiles.at(profileRow).builtIn;
    const bool canResetBuiltIn = selectedBuiltIn && m_profiles.at(profileRow).persisted;
    m_profileTable->removeRowAction()->setText(selectedBuiltIn ? tr("Reset") : tr("Remove"));
    m_profileTable->removeRowAction()->setEnabled(validProfileRow && (!selectedBuiltIn || canResetBuiltIn));

    const bool validDestination = !fixedFolder || !m_folder->text().trimmed().isEmpty();
    m_convert->setEnabled(encoder && validDestination && !m_filenamePattern->text().trimmed().isEmpty()
                          && !m_tracks.empty());

    updatePreview();
    updateOverview();
}

void ConverterSetupDialog::updatePreview()
{
    m_preview->clear();

    const AudioEncoderInfo* encoder = selectedEncoder();
    if(!encoder) {
        return;
    }

    ConversionPathResolver::Request request;
    request.tracks                       = m_tracks;
    request.extension                    = encoder->profile.extension;
    request.destination.mode             = destinationMode();
    request.destination.fixedFolder      = m_folder->text().trimmed();
    request.destination.filenamePattern  = m_filenamePattern->text().trimmed();
    request.destination.existingFileMode = ExistingFileMode::Overwrite;
    request.destination.outputStyle      = static_cast<OutputStyle>(m_outputStyle->currentData().toInt());
    request.askFolder                    = QDir::homePath();

    const auto paths   = Fooyin::ConversionPathResolver::resolve(request);
    const size_t count = std::min<size_t>(paths.size(), 20);
    for(size_t i{0}; i < count; ++i) {
        const QString filename = QFileInfo{paths.at(i).outputPath}.fileName();
        m_preview->addItem(filename.isEmpty() ? paths.at(i).error : filename);
    }
}

void ConverterSetupDialog::updateOverview()
{
    const AudioEncoderInfo* encoder = selectedEncoder();

    m_outputSummary->setText(
        encoder ? encoder->name + (encoder->description.isEmpty() ? QString{} : u", "_s + encoder->description)
                : tr("No encoder available"));

    QStringList destinationParts;

    switch(destinationMode()) {
        case DestinationMode::Ask:
            destinationParts.push_back(tr("Ask when conversion starts"));
            break;
        case DestinationMode::SourceFolder:
            destinationParts.push_back(tr("Source track folder"));
            break;
        case DestinationMode::FixedFolder: {
            const QString folder = m_folder->text().trimmed();
            destinationParts.push_back(folder.isEmpty() ? tr("No folder specified") : folder);
            break;
        }
    }
    switch(static_cast<OutputStyle>(m_outputStyle->currentData().toInt())) {
        case OutputStyle::IndividualFiles:
            break;
        case OutputStyle::MultiTrackFiles:
            destinationParts.push_back(tr("group tracks by output name"));
            break;
        case OutputStyle::MergeTracks:
            destinationParts.push_back(tr("merge tracks"));
            break;
    }
    const QString filenamePattern = m_filenamePattern->text().trimmed();
    destinationParts.push_back(filenamePattern.isEmpty() ? tr("No name format") : filenamePattern);

    m_destinationSummary->setText(destinationParts.join(u", "_s));

    QStringList processing;

    if(m_transferMetadata->isChecked()) {
        processing.push_back(tr("metadata"));
    }
    if(m_transferRating->isChecked() && m_transferRating->isEnabled()) {
        processing.push_back(tr("rating"));
    }
    if(m_transferPlaycount->isChecked() && m_transferPlaycount->isEnabled()) {
        processing.push_back(tr("play count"));
    }
    if(m_transferPictures->isChecked() && m_transferPictures->isEnabled()) {
        processing.push_back(tr("attached pictures"));
    }

    switch(static_cast<ConversionReplayGainMode>(m_replayGainMode->currentData().toInt())) {
        case ConversionReplayGainMode::Track:
            processing.push_back(tr("ReplayGain (track)"));
            break;
        case ConversionReplayGainMode::Album:
            processing.push_back(tr("ReplayGain (album)"));
            break;
        case ConversionReplayGainMode::None:
            break;
    }

    if(!m_dspChain.empty()) {
        processing.push_back(tr("%Ln DSP(s)", nullptr, static_cast<int>(m_dspChain.size())));
        if(m_preserveDspState->isChecked()) {
            processing.push_back(tr("continuous DSP"));
        }
    }

    m_processingSummary->setText(processing.isEmpty() ? tr("None") : processing.join(u", "_s));

    QStringList other;

    if(m_generatePreview->isChecked()) {
        other.push_back(tr("%1% previews").arg(m_previewPercentage->value()));
    }
    if(m_showReport->isChecked()) {
        other.push_back(tr("Show status report"));
    }
    if(!m_copyFilesPattern->text().trimmed().isEmpty()) {
        other.push_back(tr("Copy matching files"));
    }
    if(m_verifyOutput->isChecked()) {
        other.push_back(tr("Verify output"));
    }

    m_otherSummary->setText(other.isEmpty() ? tr("None") : other.join(u", "_s));
}

const AudioEncoderInfo* ConverterSetupDialog::selectedEncoder() const
{
    const int row = m_profileTable->currentIndex().row();
    return row >= 0 && std::cmp_less(row, m_profiles.size()) ? &m_profiles.at(row).info : nullptr;
}

DestinationMode ConverterSetupDialog::destinationMode() const
{
    return static_cast<DestinationMode>(m_destinationMode->currentData().toInt());
}
} // namespace Fooyin

#include "moc_convertersetupdialog.cpp"
