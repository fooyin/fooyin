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

#include "conversioncontroller.h"

#include "convertersettingsstore.h"
#include "convertersetupdialog.h"

#include <core/engine/audioloader.h>
#include <core/engine/conversion/conversionrunner.h>
#include <core/library/libraryscanutils.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/async.h>

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <atomic>
#include <chrono>
#include <future>
#include <set>

Q_LOGGING_CATEGORY(CONV_CTRL, "fy.conversioncontroller")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct ConversionCounts
{
    int succeeded{0};
    int skipped{0};
    int failed{0};
    int cancelled{0};
};

QString conversionStatus(ConversionResultStatus status, bool verifyOutput)
{
    switch(status) {
        case ConversionResultStatus::Succeeded:
            return verifyOutput ? ConversionController::tr("Converted and verified")
                                : ConversionController::tr("Converted");
        case ConversionResultStatus::Skipped:
            return ConversionController::tr("Skipped");
        case ConversionResultStatus::Failed:
            return ConversionController::tr("Failed");
        case ConversionResultStatus::Cancelled:
            return ConversionController::tr("Cancelled");
    }
    return {};
}

QString conversionName(const Track& track)
{
    return !track.effectiveTitle().isEmpty() ? track.effectiveTitle() : QFileInfo{track.filepath()}.fileName();
}

ConversionCounts conversionCounts(const std::vector<ConversionTrackResult>& results)
{
    ConversionCounts counts;
    for(const auto& result : results) {
        switch(result.status) {
            case ConversionResultStatus::Succeeded:
                ++counts.succeeded;
                break;
            case ConversionResultStatus::Skipped:
                ++counts.skipped;
                break;
            case ConversionResultStatus::Failed:
                ++counts.failed;
                break;
            case ConversionResultStatus::Cancelled:
                ++counts.cancelled;
                break;
        }
    }
    return counts;
}

void showConversionResults(QWidget* parent, const std::vector<ConversionTrackResult>& results,
                           const ConversionCounts& counts, bool showDetails, bool verifyOutput)
{
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(ConversionController::tr("Audio Conversion Results"));
    dialog->resize(900, 440);

    auto* table = new QTableWidget(static_cast<int>(results.size()), showDetails ? 4 : 3, dialog);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);

    QStringList headers{ConversionController::tr("Name"), ConversionController::tr("Status"),
                        ConversionController::tr("Output")};
    if(showDetails) {
        headers.push_back(ConversionController::tr("Details"));
    }
    table->setHorizontalHeaderLabels(headers);

    for(int row{0}; std::cmp_less(row, results.size()); ++row) {
        const auto& result  = results.at(row);
        QStringList details = result.warnings;
        if(!result.error.isEmpty()) {
            details.push_front(result.error);
        }

        auto* nameItem = new QTableWidgetItem(conversionName(result.sourceTrack));
        nameItem->setToolTip(result.sourceTrack.prettyFilepath());
        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, new QTableWidgetItem(conversionStatus(result.status, verifyOutput)));

        auto* outputItem = new QTableWidgetItem(QDir::toNativeSeparators(result.outputPath));
        outputItem->setToolTip(QDir::toNativeSeparators(result.outputPath));
        table->setItem(row, 2, outputItem);

        if(showDetails) {
            auto* detailsItem = new QTableWidgetItem(details.join(u"; "_s));
            detailsItem->setToolTip(details.join(u'\n'));
            table->setItem(row, 3, detailsItem);
        }
    }

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    if(showDetails) {
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    }

    auto* summary = new QLabel(ConversionController::tr("Converted: %1 | Skipped: %2 | Failed: %3 | Cancelled: %4")
                                   .arg(counts.succeeded)
                                   .arg(counts.skipped)
                                   .arg(counts.failed)
                                   .arg(counts.cancelled),
                               dialog);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    auto* footer = new QHBoxLayout();
    footer->addWidget(summary, 1);
    footer->addWidget(buttons);

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(table);
    layout->addLayout(footer);

    dialog->show();
}

class ConversionSession : public QObject
{
    Q_OBJECT

public:
    ConversionSession(std::shared_ptr<AudioLoader> audioLoader, AudioEncoderRegistry* encoderRegistry,
                      DspRegistry* dspRegistry, ConversionJob job, QString askFolder, bool showReport,
                      bool showOutputFiles, std::shared_ptr<ConversionInputObserver> sourceObserver,
                      std::function<void(const std::vector<ConversionTrackResult>&)> completion, QWidget* parentWindow,
                      QObject* parent)
        : QObject{parent}
        , m_audioLoader{std::move(audioLoader)}
        , m_encoderRegistry{encoderRegistry}
        , m_dspRegistry{dspRegistry}
        , m_job{std::move(job)}
        , m_askFolder{std::move(askFolder)}
        , m_showReport{showReport}
        , m_showOutputFiles{showOutputFiles}
        , m_sourceObserver{std::move(sourceObserver)}
        , m_completion{std::move(completion)}
        , m_parentWindow{parentWindow}
        , m_cancelled{std::make_shared<std::atomic_bool>(false)}
        , m_progress{new ElapsedProgressDialog(tr("Preparing conversion…"), tr("Cancel"), 0, 100, parentWindow)}
        , m_watcher{new QFutureWatcher<std::vector<ConversionTrackResult>>(this)}
    {
        m_progress->setAttribute(Qt::WA_DeleteOnClose, false);
        m_progress->setWindowTitle(tr("Audio Conversion"));
        m_progress->setModal(false);
        m_progress->setMinimumDuration(250ms);
        m_progress->startTimer();
        m_progress->setValue(0);

        QObject::connect(m_progress, &ElapsedProgressDialog::cancelled, m_progress,
                         [cancelled = m_cancelled]() { cancelled->store(true, std::memory_order_release); });
        QObject::connect(m_progress, &QDialog::rejected, m_progress,
                         [cancelled = m_cancelled]() { cancelled->store(true, std::memory_order_release); });
        QObject::connect(m_watcher, &QFutureWatcherBase::finished, this, &ConversionSession::finish);
    }

    void start()
    {
        auto future = Utils::asyncExec([audioLoader = m_audioLoader, encoderRegistry = m_encoderRegistry,
                                        dspRegistry = m_dspRegistry, job = m_job, askFolder = m_askFolder,
                                        cancelled = m_cancelled, progress = m_progress, session = this,
                                        sourceObserver = m_sourceObserver]() {
            ConversionRunner::Request request;
            request.audioLoader     = audioLoader.get();
            request.encoderRegistry = encoderRegistry;
            request.dspRegistry     = dspRegistry;
            request.job             = job;
            request.sourceObserver  = sourceObserver;
            request.askFolder       = askFolder;
            request.cancelCallback  = [cancelled]() {
                return cancelled->load(std::memory_order_acquire);
            };
            request.progressCallback = [progress](const ConversionProgress& current) {
                const int trackCount       = std::max(1, current.trackCount);
                const double trackProgress = current.sourceDurationMs > 0
                                               ? std::clamp(static_cast<double>(current.sourcePositionMs)
                                                                / static_cast<double>(current.sourceDurationMs),
                                                            0.0, 1.0)
                                               : 0.0;
                const auto percentage      = static_cast<int>(
                    ((static_cast<double>(current.trackIndex) + trackProgress) / trackCount) * 100.0);
                const QString progressText = current.sourceDurationMs > 0
                                               ? tr("%1%").arg(QString::number(trackProgress * 100.0, 'f', 1))
                                               : tr("Calculating…");
                const QString text         = tr("Converting %1 of %2 (%3)")
                                                 .arg(current.trackIndex + 1)
                                                 .arg(current.trackCount)
                                                 .arg(progressText)
                                           + "\n"_L1 + tr("Current file") + ":\n"_L1 + current.sourcePath;
                QMetaObject::invokeMethod(progress, [progress, percentage, text]() {
                    progress->setText(text);
                    progress->setValue(percentage);
                });
            };
            request.existingFileCallback = [cancelled, progress, session](const QString& path) {
                if(cancelled->load(std::memory_order_acquire)) {
                    return ExistingFileMode::Ask;
                }

                auto promise        = std::make_shared<std::promise<ExistingFileMode>>();
                auto decisionFuture = promise->get_future();
                const bool invoked  = QMetaObject::invokeMethod(
                    session,
                    [cancelled, progress, path, promise, session]() {
                        if(cancelled->load(std::memory_order_acquire)) {
                            promise->set_value(ExistingFileMode::Ask);
                            return;
                        }

                        progress->show();

                        auto* prompt
                            = new QMessageBox{QMessageBox::Question, tr("File already exists"),
                                              tr("The file already exists:\n%1").arg(path),
                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, progress};
                        prompt->setAttribute(Qt::WA_DeleteOnClose);
                        prompt->setModal(false);
                        prompt->button(QMessageBox::Yes)->setText(tr("Overwrite"));
                        prompt->button(QMessageBox::No)->setText(tr("Skip"));

                        QObject::connect(prompt, &QDialog::finished, session, [cancelled, promise](int result) {
                            if(result == QMessageBox::Yes) {
                                promise->set_value(ExistingFileMode::Overwrite);
                            }
                            else if(result == QMessageBox::No) {
                                promise->set_value(ExistingFileMode::Skip);
                            }
                            else {
                                cancelled->store(true, std::memory_order_release);
                                promise->set_value(ExistingFileMode::Ask);
                            }
                        });

                        prompt->open();
                        prompt->raise();
                        prompt->activateWindow();
                    },
                    Qt::QueuedConnection);
                return invoked ? decisionFuture.get() : ExistingFileMode::Ask;
            };
            return ConversionRunner::run(request);
        });

        m_watcher->setFuture(future);
    }

private:
    void finish()
    {
        const auto results = m_watcher->result();
        m_progress->setValue(100);

        const ConversionCounts counts = conversionCounts(results);

        bool hasDetails{false};
        for(const ConversionTrackResult& result : results) {
            hasDetails = hasDetails || !result.error.isEmpty() || !result.warnings.empty();
        }

        if(m_showReport || counts.failed > 0 || counts.cancelled > 0 || hasDetails) {
            m_progress->hide();
            showConversionResults(m_parentWindow, results, counts, hasDetails, m_job.preset.other.verifyOutput);
        }

        if(m_showOutputFiles) {
            TrackList convertedTracks;
            std::set<QString> outputPaths;
            for(const ConversionTrackResult& result : results) {
                if(result.status != ConversionResultStatus::Succeeded || result.outputPath.isEmpty()) {
                    continue;
                }

                const QString outputPath = QDir::cleanPath(QFileInfo{result.outputPath}.absoluteFilePath());
                if(!outputPaths.insert(outputPath).second) {
                    continue;
                }

                Track outputTrack{outputPath};
                if(!m_audioLoader->readTrackMetadata(outputTrack)) {
                    qCWarning(CONV_CTRL) << "Failed to read metadata of file:" << outputPath;
                }
                readFileProperties(outputTrack);
                convertedTracks.push_back(std::move(outputTrack));
            }

            if(!convertedTracks.empty()) {
                Q_EMIT convertedFilesReady(convertedTracks);
            }
        }

        if(m_completion) {
            m_completion(results);
        }

        m_progress->deleteLater();
        deleteLater();
    }

    std::shared_ptr<AudioLoader> m_audioLoader;
    AudioEncoderRegistry* m_encoderRegistry;
    DspRegistry* m_dspRegistry;

    ConversionJob m_job;
    QString m_askFolder;
    bool m_showReport;
    bool m_showOutputFiles;
    std::shared_ptr<ConversionInputObserver> m_sourceObserver;
    std::function<void(const std::vector<ConversionTrackResult>&)> m_completion;
    QWidget* m_parentWindow;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    ElapsedProgressDialog* m_progress;
    QFutureWatcher<std::vector<ConversionTrackResult>>* m_watcher;

Q_SIGNALS:
    void convertedFilesReady(const Fooyin::TrackList& tracks);
};
} // namespace

ConversionController::ConversionController(std::shared_ptr<AudioLoader> audioLoader,
                                           AudioEncoderRegistry* encoderRegistry, DspRegistry* dspRegistry,
                                           DspChainStore* dspChainStore, DspSettingsRegistry* dspSettingsRegistry,
                                           SettingsManager* settings, QWidget* parentWindow, QObject* parent)
    : QObject{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_encoderRegistry{encoderRegistry}
    , m_dspRegistry{dspRegistry}
    , m_dspChainStore{dspChainStore}
    , m_dspSettingsRegistry{dspSettingsRegistry}
    , m_settings{settings}
    , m_parentWindow{parentWindow}
{ }

void ConversionController::showSetup(const TrackList& tracks)
{
    showSetup(tracks, {}, {}, {});
}

void ConversionController::showSetup(const TrackList& tracks, const QString& suggestedFilenamePattern,
                                     std::shared_ptr<ConversionInputObserver> sourceObserver,
                                     std::function<void(const std::vector<ConversionTrackResult>&)> completion)
{
    if(tracks.empty()) {
        return;
    }

    auto* setup = new ConverterSetupDialog(m_encoderRegistry, m_dspChainStore, m_settings, tracks, m_parentWindow,
                                           m_dspSettingsRegistry);
    setup->setAttribute(Qt::WA_DeleteOnClose);
    setup->setModal(false);
    setup->applySuggestedFilenamePattern(suggestedFilenamePattern);

    QObject::connect(setup, &QDialog::finished, this,
                     [this, setup, sourceObserver = std::move(sourceObserver),
                      completion = std::move(completion)](int result) mutable {
                         if(result != QDialog::Accepted) {
                             Q_EMIT conversionPresetsChanged();
                             return;
                         }

                         ConversionJob job          = setup->job();
                         const bool showReport      = setup->showReport();
                         const bool showOutputFiles = setup->showOutputFiles();
                         ConverterSettings::setLastUsedConversionPreset({
                             .name            = u"[last used]"_s,
                             .preset          = job.preset,
                             .showReport      = showReport,
                             .showOutputFiles = showOutputFiles,
                         });
                         Q_EMIT conversionPresetsChanged();
                         start(std::move(job), setup->askFolder(), showReport, showOutputFiles,
                               std::move(sourceObserver), std::move(completion));
                     });

    setup->show();
    setup->raise();
    setup->activateWindow();
}

std::vector<ConversionPresetInfo> ConversionController::presets() const
{
    std::vector<ConversionPresetInfo> result;

    for(const StoredConversionPreset& stored : ConverterSettings::conversionPresets()) {
        if(!stored.preset.id.isEmpty()) {
            result.push_back({.id = stored.preset.id, .name = stored.name});
        }
    }

    return result;
}

bool ConversionController::startPreset(const QString& presetId, const TrackList& tracks)
{
    return startPreset(presetId, tracks, {}, {}, {});
}

bool ConversionController::startPreset(const QString& presetId, const TrackList& tracks,
                                       const QString& suggestedFilenamePattern,
                                       std::shared_ptr<ConversionInputObserver> sourceObserver,
                                       std::function<void(const std::vector<ConversionTrackResult>&)> completion)
{
    if(presetId.isEmpty() || tracks.empty()) {
        return false;
    }

    const auto presets = ConverterSettings::conversionPresets();
    const auto stored
        = std::ranges::find(presets, presetId, [](const StoredConversionPreset& preset) { return preset.preset.id; });
    if(stored == presets.cend()) {
        return false;
    }

    QString askFolder;
    if(stored->preset.destination.mode == DestinationMode::Ask) {
        askFolder = QFileDialog::getExistingDirectory(m_parentWindow, tr("Choose destination"), QDir::homePath());
        if(askFolder.isEmpty()) {
            return false;
        }
    }

    StoredConversionPreset selected{*stored};
    if(!suggestedFilenamePattern.trimmed().isEmpty()
       && selected.preset.destination.filenamePattern.trimmed() == u"%filename%"_s) {
        selected.preset.destination.filenamePattern = suggestedFilenamePattern.trimmed();
    }

    StoredConversionPreset lastUsed{selected};
    lastUsed.name        = u"[last used]"_s;
    lastUsed.preset.name = lastUsed.name;
    ConverterSettings::setLastUsedConversionPreset(lastUsed);
    Q_EMIT conversionPresetsChanged();

    start({.tracks = tracks, .preset = std::move(selected.preset)}, std::move(askFolder), selected.showReport,
          selected.showOutputFiles, std::move(sourceObserver), std::move(completion));
    return true;
}

void ConversionController::start(ConversionJob job, QString askFolder, bool showReport, bool showOutputFiles)
{
    start(std::move(job), std::move(askFolder), showReport, showOutputFiles, {}, {});
}

void ConversionController::start(ConversionJob job, QString askFolder, bool showReport, bool showOutputFiles,
                                 std::shared_ptr<ConversionInputObserver> sourceObserver,
                                 std::function<void(const std::vector<ConversionTrackResult>&)> completion)
{
    if(job.tracks.empty()) {
        return;
    }

    auto* session = new ConversionSession{m_audioLoader,
                                          m_encoderRegistry,
                                          m_dspRegistry,
                                          std::move(job),
                                          std::move(askFolder),
                                          showReport,
                                          showOutputFiles,
                                          std::move(sourceObserver),
                                          std::move(completion),
                                          m_parentWindow,
                                          this};
    QObject::connect(session, &ConversionSession::convertedFilesReady, this,
                     &ConversionController::convertedFilesReady);
    session->start();
}
} // namespace Fooyin

#include "conversioncontroller.moc"
#include "moc_conversioncontroller.cpp"
