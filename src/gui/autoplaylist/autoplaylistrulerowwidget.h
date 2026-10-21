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

#include "fygui_export.h"

#include "autoplaylistquery.h"

#include <QWidget>

#include <vector>

class QComboBox;
class QLineEdit;
class QDateEdit;
class QSpinBox;
class QStackedWidget;

namespace Fooyin {
class AutoPlaylistFieldComboBox;
class RuleRatingEditor;

class FYGUI_EXPORT AutoPlaylistRuleRowWidget : public QWidget
{
    Q_OBJECT

public:
    AutoPlaylistRuleRowWidget(const AutoPlaylistRule& rule, QWidget* parent);

    [[nodiscard]] AutoPlaylistRule rule() const;

Q_SIGNALS:
    void changed();
    void removeRequested();

private:
    void updateDateUnits();
    void updateComparisons();
    void updateValue();
    void editList();
    void updateListSummary();

    AutoPlaylistFieldComboBox* m_field;
    QComboBox* m_comparison;
    QStackedWidget* m_values;
    QLineEdit* m_text;
    QSpinBox* m_number;
    RuleRatingEditor* m_rating;
    QSpinBox* m_minutes;
    QSpinBox* m_seconds;
    QDateEdit* m_date;
    QSpinBox* m_period;
    QComboBox* m_unit;
    QLineEdit* m_listSummary;
    std::vector<QString> m_listValues;
};
} // namespace Fooyin
