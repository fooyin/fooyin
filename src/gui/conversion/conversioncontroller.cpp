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

#include <core/engine/conversion/conversionrunner.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/async.h>

#include <QAbstractButton>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMessageBox>

#include <atomic>
#include <chrono>
#include <future>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class ConversionSession : public QObject
{
    Q_OBJECT

public:
    ConversionSession(std::shared_ptr<AudioLoader> audioLoader, AudioEncoderRegistry* encoderRegistry,
                      DspRegistry* dspRegistry, ConversionJob job, QString askFolder, bool showReport,
                      QWidget* parentWindow, QObject* parent)
        : QObject{parent}
        , m_audioLoader{std::move(audioLoader)}
        , m_encoderRegistry{encoderRegistry}
        , m_dspRegistry{dspRegistry}
        , m_job{std::move(job)}
        , m_askFolder{std::move(askFolder)}
        , m_showReport{showReport}
        , m_parentWindow{parentWindow}
        , m_cancelled{std::make_shared<std::atomic_bool>(false)}
        , m_progress{new ElapsedProgressDialog(tr("Preparing conversion…"), tr("Cancel"), 0, 100, parentWindow)}
        , m_watcher{new QFutureWatcher<std::vector<ConversionTrackResult>>(this)}
    {
        m_progress->setAttribute(Qt::WA_DeleteOnClose, false);
        m_progress->setWindowTitle(tr("Audio Conversion"));
        m_progress->setModal(true);
        m_progress->setMinimumDuration(250ms);
        m_progress->startTimer();
        m_progress->setValue(0);

        QObject::connect(m_progress, &ElapsedProgressDialog::cancelled, m_progress,
                         [cancelled = m_cancelled]() { cancelled->store(true, std::memory_order_release); });
        QObject::connect(m_watcher, &QFutureWatcherBase::finished, this, &ConversionSession::finish);
    }

    void start()
    {
        auto future = Utils::asyncExec([audioLoader = m_audioLoader, encoderRegistry = m_encoderRegistry,
                                        dspRegistry = m_dspRegistry, job = m_job, askFolder = m_askFolder,
                                        cancelled = m_cancelled, progress = m_progress, parent = m_parentWindow,
                                        session = this]() {
            ConversionRunner::Request request;
            request.audioLoader     = audioLoader.get();
            request.encoderRegistry = encoderRegistry;
            request.dspRegistry     = dspRegistry;
            request.job             = job;
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
                const QString text = tr("Current file") + ":\n"_L1 + current.sourcePath;
                QMetaObject::invokeMethod(progress, [progress, percentage, text]() {
                    progress->setText(text);
                    progress->setValue(percentage);
                });
            };
            request.existingFileCallback = [cancelled, parent, session](const QString& path) {
                if(cancelled->load(std::memory_order_acquire)) {
                    return ExistingFileMode::Ask;
                }

                auto promise        = std::make_shared<std::promise<ExistingFileMode>>();
                auto decisionFuture = promise->get_future();
                const bool invoked  = QMetaObject::invokeMethod(
                    session,
                    [cancelled, parent, path, promise, session]() {
                        if(cancelled->load(std::memory_order_acquire)) {
                            promise->set_value(ExistingFileMode::Ask);
                            return;
                        }

                        auto* prompt
                            = new QMessageBox{QMessageBox::Question, tr("File already exists"),
                                              tr("The file already exists:\n%1").arg(path),
                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, parent};
                        prompt->setAttribute(Qt::WA_DeleteOnClose);
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

        int succeeded{0};
        int skipped{0};
        int failed{0};
        int cancelledCount{0};
        QStringList details;
        for(const ConversionTrackResult& result : results) {
            switch(result.status) {
                case ConversionResultStatus::Succeeded:
                    ++succeeded;
                    break;
                case ConversionResultStatus::Skipped:
                    ++skipped;
                    break;
                case ConversionResultStatus::Failed:
                    ++failed;
                    break;
                case ConversionResultStatus::Cancelled:
                    ++cancelledCount;
                    break;
            }
            if(!result.error.isEmpty()) {
                details.push_back(QFileInfo{result.sourceTrack.filepath()}.fileName() + u": "_s + result.error);
            }
            for(const QString& warning : result.warnings) {
                details.push_back(QFileInfo{result.sourceTrack.filepath()}.fileName() + u": "_s + warning);
            }
        }

        if(m_showReport || failed > 0 || !details.isEmpty()) {
            const QString summary = QStringList{tr("Converted: %Ln track(s)", nullptr, succeeded),
                                                tr("Skipped: %Ln track(s)", nullptr, skipped),
                                                tr("Failed: %Ln track(s)", nullptr, failed),
                                                tr("Cancelled: %Ln track(s)", nullptr, cancelledCount)}
                                        .join(u'\n');
            QMessageBox report{failed > 0 ? QMessageBox::Warning : QMessageBox::Information, tr("Audio Conversion"),
                               summary, QMessageBox::Ok, m_parentWindow};
            if(!details.isEmpty()) {
                report.setDetailedText(details.join(u'\n'));
            }
            report.exec();
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
    QWidget* m_parentWindow;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    ElapsedProgressDialog* m_progress;
    QFutureWatcher<std::vector<ConversionTrackResult>>* m_watcher;
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
    if(tracks.empty()) {
        return;
    }

    ConverterSetupDialog setup{m_encoderRegistry, m_dspChainStore,      m_settings, tracks,
                               m_parentWindow,    m_dspSettingsRegistry};
    const int result = setup.exec();
    if(result != QDialog::Accepted) {
        Q_EMIT conversionPresetsChanged();
        return;
    }

    ConversionJob job = setup.job();
    ConverterSettings::setLastUsedConversionPreset({
        .name       = u"[last used]"_s,
        .preset     = job.preset,
        .showReport = setup.showReport(),
    });
    Q_EMIT conversionPresetsChanged();
    start(std::move(job), setup.askFolder(), setup.showReport());
}

void ConversionController::start(ConversionJob job, QString askFolder, bool showReport)
{
    if(job.tracks.empty()) {
        return;
    }

    auto* session = new ConversionSession{m_audioLoader,        m_encoderRegistry, m_dspRegistry,  std::move(job),
                                          std::move(askFolder), showReport,        m_parentWindow, this};
    session->start();
}
} // namespace Fooyin

#include "conversioncontroller.moc"
#include "moc_conversioncontroller.cpp"
