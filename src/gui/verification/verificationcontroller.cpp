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
 */

#include "verificationcontroller.h"

#include <core/engine/verification/accuraterip.h>
#include <core/engine/verification/audioverifier.h>
#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/async.h>

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QVBoxLayout>

#include <atomic>
#include <chrono>
#include <ranges>
#include <utility>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
enum class VerificationMode : uint8_t
{
    Integrity = 0,
    AccurateRip,
};

QString displayName(const Track& track)
{
    if(!track.trackNumber().isEmpty() && !track.effectiveTitle().isEmpty()) {
        return track.trackNumber() + u". "_s + track.effectiveTitle();
    }
    if(!track.effectiveTitle().isEmpty()) {
        return track.effectiveTitle();
    }
    return QFileInfo{track.filepath()}.fileName();
}

QString integrityStatus(const AudioVerificationResult& result)
{
    switch(result.status) {
        case AudioVerificationStatus::Succeeded:
            return VerificationController::tr("OK");
        case AudioVerificationStatus::Failed:
            return VerificationController::tr("Failed");
        case AudioVerificationStatus::Cancelled:
            return VerificationController::tr("Cancelled");
    }
    return {};
}

QString accurateRipStatus(AccurateRip::VerifyStatus status)
{
    switch(status) {
        case AccurateRip::VerifyStatus::Verified:
            return VerificationController::tr("Accurately ripped");
        case AccurateRip::VerifyStatus::Mismatch:
            return VerificationController::tr("No match");
        case AccurateRip::VerifyStatus::Incomplete:
            return VerificationController::tr("Incomplete");
        case AccurateRip::VerifyStatus::InvalidFormat:
            return VerificationController::tr("Unsupported format");
    }
    return {};
}

QString crcText(uint32_t crc)
{
    return QString::number(crc, 16).rightJustified(8, u'0').toUpper();
}

QString integrityName(const Track& track)
{
    return !track.effectiveTitle().isEmpty() ? track.effectiveTitle() : QFileInfo{track.filepath()}.fileName();
}

QString md5Text(const AudioVerificationResult& result)
{
    return QString::fromLatin1(result.md5.toHex().toUpper());
}

QString sanitiseFilename(QString filename)
{
    static const auto invalidCharacters = u"<>:\"/\\|?*"_s;
    for(qsizetype index{0}; index < filename.size(); ++index) {
        const QChar character = filename.at(index);
        if(character.unicode() < 0x20 || invalidCharacters.contains(character)) {
            filename[index] = u'_';
        }
    }
    filename = filename.simplified();
    while(filename.endsWith(u' ') || filename.endsWith(u'.')) {
        filename.chop(1);
    }
    return filename;
}

QString verificationReportPath(const Track& track, const QString& fallbackName, const QString& extension)
{
    QString artist = track.effectiveAlbumArtist(true).trimmed();
    if(artist.isEmpty()) {
        artist = track.primaryArtist().trimmed();
    }

    QStringList filenameParts;
    if(!artist.isEmpty()) {
        filenameParts.push_back(artist);
    }
    if(!track.album().trimmed().isEmpty()) {
        filenameParts.push_back(track.album().trimmed());
    }

    QString filename = sanitiseFilename(filenameParts.join(u" - "_s));
    if(filename.isEmpty()) {
        filename = fallbackName;
    }

    const QString sourcePath = track.isInArchive() ? track.archivePath() : track.filepath();
    const QString directory  = QFileInfo{sourcePath}.absolutePath();
    return QDir{directory.isEmpty() ? QDir::homePath() : directory}.filePath(filename + extension);
}

QString integrityReportPath(const std::vector<AudioVerificationResult>& results)
{
    return results.empty()
             ? QDir::home().filePath(u"fooyin-integrity-verification.txt"_s)
             : verificationReportPath(results.front().track, u"fooyin-integrity-verification"_s, u".txt"_s);
}

QString integrityReport(const std::vector<AudioVerificationResult>& results)
{
    QStringList lines{
        u"fooyin File Integrity Verification"_s,
        VerificationController::tr("Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
        {}};
    int succeeded{0};

    for(const auto& result : results) {
        lines.push_back(VerificationController::tr("Item: \"%1\"").arg(result.track.prettyFilepath()));
        lines.push_back(VerificationController::tr("Status: %1").arg(integrityStatus(result)));
        if(!result.md5.isEmpty()) {
            lines.push_back(VerificationController::tr("Decoded audio MD5: %1").arg(md5Text(result)));
            lines.push_back(VerificationController::tr("Decoded audio CRC32: %1").arg(crcText(result.crc32)));
        }
        for(const QString& warning : result.warnings) {
            lines.push_back(VerificationController::tr("Warning: %1").arg(warning));
        }

        if(result.status == AudioVerificationStatus::Succeeded) {
            ++succeeded;
        }
        else if(result.status == AudioVerificationStatus::Failed) {
            lines.push_back(VerificationController::tr("Error: %1").arg(result.error));
        }
        lines.push_back({});
    }

    lines.push_back({});
    if(std::cmp_equal(succeeded, results.size())) {
        lines.push_back(VerificationController::tr("All items decoded successfully."));
    }
    else {
        lines.push_back(VerificationController::tr("%1 of %Ln item(s) decoded successfully.", nullptr,
                                                   static_cast<int>(results.size()))
                            .arg(succeeded));
    }
    return lines.join(u'\n') + u'\n';
}

QString albumReportPath(const std::vector<AccurateRip::TrackResult>& results)
{
    if(results.empty()) {
        return QDir::home().filePath(u"fooyin-accuraterip-verification.log"_s);
    }
    return verificationReportPath(results.front().track, u"fooyin-accuraterip-verification"_s, u".log"_s);
}

struct AccurateRipOutcomeCounts
{
    int verified{0};
    int mismatch{0};
    int incomplete{0};
    int invalidFormat{0};
};

AccurateRipOutcomeCounts accurateRipOutcomeCounts(const std::vector<AccurateRip::TrackResult>& results)
{
    AccurateRipOutcomeCounts counts;
    for(const auto& result : results) {
        switch(result.status) {
            case AccurateRip::VerifyStatus::Verified:
                ++counts.verified;
                break;
            case AccurateRip::VerifyStatus::Mismatch:
                ++counts.mismatch;
                break;
            case AccurateRip::VerifyStatus::Incomplete:
                ++counts.incomplete;
                break;
            case AccurateRip::VerifyStatus::InvalidFormat:
                ++counts.invalidFormat;
                break;
        }
    }
    return counts;
}

QString accurateRipDiscId(const AccurateRip::DiscId& id)
{
    return u"dBAR-%1-%2-%3-%4"_s.arg(id.trackCount, 3, 10, QChar{u'0'})
        .arg(id.id1, 8, 16, QChar{u'0'})
        .arg(id.id2, 8, 16, QChar{u'0'})
        .arg(id.cddbId, 8, 16, QChar{u'0'});
}

QString pressingOffsetText(int sampleOffset)
{
    if(sampleOffset == 0) {
        return VerificationController::tr("0 samples");
    }
    return VerificationController::tr("%1 samples")
        .arg(sampleOffset > 0 ? u"+%1"_s.arg(sampleOffset) : QString::number(sampleOffset));
}

QStringList matchedDatabaseCrcs(const AccurateRip::TrackResult& result)
{
    QStringList matches;

    const bool matchesV1 = std::ranges::contains(result.databaseCrcs, result.crcV1);
    const bool matchesV2 = std::ranges::contains(result.databaseCrcs, result.crcV2);

    if(matchesV1 && matchesV2 && result.crcV1 == result.crcV2) {
        matches.push_back(VerificationController::tr("%1 (AR v1/v2)").arg(crcText(result.crcV1)));
    }
    else if(matchesV1) {
        matches.push_back(VerificationController::tr("%1 (AR v1)").arg(crcText(result.crcV1)));
    }
    if(matchesV2 && result.crcV2 != result.crcV1) {
        matches.push_back(VerificationController::tr("%1 (AR v2)").arg(crcText(result.crcV2)));
    }

    return matches;
}

QString accurateRipSummary(const AccurateRipOutcomeCounts& counts)
{
    return VerificationController::tr("Accurately ripped: %1 | No match: %2 | Incomplete: %3 | Unsupported: %4")
        .arg(counts.verified)
        .arg(counts.mismatch)
        .arg(counts.incomplete)
        .arg(counts.invalidFormat);
}

QString accurateRipReport(const AccurateRip::DiscId& discId, const std::vector<AccurateRip::TrackResult>& results)
{
    QStringList lines{
        u"fooyin AccurateRip verification log"_s,
        VerificationController::tr("Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
        {}};

    if(!results.empty()) {
        const Track& first = results.front().track;
        QString artist     = first.effectiveAlbumArtist(true).trimmed();

        if(artist.isEmpty()) {
            artist = first.primaryArtist().trimmed();
        }

        if(!artist.isEmpty() || !first.album().isEmpty()) {
            QStringList albumParts;
            if(!artist.isEmpty()) {
                albumParts.push_back(artist);
            }
            if(!first.album().isEmpty()) {
                albumParts.push_back(first.album());
            }
            lines.push_back(albumParts.join(u" / "_s));
        }
    }
    lines.push_back(VerificationController::tr("AccurateRip disc ID: %1").arg(accurateRipDiscId(discId)));
    lines.push_back(VerificationController::tr("Tracks: %1").arg(results.size()));
    lines.push_back({});

    for(size_t index{0}; index < results.size(); ++index) {
        const auto& result = results.at(index);
        const QString trackNumber
            = result.track.trackNumber().isEmpty() ? QString::number(index + 1) : result.track.trackNumber();

        lines.push_back(VerificationController::tr("Track %1").arg(trackNumber));
        lines.push_back(u"     "_s + VerificationController::tr("Filename: %1").arg(result.track.prettyFilepath()));
        lines.push_back(u"     "_s + VerificationController::tr("Status: %1").arg(accurateRipStatus(result.status)));
        lines.push_back(u"     "_s + VerificationController::tr("AR v1 CRC: %1").arg(crcText(result.crcV1)));
        lines.push_back(u"     "_s + VerificationController::tr("AR v2 CRC: %1").arg(crcText(result.crcV2)));

        if(result.status == AccurateRip::VerifyStatus::Verified) {
            lines.push_back(u"     "_s + VerificationController::tr("Confidence: %1").arg(result.confidence));
            lines.push_back(
                u"     "_s
                + VerificationController::tr("Pressing offset: %1").arg(pressingOffsetText(result.sampleOffset)));
            lines.push_back(u"     "_s
                            + VerificationController::tr("Matched database CRC: %1")
                                  .arg(matchedDatabaseCrcs(result).join(u", "_s)));
        }
        else if(!result.databaseCrcs.empty()) {
            QStringList databaseCrcs;
            for(const uint32_t crc : result.databaseCrcs) {
                databaseCrcs.push_back(crcText(crc));
            }
            lines.push_back(u"     "_s
                            + VerificationController::tr("Database CRCs: %1").arg(databaseCrcs.join(u", "_s)));
        }
        lines.push_back({});
    }

    const AccurateRipOutcomeCounts counts = accurateRipOutcomeCounts(results);
    lines.push_back(VerificationController::tr("Summary"));
    lines.push_back(u"     "_s + VerificationController::tr("Accurately ripped: %1").arg(counts.verified));
    lines.push_back(u"     "_s + VerificationController::tr("No match: %1").arg(counts.mismatch));
    lines.push_back(u"     "_s + VerificationController::tr("Incomplete: %1").arg(counts.incomplete));
    lines.push_back(u"     "_s + VerificationController::tr("Unsupported: %1").arg(counts.invalidFormat));
    return lines.join(u'\n') + u'\n';
}

void exportResults(QWidget* parent, const QString& title, const QString& suggestedPath, const QString& report,
                   const QString& filter)
{
    const QString path = QFileDialog::getSaveFileName(parent, VerificationController::tr("Export Results"),
                                                      suggestedPath, filter, nullptr, QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    QSaveFile file{path};
    const QByteArray data = report.toUtf8();
    if(!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        QMessageBox::warning(parent, title, VerificationController::tr("The results file could not be written."));
    }
}

void showIntegrityResults(QWidget* parent, const std::vector<AudioVerificationResult>& results)
{
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(VerificationController::tr("File Integrity Verification"));
    dialog->resize(900, 440);

    int problems{0};
    for(const auto& result : results) {
        problems += result.status != AudioVerificationStatus::Succeeded || !result.warnings.empty() ? 1 : 0;
    }

    auto* summary
        = new QLabel(problems == 0 ? VerificationController::tr("No problems found.")
                                   : VerificationController::tr("Problems found in %Ln item(s).", nullptr, problems),
                     dialog);
    auto* table = new QTableWidget(static_cast<int>(results.size()), 6, dialog);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);

    table->setHorizontalHeaderLabels({u"#"_s, VerificationController::tr("Name"), VerificationController::tr("Status"),
                                      VerificationController::tr("Warnings"), u"MD5"_s, u"CRC32"_s});

    for(int row{0}; std::cmp_less(row, results.size()); ++row) {
        const auto& result   = results.at(row);
        QStringList warnings = result.warnings;
        if(!result.error.isEmpty()) {
            warnings.push_front(result.error);
        }

        auto* numberItem = new QTableWidgetItem(QString::number(row + 1));
        numberItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 0, numberItem);
        table->setItem(row, 1, new QTableWidgetItem(integrityName(result.track)));
        table->setItem(row, 2, new QTableWidgetItem(integrityStatus(result)));
        table->setItem(row, 3, new QTableWidgetItem(warnings.join(u"; "_s)));
        table->setItem(row, 4, new QTableWidgetItem(result.md5.isEmpty() ? QString{} : md5Text(result)));
        table->setItem(row, 5, new QTableWidgetItem(result.md5.isEmpty() ? QString{} : crcText(result.crc32)));
    }

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for(int column{2}; column < table->columnCount(); ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    auto* buttons      = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto* exportButton = buttons->addButton(VerificationController::tr("Export…"), QDialogButtonBox::ActionRole);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    const QString reportPath = integrityReportPath(results);
    const QString report     = integrityReport(results);
    QObject::connect(exportButton, &QPushButton::clicked, dialog, [dialog, reportPath, report] {
        exportResults(dialog, dialog->windowTitle(), reportPath, report,
                      VerificationController::tr("Text Files (*.txt)"));
    });

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(table);
    auto* footer = new QHBoxLayout;
    footer->addWidget(summary, 1);
    footer->addWidget(buttons);
    layout->addLayout(footer);

    dialog->show();
}

void showAccurateRipResults(QWidget* parent, const AccurateRip::DiscId& discId,
                            const std::vector<AccurateRip::TrackResult>& results)
{
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(VerificationController::tr("AccurateRip Verification"));
    dialog->resize(900, 440);

    const AccurateRipOutcomeCounts counts = accurateRipOutcomeCounts(results);
    auto* summary                         = new QLabel(accurateRipSummary(counts), dialog);

    auto* table = new QTableWidget(static_cast<int>(results.size()), 6, dialog);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);

    table->setHorizontalHeaderLabels({VerificationController::tr("Track"), VerificationController::tr("Status"),
                                      VerificationController::tr("Confidence"),
                                      VerificationController::tr("Pressing offset"),
                                      VerificationController::tr("CRC v1"), VerificationController::tr("CRC v2")});

    for(int row{0}; std::cmp_less(row, results.size()); ++row) {
        const auto& result = results.at(row);

        QStringList database;
        for(const uint32_t crc : result.databaseCrcs) {
            database.push_back(crcText(crc));
        }
        const bool verified = result.status == AccurateRip::VerifyStatus::Verified;

        table->setItem(row, 0, new QTableWidgetItem(displayName(result.track)));
        auto* statusItem = new QTableWidgetItem(accurateRipStatus(result.status));
        if(!database.empty()) {
            statusItem->setToolTip(VerificationController::tr("Database CRCs: %1").arg(database.join(u", "_s)));
        }
        table->setItem(row, 1, statusItem);
        table->setItem(row, 2, new QTableWidgetItem(verified ? QString::number(result.confidence) : u"—"_s));
        table->setItem(row, 3, new QTableWidgetItem(verified ? pressingOffsetText(result.sampleOffset) : u"—"_s));

        auto* crcV1Item = new QTableWidgetItem(crcText(result.crcV1));
        if(std::ranges::contains(result.databaseCrcs, result.crcV1)) {
            QFont font = crcV1Item->font();
            font.setBold(true);
            crcV1Item->setFont(font);
            crcV1Item->setToolTip(VerificationController::tr("Matches the AccurateRip database (AR v1)"));
        }
        table->setItem(row, 4, crcV1Item);

        auto* crcV2Item = new QTableWidgetItem(crcText(result.crcV2));
        if(std::ranges::contains(result.databaseCrcs, result.crcV2)) {
            QFont font = crcV2Item->font();
            font.setBold(true);
            crcV2Item->setFont(font);
            crcV2Item->setToolTip(VerificationController::tr("Matches the AccurateRip database (AR v2)"));
        }
        table->setItem(row, 5, crcV2Item);
    }

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    for(int column{1}; column < table->columnCount(); ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    auto* buttons      = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto* exportButton = buttons->addButton(VerificationController::tr("Export…"), QDialogButtonBox::ActionRole);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    const QString reportPath = albumReportPath(results);
    const QString report     = accurateRipReport(discId, results);

    QObject::connect(exportButton, &QPushButton::clicked, dialog, [dialog, reportPath, report] {
        exportResults(dialog, dialog->windowTitle(), reportPath, report,
                      VerificationController::tr("Log Files (*.log);;Text Files (*.txt)"));
    });

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(table);
    auto* footer = new QHBoxLayout();
    footer->addWidget(summary, 1);
    footer->addWidget(buttons);
    layout->addLayout(footer);

    dialog->show();
}

class VerificationSession : public QObject
{
    Q_OBJECT

public:
    VerificationSession(std::shared_ptr<AudioLoader> audioLoader, std::shared_ptr<NetworkAccessManager> networkAccess,
                        TrackList tracks, VerificationMode mode, QWidget* parentWindow, QObject* parent)
        : QObject{parent}
        , m_audioLoader{std::move(audioLoader)}
        , m_networkAccess{std::move(networkAccess)}
        , m_tracks{std::move(tracks)}
        , m_mode{mode}
        , m_parentWindow{parentWindow}
        , m_cancelled{std::make_shared<std::atomic_bool>(false)}
        , m_progress{new ElapsedProgressDialog(tr("Preparing verification…"), tr("Cancel"), 0, 100, parentWindow)}
        , m_watcher{new QFutureWatcher<std::vector<AudioVerificationResult>>(this)}
    {
        m_progress->setWindowTitle(mode == VerificationMode::Integrity ? tr("File Integrity Verification")
                                                                       : tr("AccurateRip Verification"));
        m_progress->setModal(false);
        m_progress->setMinimumDuration(250ms);
        m_progress->startTimer();
        m_progress->setValue(0);

        QObject::connect(m_progress, &ElapsedProgressDialog::cancelled, m_progress,
                         [cancelled = m_cancelled] { cancelled->store(true, std::memory_order_release); });
        QObject::connect(m_watcher, &QFutureWatcherBase::finished, this, [this] { decodingFinished(); });
    }

    void start()
    {
        if(m_mode == VerificationMode::AccurateRip) {
            m_albumVerifier = std::make_shared<AccurateRip::AlbumVerifier>(m_tracks);
        }
        startDecode(m_albumVerifier);
    }

private:
    void startDecode(const std::shared_ptr<ConversionInputObserver>& observer)
    {
        const QPointer progress{m_progress};
        auto future = Utils::asyncExec(
            [audioLoader = m_audioLoader, tracks = m_tracks, cancelled = m_cancelled, progress, observer] {
                AudioVerifier::Request request;
                request.audioLoader     = audioLoader.get();
                request.tracks          = tracks;
                request.verifyIntegrity = true;
                request.observer        = observer;
                request.cancelCallback  = [cancelled] {
                    return cancelled->load(std::memory_order_acquire);
                };
                request.progressCallback = [progress](const AudioVerificationProgress& current) {
                    if(!progress) {
                        return;
                    }
                    QMetaObject::invokeMethod(
                        progress,
                        [progress, current] {
                            if(!progress) {
                                return;
                            }
                            const int count = std::max(1, current.trackCount);
                            progress->setValue(std::clamp(current.trackIndex * 100 / count, 0, 99));
                            progress->setText(
                                VerificationSession::tr("Current file:\n%1").arg(current.track.prettyFilepath()));
                        },
                        Qt::QueuedConnection);
                };
                return AudioVerifier::run(request);
            });

        m_watcher->setFuture(future);
    }

    void fail(const QString& message)
    {
        m_progress->deleteLater();
        QMessageBox::warning(m_parentWindow, tr("AccurateRip Verification"), message);
        deleteLater();
    }

    void decodingFinished()
    {
        m_decodeResults = m_watcher->result();

        if(m_offsetVerifier) {
            m_progress->deleteLater();
            if(std::ranges::any_of(m_decodeResults, [](const auto& result) {
                   return result.status != AudioVerificationStatus::Succeeded;
               })) {
                showIntegrityResults(m_parentWindow, m_decodeResults);
            }
            else {
                Q_ASSERT(m_discId);
                showAccurateRipResults(m_parentWindow, *m_discId, m_offsetVerifier->results());
            }
            deleteLater();
            return;
        }

        if(m_mode == VerificationMode::Integrity) {
            m_progress->deleteLater();
            showIntegrityResults(m_parentWindow, m_decodeResults);
            deleteLater();
            return;
        }

        if(std::ranges::any_of(m_decodeResults, [](const auto& result) {
               return result.status != AudioVerificationStatus::Succeeded;
           })) {
            m_progress->deleteLater();
            showIntegrityResults(m_parentWindow, m_decodeResults);
            deleteLater();
            return;
        }

        const auto layout = m_albumVerifier->layout();
        if(!layout) {
            fail(layout.error());
            return;
        }

        const auto id = AccurateRip::discId(*layout);
        if(!id) {
            fail(id.error());
            return;
        }

        if(m_cancelled->load(std::memory_order_acquire)) {
            m_progress->deleteLater();
            deleteLater();
            return;
        }

        m_discId = *id;
        m_progress->setBusy(true);
        m_progress->setText(tr("Looking up album in AccurateRip…"));

        QNetworkReply* reply = m_networkAccess->get(makeNetworkRequest(AccurateRip::discUrl(*id)));
        QObject::connect(m_progress, &ElapsedProgressDialog::cancelled, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id = *id] {
            if(m_cancelled->load(std::memory_order_acquire)) {
                m_progress->deleteLater();
                deleteLater();
                return;
            }

            if(reply->error() != QNetworkReply::NoError) {
                fail(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404
                         ? tr("This album is not present in AccurateRip.")
                         : tr("AccurateRip lookup failed: %1").arg(reply->errorString()));
                return;
            }

            static constexpr qsizetype MaximumResponseSize = 8UL * 1024 * 1024;
            const QByteArray payload                       = reply->read(MaximumResponseSize + 1);
            if(payload.size() > MaximumResponseSize) {
                fail(tr("AccurateRip returned an unexpectedly large disc record."));
                return;
            }

            const auto pressings = AccurateRip::parseResponse(payload, id);
            if(!pressings) {
                fail(pressings.error());
                return;
            }

            const auto offsetLayout = m_albumVerifier->layout();
            if(!offsetLayout) {
                fail(offsetLayout.error());
                return;
            }

            const AccurateRip::OffsetMatch offset = m_albumVerifier->bestOffset(*pressings);
            m_offsetVerifier = std::make_shared<AccurateRip::OffsetVerifier>(*offsetLayout, *pressings, m_tracks,
                                                                             offset.sampleOffset);
            m_progress->setBusy(false);
            m_progress->setValue(0);
            m_progress->setText(tr("Verifying album at sample offset %1…").arg(offset.sampleOffset));
            startDecode(m_offsetVerifier);
        });
    }

    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    TrackList m_tracks;
    VerificationMode m_mode;
    QWidget* m_parentWindow;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    ElapsedProgressDialog* m_progress;
    QFutureWatcher<std::vector<AudioVerificationResult>>* m_watcher;
    std::shared_ptr<AccurateRip::AlbumVerifier> m_albumVerifier;
    std::shared_ptr<AccurateRip::OffsetVerifier> m_offsetVerifier;
    std::optional<AccurateRip::DiscId> m_discId;
    std::vector<AudioVerificationResult> m_decodeResults;
};
} // namespace

VerificationController::VerificationController(std::shared_ptr<AudioLoader> audioLoader,
                                               std::shared_ptr<NetworkAccessManager> networkAccess,
                                               QWidget* parentWindow, QObject* parent)
    : QObject{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_networkAccess{std::move(networkAccess)}
    , m_parentWindow{parentWindow}
{ }

void VerificationController::verifyIntegrity(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    auto* session = new VerificationSession{
        m_audioLoader, m_networkAccess, tracks, VerificationMode::Integrity, m_parentWindow, this};
    session->start();
}

void VerificationController::verifyAccurateRip(const TrackList& tracks)
{
    const auto albumTracks = AccurateRip::prepareAlbumTracks(tracks);
    if(!albumTracks) {
        QMessageBox::warning(m_parentWindow, tr("AccurateRip Verification"), albumTracks.error());
        return;
    }

    auto* session = new VerificationSession{
        m_audioLoader, m_networkAccess, *albumTracks, VerificationMode::AccurateRip, m_parentWindow, this};
    session->start();
}
} // namespace Fooyin

#include "moc_verificationcontroller.cpp"
#include "verificationcontroller.moc"
