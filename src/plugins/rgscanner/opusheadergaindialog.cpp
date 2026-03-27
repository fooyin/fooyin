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

#include "opusheadergaindialog.h"

#include "opusreplaygainutils.h"
#include "rgscannerdefs.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpacerItem>
#include <QtConcurrentRun>

#include <atomic>
#include <limits>
#include <optional>

using namespace Qt::StringLiterals;

constexpr auto MinTargetVolumeDb = 70;
constexpr auto MaxTargetVolumeDb = 110;

namespace Fooyin::RGScanner {
namespace {
struct OpusHeaderGainBatchResult
{
    TrackList updatedTracks;
    QStringList failedFiles;
    int unchanged{0};
    bool cancelled{false};
};

struct OpusHeaderGainAvailability
{
    int missingTrackGain{0};
    int missingAlbumGain{0};

    [[nodiscard]] bool hasTrackGainForAll() const
    {
        return missingTrackGain == 0;
    }

    [[nodiscard]] bool hasAlbumGainForAll() const
    {
        return missingAlbumGain == 0;
    }
};

QString dialogText(const char* sourceText)
{
    return QCoreApplication::translate("OpusHeaderGainDialog", sourceText);
}

OpusHeaderGainAvailability analyseSelection(const TrackList& tracks)
{
    OpusHeaderGainAvailability availability;

    for(const Track& track : tracks) {
        if(!track.hasTrackGain()) {
            ++availability.missingTrackGain;
        }
        if(!track.hasAlbumGain()) {
            ++availability.missingAlbumGain;
        }
    }

    return availability;
}

Track updatedTrackFromState(const Track& originalTrack, const OpusGainState& state, bool storeZeroHeaderGain = false)
{
    Track updatedTrack{originalTrack};
    removeReplayGainTags(updatedTrack);
    updatedTrack.clearOpusHeaderGain();

    if(state.trackGain.has_value()) {
        updatedTrack.setRGTrackGain(q78ToDb(*state.trackGain) + 5.0F);
    }
    if(state.albumGain.has_value()) {
        updatedTrack.setRGAlbumGain(q78ToDb(*state.albumGain) + 5.0F);
    }
    if(storeZeroHeaderGain || state.headerGain != 0) {
        updatedTrack.setOpusHeaderGainQ78(state.headerGain);
    }

    const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
    updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    return updatedTrack;
}

bool updateOpusHeaderGainForTrack(AudioLoader* audioLoader, const Track& track, OpusHeaderGainMode mode,
                                  std::optional<int16_t> explicitGain, const OpusHeaderGainOptions& headerOptions,
                                  bool preserveTimestamps, Track& updatedTrack)
{
    if(!audioLoader) {
        return false;
    }

    const OpusGainState currentState{
        .headerGain = track.opusHeaderGainQ78().value_or(0),
        .trackGain  = track.hasTrackGain() ? replayGainToOpusR128Q78(track.rgTrackGain()) : std::optional<int16_t>{},
        .albumGain  = track.hasAlbumGain() ? replayGainToOpusR128Q78(track.rgAlbumGain()) : std::optional<int16_t>{},
    };

    const auto updatedState = applyHeaderGainMode(currentState, mode, explicitGain, headerOptions);
    if(!updatedState.has_value()) {
        return false;
    }
    if(*updatedState == currentState) {
        updatedTrack = Track{};
        return true;
    }

    const Track trackToWrite = updatedTrackFromState(track, *updatedState, true);

    AudioReader::WriteOptions options{AudioReader::Metadata};
    if(preserveTimestamps) {
        options |= AudioReader::PreserveTimestamps;
    }

    if(!audioLoader->writeTrackMetadata(trackToWrite, options)) {
        return false;
    }

    updatedTrack                 = updatedTrackFromState(track, *updatedState);
    const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
    updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    return true;
}

class OpusHeaderGainDialog : public QDialog
{
public:
    OpusHeaderGainDialog(std::shared_ptr<AudioLoader> audioLoader, MusicLibrary* library, SettingsManager* settings,
                         TrackList tracks, QWidget* parent = nullptr)
        : QDialog(parent)
        , m_audioLoader(std::move(audioLoader))
        , m_library(library)
        , m_settings(settings)
        , m_tracks(std::move(tracks))
        , m_availability(analyseSelection(m_tracks))
        , m_applyTrack(new QRadioButton(dialogText("Apply track ReplayGain"), this))
        , m_applyAlbum(new QRadioButton(dialogText("Apply album ReplayGain"), this))
        , m_setExplicit(new QRadioButton(dialogText("Specify header gain"), this))
        , m_clearHeader(new QRadioButton(dialogText("Clear header gain"), this))
        , m_explicitGain(new QDoubleSpinBox(this))
        , m_targetVolume(new SliderEditor(dialogText("Target volume"), this))
        , m_adjustLouderOrQuieter(new QRadioButton(dialogText("Make files louder or quieter"), this))
        , m_adjustLouderOnly(new QRadioButton(dialogText("Make files louder only, do not change if too loud"), this))
        , m_summary(new QLabel(this))
        , m_currentFile(new QLabel(this))
        , m_progress(new QProgressBar(this))
        , m_buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
        , m_watcher(new QFutureWatcher<OpusHeaderGainBatchResult>(this))
        , m_cancelled(std::make_shared<std::atomic_bool>(false))
    {
        setModal(true);
        setWindowTitle(dialogText("Opus Header Gain"));

        auto* modeGroup = new QButtonGroup(this);
        modeGroup->addButton(m_applyTrack);
        modeGroup->addButton(m_applyAlbum);
        modeGroup->addButton(m_setExplicit);
        modeGroup->addButton(m_clearHeader);

        auto* adjustmentGroup = new QButtonGroup(this);
        m_adjustLouderOrQuieter->setAutoExclusive(false);
        m_adjustLouderOnly->setAutoExclusive(false);
        adjustmentGroup->addButton(m_adjustLouderOrQuieter);
        adjustmentGroup->addButton(m_adjustLouderOnly);

        m_explicitGain->setRange(-128.0, 127.99);
        m_explicitGain->setDecimals(2);
        m_explicitGain->setSingleStep(0.10);

        m_targetVolume->setRange(MinTargetVolumeDb, MaxTargetVolumeDb);
        m_targetVolume->setSingleStep(1);
        m_targetVolume->setSuffix(u" dB"_s);
        m_targetVolume->setValue(
            m_settings->fileValue(OpusHeaderTargetVolumeSetting, static_cast<int>(ReplayGainReferenceDb)).toInt());
        m_adjustLouderOnly->setChecked(m_settings->fileValue(OpusHeaderLouderOnlySetting, false).toBool());
        m_adjustLouderOrQuieter->setChecked(!m_adjustLouderOnly->isChecked());

        m_summary->setText(dialogText("%1 writable Opus file(s) selected").arg(m_tracks.size()));
        m_currentFile->setText(dialogText("Ready"));
        m_progress->setRange(0, static_cast<int>(m_tracks.size()));
        m_progress->setValue(0);

        if(m_availability.hasTrackGainForAll()) {
            m_applyTrack->setChecked(true);
        }
        else if(m_availability.hasAlbumGainForAll()) {
            m_applyAlbum->setChecked(true);
        }
        else {
            m_setExplicit->setChecked(true);
        }

        auto* layout = new QGridLayout(this);

        int row{0};
        layout->addWidget(m_applyTrack, row++, 0, 1, 4);
        layout->addWidget(m_applyAlbum, row++, 0, 1, 4);
        layout->addWidget(m_setExplicit, row, 0);
        layout->addWidget(m_explicitGain, row, 1);
        layout->addWidget(new QLabel(u"dB"_s, this), row++, 2);
        layout->addWidget(m_clearHeader, row++, 0, 1, 4);
        layout->addItem(new QSpacerItem(0, 16), row++, 0);
        layout->addWidget(m_targetVolume, row++, 0, 1, 4);
        layout->addWidget(m_adjustLouderOrQuieter, row++, 0, 1, 4);
        layout->addWidget(m_adjustLouderOnly, row++, 0, 1, 4);
        layout->addItem(new QSpacerItem(0, 16), row++, 0);
        layout->addWidget(m_summary, row++, 0, 1, 4);
        layout->addWidget(m_currentFile, row++, 0, 1, 4);
        layout->addWidget(m_progress, row++, 0, 1, 4);
        layout->addWidget(m_buttonBox, row++, 0, 1, 4);
        layout->setColumnStretch(1, 0);
        layout->setColumnStretch(2, 0);
        layout->setColumnStretch(3, 1);

        m_buttonBox->button(QDialogButtonBox::Ok)->setText(dialogText("Start"));
        m_buttonBox->button(QDialogButtonBox::Cancel)->setText(dialogText("Cancel"));

        QObject::connect(m_applyTrack, &QRadioButton::toggled, this, [this]() { refreshControls(); });
        QObject::connect(m_applyAlbum, &QRadioButton::toggled, this, [this]() { refreshControls(); });
        QObject::connect(m_setExplicit, &QRadioButton::toggled, this, [this]() { refreshControls(); });
        QObject::connect(m_clearHeader, &QRadioButton::toggled, this, [this]() { refreshControls(); });

        QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
            if(!m_running) {
                start();
            }
        });
        QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, this, [this]() {
            if(m_running) {
                m_cancelled->store(true, std::memory_order_release);
                m_summary->setText(dialogText("Cancelling after the current file…"));
            }
            else {
                reject();
            }
        });

        QObject::connect(m_watcher, &QFutureWatcher<OpusHeaderGainBatchResult>::finished, this, [this]() {
            m_running = false;

            const auto result = m_watcher->result();
            if(!result.failedFiles.isEmpty()) {
                QMessageBox::warning(this, windowTitle(),
                                     dialogText("Failed to update %1 file(s).\n\n%2")
                                         .arg(result.failedFiles.size())
                                         .arg(result.failedFiles.join(u'\n')));
            }

            if(!result.updatedTracks.empty()) {
                m_summary->setText(dialogText("Updating library metadata…"));
                QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, this, &QDialog::accept,
                                 Qt::SingleShotConnection);
                m_library->updateTrackMetadata(result.updatedTracks);
                return;
            }

            if(result.cancelled) {
                m_summary->setText(dialogText("Cancelled"));
            }
            else if(result.unchanged > 0) {
                m_summary->setText(dialogText("No Opus header gain changes were needed"));
            }
            else {
                m_summary->setText(dialogText("No files were updated"));
            }

            m_buttonBox->button(QDialogButtonBox::Cancel)->setText(dialogText("Close"));
        });

        refreshControls();
    }

private:
    [[nodiscard]] OpusHeaderGainMode selectedMode() const
    {
        return m_applyAlbum->isChecked()  ? OpusHeaderGainMode::Album
             : m_setExplicit->isChecked() ? OpusHeaderGainMode::Explicit
             : m_clearHeader->isChecked() ? OpusHeaderGainMode::Clear
                                          : OpusHeaderGainMode::Track;
    }

    void refreshControls()
    {
        if(m_running) {
            return;
        }

        const bool canApplyTrack = m_availability.hasTrackGainForAll();
        const bool canApplyAlbum = m_availability.hasAlbumGainForAll();

        if(m_applyTrack->isChecked() && !canApplyTrack) {
            if(canApplyAlbum) {
                m_applyAlbum->setChecked(true);
            }
            else {
                m_setExplicit->setChecked(true);
            }
        }
        if(m_applyAlbum->isChecked() && !canApplyAlbum) {
            if(canApplyTrack) {
                m_applyTrack->setChecked(true);
            }
            else {
                m_setExplicit->setChecked(true);
            }
        }

        m_applyTrack->setEnabled(canApplyTrack);
        m_applyAlbum->setEnabled(canApplyAlbum);

        const bool applyReplayGainMode
            = selectedMode() == OpusHeaderGainMode::Track || selectedMode() == OpusHeaderGainMode::Album;

        m_explicitGain->setEnabled(m_setExplicit->isChecked());

        m_targetVolume->setEnabled(applyReplayGainMode);
        m_adjustLouderOrQuieter->setEnabled(applyReplayGainMode);
        m_adjustLouderOnly->setEnabled(applyReplayGainMode);
    }

    void persistOptions() const
    {
        m_settings->fileSet(OpusHeaderTargetVolumeSetting, m_targetVolume->value());
        m_settings->fileSet(OpusHeaderLouderOnlySetting, m_adjustLouderOnly->isChecked());
    }

    void start()
    {
        m_cancelled->store(false, std::memory_order_release);

        const OpusHeaderGainMode mode = selectedMode();

        const auto explicitGain
            = mode == OpusHeaderGainMode::Explicit ? dbToQ78(m_explicitGain->value()) : std::optional<int16_t>{};
        if(mode == OpusHeaderGainMode::Explicit && !explicitGain.has_value()) {
            QMessageBox::warning(this, windowTitle(), dialogText("The requested header gain is out of range."));
            return;
        }

        const OpusHeaderGainOptions headerOptions{
            .targetVolumeDb = static_cast<double>(m_targetVolume->value()),
            .louderOnly     = m_adjustLouderOnly->isChecked(),
        };
        persistOptions();

        m_running = true;
        m_progress->setValue(0);
        m_summary->setText(dialogText("Updating Opus header gain…"));
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        m_applyTrack->setEnabled(false);
        m_applyAlbum->setEnabled(false);
        m_setExplicit->setEnabled(false);
        m_clearHeader->setEnabled(false);
        m_explicitGain->setEnabled(false);
        m_targetVolume->setEnabled(false);
        m_adjustLouderOrQuieter->setEnabled(false);
        m_adjustLouderOnly->setEnabled(false);

        const bool preserveTimestamps = m_settings->value<Settings::Core::PreserveTimestamps>();
        const auto tracks             = m_tracks;
        const auto cancel             = m_cancelled;

        m_watcher->setFuture(
            QtConcurrent::run([this, tracks, cancel, mode, explicitGain, headerOptions, preserveTimestamps]() {
                OpusHeaderGainBatchResult result;

                for(size_t i{0}; i < tracks.size(); ++i) {
                    if(cancel->load(std::memory_order_acquire)) {
                        result.cancelled = true;
                        break;
                    }

                    const Track& track = tracks.at(i);
                    QMetaObject::invokeMethod(this, [this, i, filepath = track.prettyFilepath()]() {
                        m_progress->setValue(static_cast<int>(i));
                        m_currentFile->setText(filepath);
                    });

                    Track updatedTrack;
                    if(!updateOpusHeaderGainForTrack(m_audioLoader.get(), track, mode, explicitGain, headerOptions,
                                                     preserveTimestamps, updatedTrack)) {
                        result.failedFiles.emplace_back(track.prettyFilepath());
                        continue;
                    }

                    if(updatedTrack.filepath().isEmpty()) {
                        ++result.unchanged;
                        continue;
                    }

                    result.updatedTracks.emplace_back(std::move(updatedTrack));
                }

                QMetaObject::invokeMethod(
                    this, [this]() { m_progress->setValue(m_progress->maximum()); }, Qt::QueuedConnection);
                return result;
            }));
    }

    std::shared_ptr<AudioLoader> m_audioLoader;
    MusicLibrary* m_library;
    SettingsManager* m_settings;
    TrackList m_tracks;
    OpusHeaderGainAvailability m_availability;

    QRadioButton* m_applyTrack;
    QRadioButton* m_applyAlbum;
    QRadioButton* m_setExplicit;
    QRadioButton* m_clearHeader;
    QDoubleSpinBox* m_explicitGain;
    SliderEditor* m_targetVolume;
    QRadioButton* m_adjustLouderOrQuieter;
    QRadioButton* m_adjustLouderOnly;
    QLabel* m_summary;
    QLabel* m_currentFile;
    QProgressBar* m_progress;
    QDialogButtonBox* m_buttonBox;
    QFutureWatcher<OpusHeaderGainBatchResult>* m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    bool m_running{false};
};
} // namespace

QDialog* createOpusHeaderGainDialog(std::shared_ptr<AudioLoader> audioLoader, MusicLibrary* library,
                                    SettingsManager* settings, TrackList tracks, QWidget* parent)
{
    return new OpusHeaderGainDialog(std::move(audioLoader), library, settings, std::move(tracks), parent);
}
} // namespace Fooyin::RGScanner
