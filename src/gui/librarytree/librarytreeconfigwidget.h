/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "librarytreewidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace Fooyin {
class ActionManager;
class LibraryTreeGroupRegistry;

class LibraryTreeConfigDialog : public WidgetConfigDialog<LibraryTreeWidget, LibraryTreeWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit LibraryTreeConfigDialog(LibraryTreeWidget* libraryTree, ActionManager* actionManager,
                                     LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent = nullptr);

protected:
    [[nodiscard]] LibraryTreeWidget::ConfigData config() const override;
    void setConfig(const LibraryTreeWidget::ConfigData& config) override;

private:
    ActionManager* m_actionManager;
    LibraryTreeGroupRegistry* m_groupsRegistry;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QCheckBox* m_playbackOnSend;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QCheckBox* m_keepAlive;
    QLineEdit* m_playlistName;

    QCheckBox* m_restoreState;

    QCheckBox* m_animated;
    QCheckBox* m_header;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_altColours;
    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
    QPushButton* m_manageGroupings;
};
} // namespace Fooyin
