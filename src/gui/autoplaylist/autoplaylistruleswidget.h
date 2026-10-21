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

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace Fooyin {
class AutoPlaylistFieldComboBox;
class AutoPlaylistRuleGroupWidget;

class FYGUI_EXPORT AutoPlaylistRulesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AutoPlaylistRulesWidget(QWidget* parent = nullptr);

    [[nodiscard]] AutoPlaylistRules rules() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QWidget* limitWidget() const;
    [[nodiscard]] QWidget* randomWidget() const;

    void setRules(const AutoPlaylistRules& rules);

    static QString comparisonText(Expr::Type comparison, bool negated, bool relative);

Q_SIGNALS:
    void changed();

private:
    AutoPlaylistRuleGroupWidget* m_group;
    AutoPlaylistFieldComboBox* m_sortField;
    QComboBox* m_sortDirection;
    QCheckBox* m_randomOrder;
    QWidget* m_limitWidget;
    QCheckBox* m_limitEnabled;
    QSpinBox* m_limit;
};
} // namespace Fooyin
