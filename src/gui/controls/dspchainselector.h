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

#include <gui/fywidget.h>

class QContextMenuEvent;
class QLabel;
class QJsonObject;
class QPoint;

namespace Fooyin {
class DspChainStore;
class DspPresetRegistry;
class ExpandingComboBox;
class SettingsManager;

class DspChainSelector : public FyWidget
{
    Q_OBJECT

public:
    explicit DspChainSelector(DspChainStore* chainStore, DspPresetRegistry* presetRegistry, SettingsManager* settings,
                              QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void reload();
    void setShowLabel(bool showLabel);
    void showContextMenu(const QPoint& globalPos);

    DspChainStore* m_chainStore;
    DspPresetRegistry* m_presetRegistry;
    SettingsManager* m_settings;
    QLabel* m_label;
    ExpandingComboBox* m_combo;
    bool m_showLabel;
};
} // namespace Fooyin
