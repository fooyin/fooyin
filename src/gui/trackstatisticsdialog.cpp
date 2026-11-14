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

#include "trackstatisticsdialog.h"

#include <core/coresettings.h>
#include <gui/statusevent.h>
#include <gui/widgets/hintlabel.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimerEvent>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
void TrackStatisticsDialog::openRebuild(MusicLibrary* library, SettingsManager* settings, QWidget* parent)
{
    auto* dialog = new TrackStatisticsDialog(library, settings, {}, false, parent);
    dialog->open();
}

void TrackStatisticsDialog::openReset(MusicLibrary* library, SettingsManager* settings, const TrackList& tracks,
                                      QWidget* parent)
{
    if(tracks.empty()) {
        return;
    }
    auto* dialog = new TrackStatisticsDialog(library, settings, tracks, true, parent);
    dialog->open();
}

void TrackStatisticsDialog::reject()
{
    if(!m_applying) {
        QDialog::reject();
    }
}

void TrackStatisticsDialog::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_progressTimer.timerId()) {
        QDialog::timerEvent(event);
        return;
    }

    if(!m_busy || m_progressFuture.isFinished()) {
        m_progressTimer.stop();
        return;
    }
    if(m_progressElapsed.elapsed() < 400) {
        return;
    }

    if(const QString text = m_progressFuture.progressText(); !text.isEmpty()) {
        m_preview->setText(text);
    }

    const int maximum = std::max(0, m_progressFuture.progressMaximum());
    m_progress->setRange(0, maximum);
    if(maximum > 0) {
        m_progress->setValue(std::max(0, m_progressFuture.progressValue()));
    }

    m_progress->show();
}

TrackStatisticsDialog::TrackStatisticsDialog(MusicLibrary* library, SettingsManager* settings, const TrackList& tracks,
                                             bool resetting, QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_resetting{resetting}
    , m_previewValid{false}
    , m_hasWork{false}
    , m_applying{false}
    , m_busy{false}
    , m_refreshPending{false}
    , m_affectedTrackCount{0}
    , m_script{new ScriptLineEdit(this)}
    , m_restoreOriginal{new QPushButton(tr("Restore original grouping"), this)}
    , m_playcount{new QComboBox(this)}
    , m_rating{new QComboBox(this)}
    , m_loved{new QComboBox(this)}
    , m_split{new QComboBox(this)}
    , m_clearFirst{new QCheckBox(tr("Clear first played"), this)}
    , m_clearLast{new QCheckBox(tr("Clear last played"), this)}
    , m_resetHistory{new QCheckBox(tr("Playcount, first played and last played"), this)}
    , m_resetRating{new QCheckBox(tr("Ratings"), this)}
    , m_resetLoved{new QCheckBox(tr("Loved status"), this)}
    , m_writePlaycount{new QCheckBox(tr("Playcount"), this)}
    , m_writeRating{new QCheckBox(tr("Ratings"), this)}
    , m_affectedTracks{new QLabel(u"—"_s, this)}
    , m_statisticsGroups{new QLabel(u"—"_s, this)}
    , m_mergedGroups{new QLabel(u"—"_s, this)}
    , m_splitGroups{new QLabel(u"—"_s, this)}
    , m_fallbackTracks{new QLabel(u"—"_s, this)}
    , m_orphanedGroups{new QLabel(u"—"_s, this)}
    , m_preview{new QLabel(this)}
    , m_progress{new QProgressBar(this)}
    , m_previewButton{new QPushButton(tr("Preview"), this)}
    , m_applyButton{new QPushButton(resetting ? tr("Reset") : tr("Rebuild"), this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Cancel, this)}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(resetting ? tr("Reset track statistics") : tr("Rebuild track statistics"));

    for(const auto& track : tracks) {
        if(track.id() >= 0) {
            m_trackIds.push_back(track.id());
        }
    }

    auto* details = new QLabel(
        resetting ? tr("Reset the selected statistics for these tracks and all tracks sharing their statistics. "
                       "The date added, listening history and listening-time totals will be preserved.")
                  : tr("Tracks with the same script result share track statistics. "
                       "Choose how existing values are combined below. "
                       "Listening history and listening-time totals will be preserved."),
        this);
    details->setWordWrap(true);

    m_script->setPlaceholderText(tr("Original metadata grouping"));
    m_script->setToolTip(tr("Use stable track metadata. Statistics, clock and playback values are unavailable."));

    auto* identity       = new QGroupBox(tr("Statistics grouping"), this);
    auto* identityLayout = new QFormLayout(identity);

    identityLayout->addRow(tr("Script") + u":"_s, m_script);
    identityLayout->addRow(new HintLabel(tr("Leave the script empty to use the original metadata grouping. "
                                            "Empty results also use this grouping."),
                                         this));
    identityLayout->addRow(m_restoreOriginal);

    m_playcount->addItems({tr("Maximum"), tr("Sum across groups"), tr("Clear")});
    m_rating->addItems({tr("Highest"), tr("Lowest rated"), tr("Clear")});
    m_loved->addItems({tr("Loved if any"), tr("Loved if all"), tr("Clear")});
    m_split->addItems({tr("Copy playcount and played dates to each group"), tr("Clear playcount and played dates")});

    m_clearFirst->setToolTip(
        tr("Clear the first played date for all rebuilt groups, including groups that don’t split."));
    m_clearLast->setToolTip(
        tr("Clear the last played date for all rebuilt groups, including groups that don’t split."));

    auto* clearDates = new QHBoxLayout();
    clearDates->addWidget(m_clearFirst);
    clearDates->addWidget(m_clearLast);
    clearDates->addStretch();

    auto* policies     = new QGroupBox(tr("Merge and split policies"), this);
    auto* policyLayout = new QFormLayout(policies);

    policyLayout->addRow(tr("Playcount") + u":"_s, m_playcount);
    policyLayout->addRow(tr("Rating") + u":"_s, m_rating);
    policyLayout->addRow(tr("Loved status") + u":"_s, m_loved);
    policyLayout->addRow(tr("When a group splits") + u":"_s, m_split);
    policyLayout->addRow(tr("Played dates") + u":"_s, clearDates);
    policyLayout->addRow(
        new HintLabel(tr("Date added and first played use the earliest available dates; last played uses the latest. "
                         "Unrated values are ignored when merging ratings."),
                      this));

    m_resetHistory->setChecked(true);

    auto* resetFields = new QGroupBox(tr("Statistics to reset"), this);
    auto* resetLayout = new QVBoxLayout(resetFields);

    resetLayout->addWidget(m_resetHistory);
    resetLayout->addWidget(m_resetRating);
    resetLayout->addWidget(m_resetLoved);

    m_writePlaycount->setChecked(settings->value<Settings::Core::SavePlaycountToMetadata>());
    m_writeRating->setChecked(settings->value<Settings::Core::SaveRatingToMetadata>());

    auto* fileTags       = new QGroupBox(tr("Write to file tags"), this);
    auto* fileTagLayout  = new QVBoxLayout(fileTags);
    auto* fileTagOptions = new QVBoxLayout();

    fileTagOptions->addWidget(m_writePlaycount);
    fileTagOptions->addWidget(m_writeRating);
    fileTagOptions->addStretch();
    fileTagLayout->addLayout(fileTagOptions);
    fileTagLayout->addWidget(new HintLabel(tr("Applies to this operation only."), this));

    m_fallbackTracks->setToolTip(tr("Tracks whose script result is empty use the original metadata grouping."));
    m_orphanedGroups->setToolTip(tr("Statistics retained for removed tracks will be discarded by the rebuild."));

    auto* summary      = new QGroupBox(resetting ? tr("Affected statistics") : tr("Preview summary"), this);
    auto* countsLayout = new QFormLayout(summary);
    countsLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    countsLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    countsLayout->addRow(tr("Affected tracks"), m_affectedTracks);
    countsLayout->addRow(tr("Statistics groups"), m_statisticsGroups);

    if(!resetting) {
        countsLayout->addRow(tr("Groups merging"), m_mergedGroups);
        countsLayout->addRow(tr("Groups splitting"), m_splitGroups);
        countsLayout->addRow(tr("Tracks using fallback"), m_fallbackTracks);
        countsLayout->addRow(tr("Retained records to discard"), m_orphanedGroups);
    }

    for(auto* value : {m_mergedGroups, m_splitGroups, m_fallbackTracks, m_orphanedGroups}) {
        value->setVisible(!resetting);
    }
    for(auto* value :
        {m_affectedTracks, m_statisticsGroups, m_mergedGroups, m_splitGroups, m_fallbackTracks, m_orphanedGroups}) {
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    m_preview->setMinimumWidth(QFontMetrics{font()}.horizontalAdvance(tr("Calculating statistics…")) * 2);

    m_progress->setFormat(tr("%v of %m"));
    auto progressSizePolicy = m_progress->sizePolicy();
    progressSizePolicy.setRetainSizeWhenHidden(true);
    m_progress->setSizePolicy(progressSizePolicy);
    m_progress->hide();

    m_buttons->addButton(m_previewButton, QDialogButtonBox::ActionRole);
    m_previewButton->setVisible(!resetting);
    m_buttons->addButton(m_applyButton, QDialogButtonBox::AcceptRole);
    m_applyButton->setEnabled(false);

    auto* footer = new QHBoxLayout();
    footer->addWidget(m_preview, 1);
    footer->addWidget(m_buttons);

    auto* cancel = m_buttons->button(QDialogButtonBox::Cancel);
    cancel->setDefault(true);
    cancel->setFocus();

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(details);
    layout->addWidget(identity);
    layout->addWidget(policies);
    layout->addWidget(resetFields);
    layout->addWidget(fileTags);
    layout->addWidget(summary);
    layout->addWidget(m_progress);
    layout->addStretch();
    layout->addLayout(footer);

    identity->setVisible(!resetting);
    policies->setVisible(!resetting);
    resetFields->setVisible(resetting);

    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_previewButton, &QPushButton::clicked, this, &TrackStatisticsDialog::preview);
    QObject::connect(m_applyButton, &QPushButton::clicked, this, &TrackStatisticsDialog::apply);

    QObject::connect(m_restoreOriginal, &QPushButton::clicked, this,
                     [this]() { m_script->setScript(defaultPlaybackStatisticsScript()); });

    if(!resetting) {
        QObject::connect(m_script, &QLineEdit::textChanged, this, &TrackStatisticsDialog::invalidatePreview);
        for(const auto* combo : {m_playcount, m_rating, m_loved, m_split}) {
            QObject::connect(combo, &QComboBox::currentIndexChanged, this, &TrackStatisticsDialog::invalidatePreview);
        }
        for(const auto* check : {m_clearFirst, m_clearLast}) {
            QObject::connect(check, &QCheckBox::toggled, this, &TrackStatisticsDialog::invalidatePreview);
        }
    }

    for(const auto* check : {m_resetHistory, m_resetRating, m_resetLoved}) {
        QObject::connect(check, &QCheckBox::toggled, this, &TrackStatisticsDialog::invalidatePreview);
    }

    QObject::connect(m_playcount, &QComboBox::currentIndexChanged, this, [this](int index) {
        if(index == static_cast<int>(PlaycountMergePolicy::Clear)) {
            m_clearFirst->setChecked(true);
            m_clearLast->setChecked(true);
        }
    });

    QObject::connect(library, &MusicLibrary::tracksAdded, this, &TrackStatisticsDialog::invalidatePreview);
    QObject::connect(library, &MusicLibrary::tracksMetadataChanged, this, &TrackStatisticsDialog::invalidatePreview);
    QObject::connect(library, &MusicLibrary::tracksStatsChanged, this, &TrackStatisticsDialog::invalidatePreview);
    QObject::connect(library, &MusicLibrary::tracksDeleted, this, &TrackStatisticsDialog::invalidatePreview);

    if(resetting) {
        preview();
        return;
    }

    setBusy(true);
    m_preview->setText(tr("Loading statistics configuration…"));

    auto future = library->managePlaybackStatistics({});
    trackProgress(future);
    future.then(this, [this](const PlaybackStatisticsResult& result) {
        setBusy(false);

        if(!result.error.isEmpty()) {
            m_preview->setText(result.error);
            return;
        }

        m_savedConfig = result.config;
        m_script->setScript(result.config.script);
        m_playcount->setCurrentIndex(static_cast<int>(result.config.playcount));
        m_rating->setCurrentIndex(static_cast<int>(result.config.rating));
        m_loved->setCurrentIndex(static_cast<int>(result.config.loved));
        m_split->setCurrentIndex(result.config.clearSplitHistory ? 1 : 0);
        m_clearFirst->setChecked(result.config.clearFirstPlayed);
        m_clearLast->setChecked(result.config.clearLastPlayed);

        preview();
    });

    QObject::connect(m_restoreOriginal, &QPushButton::clicked, this,
                     [this]() { m_script->setScript(defaultPlaybackStatisticsScript()); });
}

PlaybackStatisticsRequest TrackStatisticsDialog::request(bool previewing) const
{
    using Operation = PlaybackStatisticsRequest::Operation;

    PlaybackStatisticsRequest result;

    result.operation                = m_resetting ? previewing ? Operation::PreviewReset : Operation::Reset
                                    : previewing  ? Operation::Preview
                                                  : Operation::Rebuild;
    result.config.script            = m_script->text();
    result.config.playcount         = static_cast<PlaycountMergePolicy>(m_playcount->currentIndex());
    result.config.rating            = static_cast<RatingMergePolicy>(m_rating->currentIndex());
    result.config.loved             = static_cast<LovedMergePolicy>(m_loved->currentIndex());
    result.config.clearFirstPlayed  = m_clearFirst->isChecked();
    result.config.clearLastPlayed   = m_clearLast->isChecked();
    result.config.clearSplitHistory = m_split->currentIndex() == 1;
    result.trackIds                 = m_trackIds;
    result.resetStats               = Track::Stat::None;
    if(m_resetHistory->isChecked()) {
        result.resetStats |= Track::Stat::Playcount;
    }
    if(m_resetRating->isChecked()) {
        result.resetStats |= Track::Stat::Rating;
    }
    if(m_resetLoved->isChecked()) {
        result.resetStats |= Track::Stat::Loved;
    }
    Track::Stats fileTagStats;
    if(m_writePlaycount->isChecked() && (!m_resetting || m_resetHistory->isChecked())) {
        fileTagStats |= Track::Stat::Playcount;
    }
    if(m_writeRating->isChecked() && (!m_resetting || m_resetRating->isChecked())) {
        fileTagStats |= Track::Stat::Rating;
    }
    result.fileTagStats = fileTagStats;

    return result;
}

void TrackStatisticsDialog::preview()
{
    m_refreshPending = false;
    m_previewValid   = false;
    setBusy(true);
    m_preview->setText(m_resetting ? tr("Calculating statistics…") : tr("Calculating preview…"));

    auto future = m_library->managePlaybackStatistics(request(true));
    trackProgress(future);
    future.then(this, [this](const PlaybackStatisticsResult& result) {
        if(m_refreshPending) {
            if(m_resetting) {
                preview();
            }
            else {
                setBusy(false);
            }
            return;
        }
        showResult(result);
        setBusy(false);
    });
}

void TrackStatisticsDialog::apply()
{
    if(!m_previewValid || !m_hasWork) {
        return;
    }

    m_applying = true;
    m_buttons->button(QDialogButtonBox::Cancel)->setEnabled(false);
    setBusy(true);
    m_preview->setText(tr("Updating track statistics…"));

    auto future = m_library->managePlaybackStatistics(request(false));
    trackProgress(future);
    future.then(this, [this](const PlaybackStatisticsResult& result) {
        m_progressTimer.stop();
        m_progress->hide();
        m_preview->clear();
        if(!result.error.isEmpty()) {
            m_applying = false;
            m_buttons->button(QDialogButtonBox::Cancel)->setEnabled(true);
            showResult(result);
            setBusy(false);
            return;
        }

        if(!m_resetting && result.tracks == 0 && result.orphanedGroups == 0) {
            StatusEvent::post(result.config == m_savedConfig ? tr("No changes required")
                                                             : tr("Statistics configuration saved."));
        }
        else {
            StatusEvent::post(tr("Track statistics updated. Affected tracks: %Ln", nullptr, result.tracks));
        }

        if(result.failedTagWrites > 0) {
            QMessageBox::warning(this, tr("Track statistics"),
                                 tr("Statistics were saved to the database. File tag updates failed: %Ln.", nullptr,
                                    result.failedTagWrites));
        }

        accept();
    });
}

void TrackStatisticsDialog::invalidatePreview()
{
    if(m_applying) {
        return;
    }

    m_previewValid   = false;
    m_refreshPending = true;
    updateControls();

    if(m_resetting) {
        if(!m_busy) {
            preview();
        }
        return;
    }

    m_preview->setText(tr("Preview out of date"));
}

void TrackStatisticsDialog::setBusy(bool busy)
{
    m_busy = busy;
    if(!busy) {
        m_progressTimer.stop();
        m_progress->hide();
    }
    m_script->setEnabled(!busy);
    m_restoreOriginal->setEnabled(!busy);
    for(auto* combo : {m_playcount, m_rating, m_loved, m_split}) {
        combo->setEnabled(!busy);
    }
    for(auto* check : {m_clearFirst, m_clearLast, m_resetHistory, m_resetRating, m_resetLoved}) {
        check->setEnabled(!busy);
    }
    m_previewButton->setEnabled(!busy);
    updateControls();
}

void TrackStatisticsDialog::trackProgress(const QFuture<PlaybackStatisticsResult>& future)
{
    m_progressFuture = future;
    m_progressElapsed.start();
    m_progress->setRange(0, 0);
    m_progress->hide();
    m_progressTimer.start(100, this);
}

void TrackStatisticsDialog::updateControls()
{
    m_writePlaycount->setEnabled(!m_busy && (!m_resetting || m_resetHistory->isChecked()));
    m_writeRating->setEnabled(!m_busy && (!m_resetting || m_resetRating->isChecked()));
    const bool hasResetFields = m_resetHistory->isChecked() || m_resetRating->isChecked() || m_resetLoved->isChecked();
    m_applyButton->setEnabled(!m_busy && m_previewValid && m_hasWork
                              && (!m_resetting || (m_affectedTrackCount > 0 && hasResetFields)));
}

void TrackStatisticsDialog::updateHeight()
{
    // Fit the wrapped content while allowing the dialog's width to change.
    setFixedHeight(std::max(layout()->totalMinimumSize().height(), layout()->totalHeightForWidth(width())));
}

void TrackStatisticsDialog::showResult(const PlaybackStatisticsResult& result)
{
    if(!result.error.isEmpty()) {
        m_previewValid = false;
        m_preview->setText(result.error);
        return;
    }

    const bool configChanged     = !m_resetting && result.config != m_savedConfig;
    const bool configurationOnly = configChanged && result.tracks == 0 && result.orphanedGroups == 0;

    m_previewValid       = true;
    m_affectedTrackCount = result.tracks;
    m_hasWork            = configChanged || result.tracks > 0 || result.orphanedGroups > 0;

    m_applyButton->setText(m_resetting ? tr("Reset") : configurationOnly ? tr("Save") : tr("Rebuild"));
    m_affectedTracks->setText(QString::number(result.tracks));
    m_statisticsGroups->setText(QString::number(result.groups));
    if(!m_resetting) {
        m_mergedGroups->setText(QString::number(result.mergedGroups));
        m_splitGroups->setText(QString::number(result.splitGroups));
        m_fallbackTracks->setText(QString::number(result.fallbackTracks));
        m_orphanedGroups->setText(QString::number(result.orphanedGroups));
    }
    m_preview->setText(!m_hasWork          ? tr("No changes required")
                       : configurationOnly ? tr("Configuration changes only")
                                           : QString{});
}
} // namespace Fooyin

#include "moc_trackstatisticsdialog.cpp"
