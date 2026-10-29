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

#include "metadatachangesdialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Fooyin {
MetadataChangesDialog::MetadataChangesDialog(TrackList tracks, std::vector<FieldChange> changes, QWidget* parent)
    : QDialog{parent}
    , m_tracks{std::move(tracks)}
    , m_changes{std::move(changes)}
    , m_files{new QTableWidget(this)}
    , m_changesTable{new QTableWidget(this)}
{
    setWindowTitle(tr("Metadata Changes"));
    resize(1050, 560);

    std::vector<size_t> trackIndexes;
    for(const auto& change : m_changes) {
        if(change.localIndex < m_tracks.size()
           && std::ranges::find(trackIndexes, change.localIndex) == trackIndexes.cend()) {
            trackIndexes.push_back(change.localIndex);
        }
    }

    const QString changeSummary = tr("%Ln metadata change(s)", nullptr, static_cast<int>(m_changes.size()));
    const QString fileSummary   = tr("%Ln file(s)", nullptr, static_cast<int>(trackIndexes.size()));
    auto* summary               = new QLabel(tr("%1 across %2.").arg(changeSummary, fileSummary), this);

    m_files->setColumnCount(2);
    m_files->setRowCount(static_cast<int>(trackIndexes.size()));
    m_files->setHorizontalHeaderLabels({tr("File"), tr("Changes")});
    m_files->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_files->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_files->setSelectionMode(QAbstractItemView::SingleSelection);
    m_files->setAlternatingRowColors(true);
    m_files->verticalHeader()->hide();
    m_files->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_files->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    for(size_t row{0}; row < trackIndexes.size(); ++row) {
        const size_t trackIndex = trackIndexes.at(row);
        const Track& track      = m_tracks.at(trackIndex);
        const auto changeCount  = std::ranges::count(m_changes, trackIndex, &FieldChange::localIndex);

        auto* fileItem = new QTableWidgetItem(track.filenameExt());
        fileItem->setToolTip(track.filepath());
        fileItem->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(trackIndex));
        m_files->setItem(static_cast<int>(row), 0, fileItem);

        auto* countItem = new QTableWidgetItem(QString::number(changeCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        m_files->setItem(static_cast<int>(row), 1, countItem);
    }

    m_changesTable->setColumnCount(4);
    m_changesTable->setHorizontalHeaderLabels({tr("Change"), tr("Tag"), tr("Current value"), tr("New value")});
    m_changesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_changesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_changesTable->setAlternatingRowColors(true);
    m_changesTable->setWordWrap(false);
    m_changesTable->setTextElideMode(Qt::ElideRight);
    m_changesTable->verticalHeader()->hide();
    m_changesTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_changesTable->verticalHeader()->setDefaultSectionSize(m_changesTable->fontMetrics().height() + 8);
    m_changesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_changesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_changesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_changesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    QObject::connect(m_files, &QTableWidget::currentCellChanged, this,
                     [this](int currentRow, int, int, int) { populateChanges(currentRow); });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_files);
    splitter->addWidget(m_changesTable);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({260, 790});

    if(!trackIndexes.empty()) {
        m_files->selectRow(0);
        populateChanges(0);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary);
    layout->addWidget(splitter, 1);
    layout->addWidget(buttons);
}

void MetadataChangesDialog::populateChanges(int row)
{
    const auto* fileItem = m_files->item(row, 0);
    if(!fileItem) {
        m_changesTable->setRowCount(0);
        return;
    }

    const size_t trackIndex = fileItem->data(Qt::UserRole).toULongLong();
    std::vector<const FieldChange*> trackChanges;
    for(const auto& change : m_changes) {
        if(change.localIndex == trackIndex) {
            trackChanges.push_back(&change);
        }
    }

    m_changesTable->setRowCount(static_cast<int>(trackChanges.size()));
    for(int rowIndex{0}; std::cmp_less(rowIndex, trackChanges.size()); ++rowIndex) {
        const auto& change   = *trackChanges.at(rowIndex);
        const QString status = change.before.isEmpty() ? tr("Added")
                             : change.after.isEmpty()  ? tr("Removed")
                                                       : tr("Modified");

        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_changesTable->setItem(rowIndex, 0, statusItem);
        m_changesTable->setItem(rowIndex, 1, new QTableWidgetItem(change.field));

        auto* before = new QTableWidgetItem(change.before.isEmpty() ? tr("(empty)") : change.before.simplified());
        before->setToolTip(change.before);
        m_changesTable->setItem(rowIndex, 2, before);

        auto* after = new QTableWidgetItem(change.after.isEmpty() ? tr("(empty)") : change.after.simplified());
        after->setToolTip(change.after);
        m_changesTable->setItem(rowIndex, 3, after);
    }
}
} // namespace Fooyin

#include "moc_metadatachangesdialog.cpp"
