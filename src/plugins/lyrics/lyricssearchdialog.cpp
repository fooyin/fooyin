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

#include "lyricssearchdialog.h"

#include "lyricsfinder.h"
#include "lyricsparser.h"
#include "lyricssaver.h"
#include "lyricssearchmodel.h"

#include <core/coresettings.h>
#include <gui/widgets/lineediteditor.h>
#include <utils/utils.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTreeView>

using namespace Qt::StringLiterals;

constexpr auto SearchDialogStateGroup  = "Lyrics"_L1;
constexpr auto SearchDialogHeaderState = "Lyrics/SearchDialogHeaderState"_L1;
constexpr auto SearchDialogSplitter    = "Lyrics/SearchDialogSplitter"_L1;

namespace Fooyin::Lyrics {
class LyricsSearchProxyModel : public QSortFilterProxyModel
{
public:
    explicit LyricsSearchProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel{parent}
    {
        setSortCaseSensitivity(Qt::CaseInsensitive);
        setDynamicSortFilter(true);
    }

protected:
    [[nodiscard]] bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override
    {
        const QVariant left  = sourceModel()->data(sourceLeft, Qt::DisplayRole);
        const QVariant right = sourceModel()->data(sourceRight, Qt::DisplayRole);

        return QString::localeAwareCompare(left.toString(), right.toString()) < 0;
    }
};

LyricsSearchDialog::LyricsSearchDialog(const Track& track, std::shared_ptr<NetworkAccessManager> networkAccess,
                                       LyricsSaver* lyricsSaver, SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_track{track}
    , m_lyricsSaver{lyricsSaver}
    , m_finder{new LyricsFinder(std::move(networkAccess), settings, this)}
    , m_titleEdit{new LineEditEditor(tr("Title"), this)}
    , m_albumEdit{new LineEditEditor(tr("Album"), this)}
    , m_artistEdit{new LineEditEditor(tr("Artist"), this)}
    , m_searchButton{new QPushButton(tr("Search"), this)}
    , m_statusLabel{new QLabel(this)}
    , m_resultsTable{new QTreeView(this)}
    , m_resultsModel{new LyricsSearchModel(this)}
    , m_resultsProxyModel{new LyricsSearchProxyModel(this)}
    , m_preview{new QPlainTextEdit(this)}
    , m_splitter{new QSplitter(Qt::Horizontal, this)}
    , m_okButton{nullptr}
    , m_applyButton{nullptr}
{
    setWindowTitle(tr("Search for Lyrics"));

    m_titleEdit->setText(track.title());
    m_albumEdit->setText(track.album());
    m_artistEdit->setText(track.artist());

    LineEditEditor::alignLabels({m_titleEdit, m_albumEdit, m_artistEdit});

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(m_splitter, 1);

    auto* resultsWidget = new QWidget(m_splitter);
    auto* resultsLayout = new QVBoxLayout(resultsWidget);
    resultsLayout->setContentsMargins(5, 0, 5, 0);

    auto* searchLayout = new QGridLayout();
    searchLayout->addWidget(m_artistEdit, 0, 0);
    searchLayout->addWidget(m_albumEdit, 0, 1);
    searchLayout->addWidget(m_titleEdit, 1, 0);
    searchLayout->addWidget(m_searchButton, 1, 1, Qt::AlignLeft);
    searchLayout->setColumnStretch(0, 1);
    searchLayout->setColumnStretch(1, 1);
    resultsLayout->addLayout(searchLayout);

    m_resultsProxyModel->setSourceModel(m_resultsModel);
    m_resultsTable->setModel(m_resultsProxyModel);

    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSortingEnabled(true);
    m_resultsTable->setRootIsDecorated(false);

    auto* header = m_resultsTable->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_resultsTable->sortByColumn(0, Qt::AscendingOrder);

    resultsLayout->addWidget(m_resultsTable, 1);
    m_splitter->addWidget(resultsWidget);

    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    auto* buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    m_okButton    = buttonBox->button(QDialogButtonBox::Ok);
    m_applyButton = buttonBox->button(QDialogButtonBox::Apply);
    m_statusLabel->setTextFormat(Qt::PlainText);

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addWidget(m_statusLabel);
    footerLayout->addStretch();
    footerLayout->addWidget(buttonBox);
    layout->addLayout(footerLayout);

    QObject::connect(m_searchButton, &QPushButton::clicked, this, &LyricsSearchDialog::search);
    QObject::connect(m_titleEdit, &LineEditEditor::returnPressed, this, &LyricsSearchDialog::search);
    QObject::connect(m_albumEdit, &LineEditEditor::returnPressed, this, &LyricsSearchDialog::search);
    QObject::connect(m_artistEdit, &LineEditEditor::returnPressed, this, &LyricsSearchDialog::search);
    QObject::connect(m_resultsTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection&, const QItemSelection&) { updateSelection(); });
    QObject::connect(m_preview, &QPlainTextEdit::textChanged, this, &LyricsSearchDialog::updateActionState);
    QObject::connect(m_resultsTable, &QTreeView::doubleClicked, this, [this]() { accept(); });
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &LyricsSearchDialog::accept);
    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
                     &LyricsSearchDialog::applySelection);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(this, &QDialog::finished, this, &LyricsSearchDialog::saveState);
    QObject::connect(m_finder, &LyricsFinder::lyricsFound, this, &LyricsSearchDialog::addResult);
    QObject::connect(m_finder, &LyricsFinder::lyricsSearchFinished, this, &LyricsSearchDialog::finishSearch);

    QObject::connect(m_lyricsSaver, &LyricsSaver::lyricsSaved, this, [this](const Track& updatedTrack, const Lyrics&) {
        if(updatedTrack.sameIdentityAs(m_track)) {
            m_track = updatedTrack;
        }
    });

    updateActionState();
    restoreState();
    search();
}

void LyricsSearchDialog::accept()
{
    if(applySelection()) {
        QDialog::accept();
    }
}

QSize LyricsSearchDialog::sizeHint() const
{
    return {1100, 560};
}

void LyricsSearchDialog::search()
{
    if(!m_track.isValid()) {
        return;
    }

    m_resultsModel->clear();
    if(auto* selectionModel = m_resultsTable->selectionModel()) {
        selectionModel->clearSelection();
        selectionModel->setCurrentIndex({}, QItemSelectionModel::NoUpdate);
    }
    m_preview->clear();
    m_searchButton->setEnabled(false);
    m_statusLabel->setText(tr("Searching…"));
    m_titleEdit->setFocus();
    updateActionState();

    m_finder->findLyrics({.track                         = m_track,
                          .title                         = m_titleEdit->text().trimmed(),
                          .album                         = m_albumEdit->text().trimmed(),
                          .artist                        = m_artistEdit->text().trimmed(),
                          .localOnly                     = false,
                          .tagOnly                       = false,
                          .stopOnFirstResult             = false,
                          .skipExternalAfterLocalResults = false});
}

void LyricsSearchDialog::addResult(const Track& track, const Lyrics& lyrics)
{
    if(!track.sameIdentityAs(m_track)) {
        return;
    }

    m_resultsModel->addLyrics(lyrics);
    updateActionState();
}

void LyricsSearchDialog::finishSearch(const Track& track, bool foundAny)
{
    if(!track.sameIdentityAs(m_track)) {
        return;
    }

    m_searchButton->setEnabled(true);

    if(foundAny) {
        m_statusLabel->setText(tr("%Ln result(s)", nullptr, m_resultsModel->resultCount()));
    }
    else {
        m_statusLabel->setText(tr("No lyrics found"));
    }

    updateActionState();
}

void LyricsSearchDialog::updateSelection()
{
    if(const auto* lyrics = selectedLyrics()) {
        m_preview->setPlainText(lyrics->data);
    }
    else {
        m_preview->clear();
    }

    updateActionState();
}

void LyricsSearchDialog::updateActionState()
{
    const bool canApply = selectedLyrics() && m_track.isValid() && !m_track.isInArchive();
    if(m_okButton) {
        m_okButton->setEnabled(canApply);
    }
    if(m_applyButton) {
        m_applyButton->setEnabled(canApply);
    }
}

void LyricsSearchDialog::saveState()
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings, SearchDialogStateGroup);
    stateSettings.setValue(SearchDialogHeaderState, m_resultsTable->header()->saveState());
    stateSettings.setValue(SearchDialogSplitter, m_splitter->saveState());
}

void LyricsSearchDialog::restoreState()
{
    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings, SearchDialogStateGroup);

    if(stateSettings.contains(SearchDialogHeaderState)) {
        if(const QByteArray headerState = stateSettings.value(SearchDialogHeaderState).toByteArray();
           !headerState.isEmpty()) {
            m_resultsTable->header()->restoreState(headerState);
        }
    }

    if(stateSettings.contains(SearchDialogSplitter)) {
        if(const QByteArray splitterState = stateSettings.value(SearchDialogSplitter).toByteArray();
           !splitterState.isEmpty()) {
            m_splitter->restoreState(splitterState);
        }
    }
}

bool LyricsSearchDialog::applySelection()
{
    const Lyrics lyrics = editedSelectedLyrics();
    if(!lyrics.isValid() || !m_track.isValid() || m_track.isInArchive()) {
        return false;
    }

    if(!m_lyricsSaver->saveLyrics(lyrics, m_track)) {
        return false;
    }

    m_statusLabel->setText(tr("Saved lyrics from %1").arg(lyrics.source));
    return true;
}

int LyricsSearchDialog::selectedSourceRow() const
{
    auto* selectionModel = m_resultsTable->selectionModel();
    if(!selectionModel) {
        return -1;
    }

    const QModelIndexList rows = selectionModel->selectedRows(0);
    if(rows.isEmpty()) {
        return -1;
    }

    return m_resultsProxyModel->mapToSource(rows.front()).row();
}

const Lyrics* LyricsSearchDialog::selectedLyrics() const
{
    return m_resultsModel->lyricsAt(selectedSourceRow());
}

Lyrics LyricsSearchDialog::editedSelectedLyrics() const
{
    if(const auto* baseLyrics = selectedLyrics()) {
        Lyrics lyrics   = parse(m_preview->toPlainText());
        lyrics.data     = m_preview->toPlainText();
        lyrics.source   = baseLyrics->source;
        lyrics.isLocal  = baseLyrics->isLocal;
        lyrics.tag      = baseLyrics->tag;
        lyrics.filepath = baseLyrics->filepath;

        if(lyrics.metadata.title.isEmpty()) {
            lyrics.metadata.title = baseLyrics->metadata.title;
        }
        if(lyrics.metadata.album.isEmpty()) {
            lyrics.metadata.album = baseLyrics->metadata.album;
        }
        if(lyrics.metadata.artist.isEmpty()) {
            lyrics.metadata.artist = baseLyrics->metadata.artist;
        }
        if(lyrics.offset == 0 && baseLyrics->offset != 0 && !lyrics.data.contains(u"[offset:"_s)) {
            lyrics.offset = baseLyrics->offset;
        }

        return lyrics;
    }

    return {};
}
} // namespace Fooyin::Lyrics
