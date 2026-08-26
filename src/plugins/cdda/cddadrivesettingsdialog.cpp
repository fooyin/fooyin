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

#include "cddadrivesettingsdialog.h"

#include "accuraterip.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
CdDriveSettingsDialog::CdDriveSettingsDialog(CdDriveInfo drive, std::shared_ptr<CdDriveSettingsStore> settingsStore,
                                             std::shared_ptr<NetworkAccessManager> networkAccess, QWidget* parent)
    : QDialog{parent}
    , m_drive{std::move(drive)}
    , m_settingsStore{std::move(settingsStore)}
    , m_networkAccess{std::move(networkAccess)}
    , m_readOffset{new QSpinBox(this)}
    , m_security{new QComboBox(this)}
    , m_speedLimit{new QComboBox(this)}
    , m_lookupOffset{new QPushButton(tr("Lookup with AccurateRip"), this)}
    , m_offsetStatus{new QLabel(this)}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Drive Settings - %1").arg(m_drive.displayName));
    setModal(true);

    m_readOffset->setRange(-MaximumReadOffsetFrames, MaximumReadOffsetFrames);
    m_readOffset->setToolTip(
        tr("Compensates for a drive that reads slightly before or after the requested CD position.\n"
           "Positive values read later; negative values read earlier.\n"
           "Use AccurateRip to look up the correction for this drive model, or enter a known value manually."));

    m_security->addItem(tr("Disabled"), static_cast<int>(CdRippingSecurity::Disabled));
    m_security->addItem(tr("Standard"), static_cast<int>(CdRippingSecurity::Standard));
    m_security->addItem(tr("Paranoid"), static_cast<int>(CdRippingSecurity::Paranoid));
    m_security->setToolTip(
        tr("Controls error detection and correction while ripping; playback is unaffected.\n"
           "Disabled: direct reads with no verification (fastest).\n"
           "Standard: verifies overlapping reads and retries inconsistencies (slower).\n"
           "Paranoid: performs the most thorough available checking and additional retries (slowest)."));

    m_speedLimit->addItem(tr("Maximum"), 0);
    for(const int speed : {1, 2, 4, 8, 16, 24, 32, 40, 48, 52}) {
        m_speedLimit->addItem(u"%1×"_s.arg(speed), speed);
    }
    m_speedLimit->setEnabled(m_drive.supportsSpeedLimit);
    m_speedLimit->setToolTip(m_drive.supportsSpeedLimit
                                 ? tr("Limits the drive's read speed while ripping; playback is unaffected.")
                                 : tr("This drive doesn't support read-speed control."));

    const CdDriveSettings settings = m_settingsStore->settingsForDrive(m_drive);
    m_readOffset->setValue(settings.readOffsetFrames);
    m_security->setCurrentIndex(m_security->findData(static_cast<int>(settings.security)));
    m_speedLimit->setCurrentIndex(std::max(0, m_speedLimit->findData(settings.readSpeedLimit)));

    auto* form = new QGridLayout();

    int row{0};
    form->addWidget(new QLabel(tr("Read offset correction") + u":"_s, this), row, 0);
    form->addWidget(m_readOffset, row++, 1);
    form->addWidget(m_lookupOffset, row++, 1);
    form->addWidget(new QLabel(tr("Ripping security") + u":"_s, this), row, 0);
    form->addWidget(m_security, row++, 1);
    form->addWidget(new QLabel(tr("Drive speed limit") + u":"_s, this), row, 0);
    form->addWidget(m_speedLimit, row++, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &CdDriveSettingsDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_lookupOffset, &QPushButton::clicked, this, &CdDriveSettingsDialog::lookupAccurateRipOffset);

    m_lookupOffset->setEnabled(!m_drive.model.isEmpty());
    m_offsetStatus->setWordWrap(true);
    m_offsetStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_offsetStatus->setMinimumHeight(m_offsetStatus->fontMetrics().lineSpacing() * 3);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_offsetStatus);
    layout->addWidget(buttons);
}

void CdDriveSettingsDialog::lookupAccurateRipOffset()
{
    if(m_drive.model.isEmpty()) {
        return;
    }

    m_lookupOffset->setEnabled(false);
    m_offsetStatus->setText(tr("Looking up drive…"));

    const QNetworkRequest request = makeNetworkRequest(QUrl{u"https://www.accuraterip.com/driveoffsets.htm"_s});

    QNetworkReply* reply = m_networkAccess->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_lookupOffset->setEnabled(true);

        if(reply->error() != QNetworkReply::NoError) {
            m_offsetStatus->setText(tr("Lookup failed: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray payload = reply->read((4 * 1024 * 1024) + 1);
        if(payload.size() > 4LL * 1024 * 1024) {
            m_offsetStatus->setText(tr("AccurateRip returned an unexpectedly large drive list."));
            return;
        }

        const auto match
            = findAccurateRipDriveOffset(parseAccurateRipDriveOffsets(payload), m_drive.vendor, m_drive.model);
        if(!match) {
            m_offsetStatus->setText(tr("No unambiguous entry was found for this drive."));
            return;
        }

        if(match->purged) {
            m_offsetStatus->setText(tr("This drive was purged because its offset is not consistent."));
            return;
        }

        m_readOffset->setValue(match->correctionSampleFrames);
        //: Correction = CD Drive read offset correction
        m_offsetStatus->setText(
            tr("Found correction: %1 (%Ln submission(s), %2% agreement).", nullptr, match->submissions)
                .arg(match->correctionSampleFrames)
                .arg(match->agreementPercent));
    });
}

void CdDriveSettingsDialog::accept()
{
    const CdDriveSettings settings{
        .readOffsetFrames = m_readOffset->value(),
        .security         = static_cast<CdRippingSecurity>(m_security->currentData().toInt()),
        .readSpeedLimit   = m_speedLimit->currentData().toInt(),
    };

    m_settingsStore->setSettingsForDrive(m_drive, settings);

    QDialog::accept();
}
} // namespace Fooyin::Cdda
