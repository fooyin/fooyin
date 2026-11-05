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

#include <gui/widgets/expandedtreeview.h>

#include <QWidget>

#include <vector>

class QSpinBox;
class QCheckBox;
class QComboBox;

namespace Fooyin {
class ExpandedTreeView;

namespace Filters {
class CoverFlowConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CoverFlowConfigWidget(QWidget* parent = nullptr);

    [[nodiscard]] CoverFlowSettings settings() const;
    void setSettings(const CoverFlowSettings& settings);
    void setArtworkSettings(const QSize& size, int cornerRadius);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void settingsChanged();

private:
    void setupPreview();
    void updateControls();

    struct IntegerControl
    {
        int CoverFlowSettings::* member;
        QSpinBox* editor;
    };

    struct BooleanControl
    {
        bool CoverFlowSettings::* member;
        QCheckBox* editor;
    };

    ExpandedTreeView* m_preview;
    QComboBox* m_labelPosition;
    std::vector<IntegerControl> m_integerControls;
    std::vector<BooleanControl> m_booleanControls;
    bool m_updating;
};
} // namespace Filters
} // namespace Fooyin
