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

#include "commandbutton.h"

#include <gui/configdialog.h>

#include <QPointer>

class QAction;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace Fooyin {
class ActionManager;
class CommandPickerDialog;
class IconPickerDialog;

class CommandButtonConfigDialog : public WidgetConfigDialog<CommandButton, CommandButton::ConfigData>
{
    Q_OBJECT

public:
    CommandButtonConfigDialog(CommandButton* button, ActionManager* actionManager, QWidget* parent = nullptr);

protected:
    void setConfig(const CommandButton::ConfigData& config) override;
    [[nodiscard]] CommandButton::ConfigData config() const override;

private:
    void chooseCommand();
    void chooseIcon();
    void updateCommandDisplay();
    void updateIconDisplay();
    void updatePreview();

    ActionManager* m_actionManager;

    QLineEdit* m_command;
    QAction* m_commandIconAction;
    QPushButton* m_chooseCommand;
    QLineEdit* m_text;
    QComboBox* m_buttonStyle;
    QPushButton* m_iconPreview;
    QLineEdit* m_iconDescription;
    QPushButton* m_chooseIcon;

    QString m_commandId;
    QString m_iconName;
    QString m_iconPath;

    QPointer<CommandPickerDialog> m_commandPicker;
    QPointer<IconPickerDialog> m_iconPicker;
};
} // namespace Fooyin
