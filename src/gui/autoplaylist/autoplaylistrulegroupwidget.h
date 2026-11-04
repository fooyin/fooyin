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

#include <QFrame>

class QComboBox;
class QToolButton;
class QVBoxLayout;

namespace Fooyin {
class AutoPlaylistRuleRowWidget;

class FYGUI_EXPORT AutoPlaylistRuleGroupWidget : public QFrame
{
    Q_OBJECT

public:
    explicit AutoPlaylistRuleGroupWidget(bool root, QWidget* parent);

    [[nodiscard]] AutoPlaylistRuleGroup group() const;
    [[nodiscard]] bool allTracks() const;
    [[nodiscard]] QString error(const QString& path = {}) const;

    void setGroup(const AutoPlaylistRuleGroup& group, bool allTracks = false);

Q_SIGNALS:
    void changed();
    void removeRequested();

private:
    void addItem(const AutoPlaylistRuleGroup::Item& item);
    void removeItem(QWidget* item);

    bool m_root;
    QComboBox* m_match;
    QToolButton* m_expand;
    QWidget* m_body;
    QVBoxLayout* m_ruleLayout;
    std::vector<std::variant<AutoPlaylistRuleRowWidget*, AutoPlaylistRuleGroupWidget*>> m_items;
};
} // namespace Fooyin
