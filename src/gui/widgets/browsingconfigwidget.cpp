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

#include "browsingconfigwidget.h"

#include <gui/trackselectioncontroller.h>
#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItemModel>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
BrowsingActionConfigWidget::BrowsingActionConfigWidget(const BrowsingActionControls& controls, QWidget* parent)
    : QGroupBox{tr("Click actions"), parent}
    , m_controls{controls}
    , m_resultsHint{new HintLabel(
          tr("Showing browsing results makes the results playlist current before a click action runs. "
             "To append to another playlist, turn off automatic switching below and open that playlist."),
          this)}
    , m_resultsAvailable{true}
{
    const bool expanded = controls.expandedDoubleClick;

    auto* layout = new QGridLayout(this);

    int row{0};

    if(expanded) {
        layout->addWidget(new QLabel(tr("Filter items"), this), row, 1);
        layout->addWidget(new QLabel(tr("Expanded tracks"), this), row++, 2);
    }

    layout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    layout->addWidget(controls.doubleClick, row, 1);

    if(expanded) {
        layout->addWidget(controls.expandedDoubleClick, row, 2);
    }

    ++row;
    layout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    layout->addWidget(controls.middleClick, row, 1);

    if(expanded) {
        layout->addWidget(controls.expandedMiddleClick, row, 2);
    }

    ++row;

    controls.startPlayback->setText(tr("Start playback after sending tracks"));

    layout->addWidget(controls.startPlayback, row++, 0, 1, expanded ? 3 : 2);

    auto* currentPlaylistHint = new HintLabel(
        tr("‘Current playlist’ is the playlist currently shown, which may differ from the playing playlist."), this);
    auto* playNowHint = new HintLabel(tr("‘Play now’ plays directly from this browser."), this);

    for(auto* hint : {currentPlaylistHint, m_resultsHint, playNowHint}) {
        hint->setWordWrap(true);
        layout->addWidget(hint, row++, 0, 1, expanded ? 3 : 2);
    }

    layout->setColumnStretch(1, 1);
    if(expanded) {
        layout->setColumnStretch(2, 1);
    }

    for(const auto* combo :
        {controls.doubleClick, controls.middleClick, controls.expandedDoubleClick, controls.expandedMiddleClick}) {
        if(combo) {
            QObject::connect(combo, &QComboBox::currentIndexChanged, this, &BrowsingActionConfigWidget::updateControls);
        }
    }

    if(controls.resultsEnabled) {
        QObject::connect(controls.resultsEnabled, &QCheckBox::toggled, this,
                         &BrowsingActionConfigWidget::updateControls);
    }

    updateControls();
}

void BrowsingActionConfigWidget::addResultsActions(QComboBox* combo)
{
    TrackSelectionController::addAction(combo, tr("Play in results playlist"), TrackAction::PlayResultsPlaylist);
    TrackSelectionController::addAction(combo, tr("Replace results playlist"), TrackAction::ReplaceResultsPlaylist);
}

void BrowsingActionConfigWidget::setResultsAvailable(bool available)
{
    m_resultsAvailable = available;
    updateControls();
}

void BrowsingActionConfigWidget::updateControls()
{
    bool supportsPlayback{false};
    const std::array combos{m_controls.doubleClick, m_controls.middleClick, m_controls.expandedDoubleClick,
                            m_controls.expandedMiddleClick};
    for(const auto* combo : combos) {
        if(!combo) {
            continue;
        }
        int value = combo->currentData().toInt();
        if(value < 0) {
            value = (combo == m_controls.expandedDoubleClick ? m_controls.doubleClick : m_controls.middleClick)
                        ->currentData()
                        .toInt();
        }
        const auto action = static_cast<TrackAction>(value);
        supportsPlayback |= combo->isEnabled()
                         && (action == TrackAction::SendCurrentPlaylist || action == TrackAction::SendNewPlaylist
                             || action == TrackAction::ReplaceResultsPlaylist || action == TrackAction::QueueNext
                             || action == TrackAction::SendToQueue);
        if(m_controls.resultsEnabled) {
            if(const auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
                for(const auto resultAction : {TrackAction::PlayResultsPlaylist, TrackAction::ReplaceResultsPlaylist}) {
                    if(auto* item = model->item(combo->findData(static_cast<int>(resultAction)))) {
                        item->setEnabled(m_resultsAvailable);
                    }
                }
            }
        }
    }
    m_controls.startPlayback->setEnabled(supportsPlayback);
    m_resultsHint->setVisible(m_controls.resultsEnabled && m_resultsAvailable);

    for(auto* combo : {m_controls.expandedDoubleClick, m_controls.expandedMiddleClick}) {
        if(combo) {
            const auto* source
                = combo == m_controls.expandedDoubleClick ? m_controls.doubleClick : m_controls.middleClick;
            combo->setItemText(combo->findData(-1), tr("Use filter action (%1)").arg(source->currentText()));
        }
    }
}

ResultsPlaylistConfigWidget::ResultsPlaylistConfigWidget(QCheckBox* enabled, QCheckBox* show, QCheckBox* preserve,
                                                         QLineEdit* name, const BrowsingActionControls& actions,
                                                         QWidget* parent)
    : QGroupBox{tr("Browsing results"), parent}
{
    enabled->setText(tr("Update the results playlist when the selection changes"));
    show->setText(tr("Show the results playlist when the selection changes"));
    preserve->setText(tr("Keep the playing results when browsing"));
    preserve->setToolTip(tr("Keep the playing results in a playlist with ‘(Playback)’ appended to its name."));

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(enabled, row++, 0, 1, 2);
    layout->addWidget(show, row++, 0, 1, 2);
    layout->addWidget(preserve, row++, 0, 1, 2);
    layout->addWidget(new QLabel(tr("Results playlist name") + u":"_s, this), row, 0);
    layout->addWidget(name, row++, 1);
    layout->setColumnStretch(1, 1);

    const auto update = [enabled, show, preserve, name, actions]() {
        bool usesResults{false};
        for(const auto* combo :
            {actions.doubleClick, actions.middleClick, actions.expandedDoubleClick, actions.expandedMiddleClick}) {
            if(combo) {
                const auto action = static_cast<TrackAction>(combo->currentData().toInt());
                usesResults
                    |= action == TrackAction::PlayResultsPlaylist || action == TrackAction::ReplaceResultsPlaylist;
            }
        }
        const bool checked = enabled->isChecked() || usesResults;
        show->setEnabled(enabled->isChecked());
        preserve->setEnabled(checked);
        name->setEnabled(checked);
    };

    QObject::connect(enabled, &QCheckBox::toggled, this, update);

    for(const auto* combo :
        {actions.doubleClick, actions.middleClick, actions.expandedDoubleClick, actions.expandedMiddleClick}) {
        if(combo) {
            QObject::connect(combo, &QComboBox::currentIndexChanged, this, update);
        }
    }

    update();
}
} // namespace Fooyin

#include "moc_browsingconfigwidget.cpp"
