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

#include "radioguidewidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QTreeView;

namespace Fooyin::RadioBrowser {
class RadioGuideConfigModel;

class RadioGuideConfigDialog : public WidgetConfigDialog<RadioGuideWidget, RadioGuideWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit RadioGuideConfigDialog(RadioGuideWidget* guideWidget, QWidget* parent = nullptr);

protected:
    [[nodiscard]] RadioGuideWidget::ConfigData config() const override;
    void setConfig(const RadioGuideWidget::ConfigData& config) override;

private:
    void updateActionState();
    [[nodiscard]] QModelIndex currentSection() const;
    [[nodiscard]] QStringList expandedSections() const;
    void restoreExpandedSections(const QStringList& sections);
    void moveCurrentRow(int offset);

    void showContextMenu(const QPoint& pos);
    void populateStartupEntries(const QString& currentKey);

    void addSection();
    void addTag();

    QTreeView* m_tree;
    RadioGuideConfigModel* m_model;

    QAction* m_addSectionAction;
    QAction* m_addTagAction;
    QAction* m_removeAction;
    QAction* m_moveUpAction;
    QAction* m_moveDownAction;

    QCheckBox* m_countries;
    QComboBox* m_startupEntry;
};
} // namespace Fooyin::RadioBrowser
