/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "rgscanresults.h"

#include "rgscanresultsmodel.h"

#include <core/library/musiclibrary.h>
#include <utils/utils.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

namespace Fooyin::RGScanner {
RGScanResults::RGScanResults(MusicLibrary* library, TrackList tracks, std::chrono::milliseconds timeTaken,
                             QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_tracks{std::move(tracks)}
    , m_resultsView{new QTableView(this)}
    , m_resultsModel{new RGScanResultsModel(m_tracks, this)}
{
    setWindowTitle(tr("ReplayGain Scan Results"));

    m_resultsView->setModel(m_resultsModel);
    m_resultsView->verticalHeader()->hide();
    m_resultsView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    auto* buttonBox   = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* applyButton = buttonBox->button(QDialogButtonBox::Ok);
    applyButton->setText(tr("&Update File Tags"));
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* timeLabel = new QLabel(tr("Time taken") + u": " + Utils::msToString(timeTaken, false), this);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_resultsView, 0, 0, 1, 2);
    layout->addWidget(timeLabel, 1, 0);
    layout->addWidget(buttonBox, 1, 1);
    layout->setColumnStretch(1, 1);
}

void RGScanResults::accept()
{
    m_library->writeTrackMetadata(m_tracks);
    QDialog::accept();
}

QSize RGScanResults::sizeHint() const
{
    QSize size = m_resultsView->sizeHint();
    size.rheight() += 200;
    size.rwidth() += 400;
    return size;
}

QSize RGScanResults::minimumSizeHint() const
{
    return QDialog::minimumSizeHint();
}
} // namespace Fooyin::RGScanner
