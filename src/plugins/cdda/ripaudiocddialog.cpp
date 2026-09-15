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

#include "ripaudiocddialog.h"

#include "accuraterip.h"

#include <core/constants.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
QString displayMetadataValues(const QStringList& values)
{
    return values.join(u"; "_s);
}

QString displayAlbumArtists(const Track& track)
{
    return displayMetadataValues(track.hasAlbumArtists() ? track.albumArtists() : track.artists());
}

QStringList metadataValues(QString value)
{
    value.replace(QLatin1StringView{Constants::UnitSeparator}, ";"_L1);

    QStringList values = value.split(u';', Qt::SkipEmptyParts);
    for(QString& metadataValue : values) {
        metadataValue = metadataValue.trimmed();
    }
    values.removeAll(QString{});
    return values;
}

QString accurateRipCrc(uint32_t crc)
{
    return QString::number(crc, 16).rightJustified(8, u'0').toUpper();
}

QString accurateRipVerdict(AccurateRip::VerifyStatus status)
{
    switch(status) {
        case AccurateRip::VerifyStatus::Verified:
            return RipAudioCdDialog::tr("Accurately ripped");
        case AccurateRip::VerifyStatus::Mismatch:
            return RipAudioCdDialog::tr("Mismatch");
        case AccurateRip::VerifyStatus::Incomplete:
            return RipAudioCdDialog::tr("Incomplete");
        case AccurateRip::VerifyStatus::InvalidFormat:
            return RipAudioCdDialog::tr("Unsupported format");
    }
    return {};
}

} // namespace

void showAccurateRipResults(QWidget* parent, const std::vector<AccurateRip::TrackResult>& results,
                            const QString& message)
{
    auto* dialog = new QDialog(parent);
    dialog->setWindowTitle(RipAudioCdDialog::tr("AccurateRip Verification"));
    dialog->resize(1000, 420);

    int verified{0};
    int mismatched{0};
    for(const auto& result : results) {
        verified += result.status == AccurateRip::VerifyStatus::Verified ? 1 : 0;
        mismatched += result.status == AccurateRip::VerifyStatus::Mismatch ? 1 : 0;
    }

    QStringList summaryParts;
    if(!results.empty()) {
        summaryParts.push_back(RipAudioCdDialog::tr("Verified: %1").arg(verified));
        summaryParts.push_back(RipAudioCdDialog::tr("Mismatched: %1").arg(mismatched));
    }
    if(!message.isEmpty()) {
        summaryParts.push_back(message);
    }

    auto* summary = new QLabel(summaryParts.join(u" | "_s), dialog);
    summary->setWordWrap(true);

    auto* table = new QTableWidget(static_cast<int>(results.size()), 7, dialog);
    table->setHorizontalHeaderLabels({RipAudioCdDialog::tr("Track"), RipAudioCdDialog::tr("Title"),
                                      RipAudioCdDialog::tr("Result"), RipAudioCdDialog::tr("Confidence"),
                                      RipAudioCdDialog::tr("AR v1 CRC"), RipAudioCdDialog::tr("AR v2 CRC"),
                                      RipAudioCdDialog::tr("Database CRCs")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    for(int row{0}; std::cmp_less(row, results.size()); ++row) {
        const auto& result = results.at(row);

        QStringList databaseCrcs;
        for(const uint32_t crc : result.databaseCrcs) {
            databaseCrcs.push_back(accurateRipCrc(crc));
        }

        table->setItem(row, 0, new QTableWidgetItem(result.track.trackNumber()));
        table->setItem(row, 1, new QTableWidgetItem(result.track.effectiveTitle()));
        table->setItem(row, 2, new QTableWidgetItem(accurateRipVerdict(result.status)));
        table->setItem(row, 3,
                       new QTableWidgetItem(result.status == AccurateRip::VerifyStatus::Verified
                                                ? QString::number(result.confidence)
                                                : QString{}));
        table->setItem(row, 4, new QTableWidgetItem(accurateRipCrc(result.crcV1)));
        table->setItem(row, 5, new QTableWidgetItem(accurateRipCrc(result.crcV2)));
        table->setItem(row, 6, new QTableWidgetItem(databaseCrcs.join(u", "_s)));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::accept);

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(summary);
    layout->addWidget(table, 1);
    layout->addWidget(buttons);

    dialog->show();
}

RipAudioCdDialog::RipAudioCdDialog(TrackList tracks, const std::vector<ConversionPresetInfo>& presets,
                                   bool metadataLookupAvailable, bool driveOffsetConfigured, QWidget* parent)
    : QDialog{parent}
    , m_tracks{std::move(tracks)}
    , m_metadataLookupAvailable{metadataLookupAvailable}
    , m_metadataLookupPending{false}
    , m_ripPending{false}
    , m_albumArtist{new QLineEdit(this)}
    , m_albumTitle{new QLineEdit(this)}
    , m_discNumber{new QLineEdit(this)}
    , m_genre{new QLineEdit(this)}
    , m_date{new QLineEdit(this)}
    , m_trackTable{new QTableWidget(this)}
    , m_presets{new QComboBox(this)}
    , m_verifyAccurateRip{new QCheckBox(tr("Verify with AccurateRip"), this)}
    , m_accurateRipStatus{new QLabel(this)}
    , m_lookupButton{new QPushButton(tr("Lookup metadata…"), this)}
    , m_setupButton{new QPushButton(tr("Converter Setup…"), this)}
    , m_ripButton{new QPushButton(tr("Rip"), this)}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Rip Audio CD"));
    resize(700, 650);
    setModal(false);

    auto* albumGroup  = new QGroupBox(tr("Album information"), this);
    auto* albumLayout = new QGridLayout(albumGroup);

    const Track firstTrack = m_tracks.empty() ? Track{} : m_tracks.front();
    m_albumArtist->setText(displayAlbumArtists(firstTrack));
    m_albumTitle->setText(firstTrack.album());
    m_discNumber->setText(firstTrack.discNumber());
    m_genre->setText(displayMetadataValues(firstTrack.genres()));
    m_date->setText(firstTrack.date());

    const auto addAlbumField = [albumGroup, albumLayout](const QString& text, QLineEdit* field, int row, int column) {
        auto* label = new QLabel(text + u":"_s, albumGroup);
        albumLayout->addWidget(label, row, column);
        albumLayout->addWidget(field, row, column + 1);
    };

    addAlbumField(tr("Album artist"), m_albumArtist, 0, 0);
    addAlbumField(tr("Album title"), m_albumTitle, 0, 2);
    addAlbumField(tr("Genre"), m_genre, 1, 0);
    addAlbumField(tr("Date"), m_date, 1, 2);

    auto* discNumberLabel = new QLabel(tr("Disc number") + u":"_s, albumGroup);
    m_discNumber->setMaximumWidth(100);
    albumLayout->addWidget(discNumberLabel, 2, 0);
    albumLayout->addWidget(m_discNumber, 2, 1, Qt::AlignLeft);
    albumLayout->addWidget(m_lookupButton, 2, 2, 1, 2, Qt::AlignRight);
    albumLayout->setColumnStretch(1, 1);
    albumLayout->setColumnStretch(3, 1);

    auto* trackGroup = new QGroupBox(tr("Track information"), this);
    m_trackTable->setColumnCount(4);
    m_trackTable->setRowCount(static_cast<int>(m_tracks.size()));
    m_trackTable->setHorizontalHeaderLabels({QString{}, tr("#"), tr("Title"), tr("Artist")});
    m_trackTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_trackTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_trackTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_trackTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_trackTable->verticalHeader()->hide();
    m_trackTable->setAlternatingRowColors(true);
    m_trackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackTable->setContextMenuPolicy(Qt::CustomContextMenu);

    for(int row{0}; std::cmp_less(row, m_tracks.size()); ++row) {
        const Track& track = m_tracks.at(static_cast<size_t>(row));

        auto* selected = new QTableWidgetItem();
        selected->setCheckState(Qt::Checked);
        selected->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        selected->setTextAlignment(Qt::AlignCenter);

        auto* number = new QTableWidgetItem(track.trackNumber());
        number->setFlags(number->flags() & ~Qt::ItemIsEditable);
        number->setTextAlignment(Qt::AlignCenter);

        m_trackTable->setItem(row, 0, selected);
        m_trackTable->setItem(row, 1, number);
        m_trackTable->setItem(row, 2, new QTableWidgetItem(track.effectiveTitle()));
        m_trackTable->setItem(row, 3, new QTableWidgetItem(displayMetadataValues(track.artists())));
    }

    auto* trackLayout = new QVBoxLayout(trackGroup);
    trackLayout->addWidget(m_trackTable);

    m_accurateRipStatus->setWordWrap(true);
    m_accurateRipStatus->hide();

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(m_setupButton, QDialogButtonBox::ActionRole);
    buttons->addButton(m_ripButton, QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Close);

    auto* layout = new QGridLayout(this);
    layout->setVerticalSpacing(8);

    layout->addWidget(albumGroup, 0, 0, 1, 3);
    layout->addWidget(trackGroup, 1, 0, 1, 3);
    layout->addWidget(m_verifyAccurateRip, 2, 0);
    layout->addWidget(new QLabel(tr("Preset") + u":"_s, this), 2, 1, Qt::AlignRight);
    layout->addWidget(m_presets, 2, 2);
    layout->addWidget(m_accurateRipStatus, 3, 0, 1, 3);
    layout->addWidget(buttons, 4, 0, 1, 3);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);

    for(const auto& preset : presets) {
        m_presets->addItem(preset.name, preset.id);
    }

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_trackTable, &QTableWidget::itemChanged, this, &RipAudioCdDialog::updateActions);
    QObject::connect(m_trackTable, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        auto* menu                = new QMenu(m_trackTable);
        const QAction* checkAll   = menu->addAction(tr("Check all"));
        const QAction* uncheckAll = menu->addAction(tr("Uncheck all"));

        const auto updateCheckState = [&](Qt::CheckState state) {
            const QSignalBlocker blocker{m_trackTable};
            for(int row{0}; row < m_trackTable->rowCount(); ++row) {
                if(QTableWidgetItem* item = m_trackTable->item(row, 0)) {
                    item->setCheckState(state);
                }
            }
            updateActions();
        };

        QObject::connect(checkAll, &QAction::triggered, this, [updateCheckState]() { updateCheckState(Qt::Checked); });
        QObject::connect(uncheckAll, &QAction::triggered, this,
                         [updateCheckState]() { updateCheckState(Qt::Unchecked); });

        menu->popup(m_trackTable->viewport()->mapToGlobal(position));
    });
    QObject::connect(m_presets, &QComboBox::currentIndexChanged, this, &RipAudioCdDialog::updateActions);
    QObject::connect(m_lookupButton, &QPushButton::clicked, this, &RipAudioCdDialog::showMetadataLookup);
    QObject::connect(m_setupButton, &QPushButton::clicked, this, &RipAudioCdDialog::showConverterSetup);
    QObject::connect(m_ripButton, &QPushButton::clicked, this, &RipAudioCdDialog::ripWithPreset);

    m_verifyAccurateRip->setChecked(true);
    m_lookupButton->setAutoDefault(false);
    m_ripButton->setDefault(m_presets->count() > 0);
    m_setupButton->setDefault(m_presets->count() == 0);
    if(!driveOffsetConfigured) {
        m_accurateRipStatus->setText(tr("Drive offset has not been configured. Check Drive settings before ripping."));
        m_accurateRipStatus->show();
    }

    updateActions();
}

TrackList RipAudioCdDialog::selectedTracks() const
{
    return tracksFromTable(true);
}

TrackList RipAudioCdDialog::tracksFromTable(bool selectedOnly) const
{
    TrackList selected;

    const QString albumArtist = m_albumArtist->text().trimmed();
    const QString album       = m_albumTitle->text().trimmed();
    const QString discNumber  = m_discNumber->text().trimmed();
    const QString genre       = m_genre->text().trimmed();
    const QString date        = m_date->text().trimmed();

    for(int row{0}; row < m_trackTable->rowCount() && std::cmp_less(row, m_tracks.size()); ++row) {
        if(selectedOnly && m_trackTable->item(row, 0)->checkState() != Qt::Checked) {
            continue;
        }

        Track track = m_tracks.at(row);

        track.setAlbumArtists(metadataValues(albumArtist));
        track.setAlbum(album);
        track.setDiscNumber(discNumber);
        track.setGenres(metadataValues(genre));
        track.setDate(date);
        track.setTitle(m_trackTable->item(row, 2)->text().trimmed());
        track.setArtists(metadataValues(m_trackTable->item(row, 3)->text().trimmed()));

        selected.push_back(std::move(track));
    }

    return selected;
}

void RipAudioCdDialog::applyLookupTracks(const TrackList& tracks)
{
    const bool sameTracks = tracks.size() == m_tracks.size()
                         && std::ranges::equal(tracks, m_tracks, [](const Track& lhs, const Track& rhs) {
                                return lhs.sameIdentityAs(rhs);
                            });
    if(!sameTracks) {
        return;
    }

    m_tracks = tracks;

    const QSignalBlocker trackBlocker{m_trackTable};

    const Track firstTrack = m_tracks.empty() ? Track{} : m_tracks.front();
    m_albumArtist->setText(displayAlbumArtists(firstTrack));
    m_albumTitle->setText(firstTrack.album());
    m_discNumber->setText(firstTrack.discNumber());
    m_genre->setText(displayMetadataValues(firstTrack.genres()));
    m_date->setText(firstTrack.date());

    for(int row{0}; std::cmp_less(row, m_tracks.size()); ++row) {
        const Track& track = m_tracks.at(row);
        m_trackTable->item(row, 1)->setText(track.trackNumber());
        m_trackTable->item(row, 2)->setText(track.effectiveTitle());
        m_trackTable->item(row, 3)->setText(displayMetadataValues(track.artists()));
    }
    updateActions();
}

void RipAudioCdDialog::updateActions()
{
    const bool hasTracks = !selectedTracks().empty();

    m_lookupButton->setEnabled(!m_tracks.empty() && m_metadataLookupAvailable && !m_metadataLookupPending);
    m_setupButton->setEnabled(hasTracks && !m_ripPending);
    m_ripButton->setEnabled(hasTracks && m_presets->currentIndex() >= 0 && !m_ripPending);
    m_presets->setEnabled(m_presets->count() > 0 && !m_ripPending);
}

void RipAudioCdDialog::showMetadataLookup()
{
    const TrackList tracks = tracksFromTable(false);
    if(tracks.empty() || !m_metadataLookupAvailable || m_metadataLookupPending) {
        return;
    }

    m_metadataLookupPending = true;
    updateActions();
    Q_EMIT metadataLookupRequested(tracks);
}

void RipAudioCdDialog::showConverterSetup()
{
    requestRip(true);
}

void RipAudioCdDialog::ripWithPreset()
{
    requestRip(false);
}

void RipAudioCdDialog::requestRip(bool showSetup)
{
    const TrackList tracks = selectedTracks();
    if(tracks.empty() || m_ripPending) {
        return;
    }

    m_ripPending = true;
    updateActions();

    const QString status = m_verifyAccurateRip->isChecked() ? tr("Looking up disc in AccurateRip…") : QString{};
    m_accurateRipStatus->setText(status);
    m_accurateRipStatus->setVisible(!status.isEmpty());

    Q_EMIT ripRequested(tracks, showSetup ? QString{} : m_presets->currentData().toString(), showSetup,
                        m_verifyAccurateRip->isChecked());
}

void RipAudioCdDialog::metadataLookupFinished()
{
    m_metadataLookupPending = false;
    updateActions();
}

void RipAudioCdDialog::ripStartFailed()
{
    m_ripPending = false;
    updateActions();
}
} // namespace Fooyin::Cdda

#include "moc_ripaudiocddialog.cpp"
