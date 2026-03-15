/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "commandbutton.h"

#include <gui/configdialog.h>

class QAction;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace Fooyin {
class ActionManager;
class ExpandingComboBox;
class ScriptCommandHandler;

class CommandButtonConfigDialog : public WidgetConfigDialog<CommandButton, CommandButton::ConfigData>
{
    Q_OBJECT

public:
    CommandButtonConfigDialog(CommandButton* button, ActionManager* actionManager, QWidget* parent = nullptr);

protected:
    void setConfig(const CommandButton::ConfigData& config) override;
    [[nodiscard]] CommandButton::ConfigData config() const override;

private:
    void browseForIcon();
    void clearIcon();
    void updatePreview();

    struct CommandOption
    {
        QIcon icon;
        QString label;
        QString id;
        QString category;
        QString description;
    };

    [[nodiscard]] std::vector<CommandOption> buildCommandOptions() const;
    [[nodiscard]] QString currentCommandId() const;

    ActionManager* m_actionManager;

    ExpandingComboBox* m_command;
    QLineEdit* m_text;
    QComboBox* m_buttonStyle;
    QPushButton* m_iconPreview;
    QLineEdit* m_iconPath;
    QAction* m_browseIconAction;
    QAction* m_clearIconAction;
};
} // namespace Fooyin
