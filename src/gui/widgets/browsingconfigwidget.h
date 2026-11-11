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

#pragma once

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace Fooyin {
class HintLabel;
struct BrowsingActionControls
{
    QComboBox* doubleClick{nullptr};
    QComboBox* middleClick{nullptr};
    QCheckBox* startPlayback{nullptr};
    QCheckBox* resultsEnabled{nullptr};
    // TODO: Generalise these
    QComboBox* expandedDoubleClick{nullptr};
    QComboBox* expandedMiddleClick{nullptr};
};

class BrowsingActionConfigWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit BrowsingActionConfigWidget(const BrowsingActionControls& controls, QWidget* parent = nullptr);

    void setResultsAvailable(bool available);

    static void addResultsActions(QComboBox* combo);

private:
    void updateControls();

    BrowsingActionControls m_controls;
    HintLabel* m_resultsHint;
    bool m_resultsAvailable;
};

class ResultsPlaylistConfigWidget : public QGroupBox
{
    Q_OBJECT

public:
    ResultsPlaylistConfigWidget(QCheckBox* enabled, QCheckBox* show, QCheckBox* preserve, QLineEdit* name,
                                const BrowsingActionControls& actions, QWidget* parent = nullptr);
};
} // namespace Fooyin
