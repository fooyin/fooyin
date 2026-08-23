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

#include "commandbuttonconfigdialog.h"

#include "dialog/commandpickerdialog.h"
#include "dialog/iconpickerdialog.h"

#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
CommandButtonConfigDialog::CommandButtonConfigDialog(CommandButton* button, ActionManager* actionManager,
                                                     QWidget* parent)
    : WidgetConfigDialog{button, tr("Command Button Settings"), parent}
    , m_actionManager{actionManager}
    , m_command{new QLineEdit(this)}
    , m_commandIconAction{new QAction(this)}
    , m_chooseCommand{new QPushButton(tr("Choose…"), this)}
    , m_text{new QLineEdit(this)}
    , m_buttonStyle{new QComboBox(this)}
    , m_iconPreview{new QPushButton(this)}
    , m_iconDescription{new QLineEdit(this)}
    , m_chooseIcon{new QPushButton(tr("Choose…"), this)}
{
    m_command->setReadOnly(true);
    m_command->addAction(m_commandIconAction, QLineEdit::LeadingPosition);
    m_iconDescription->setReadOnly(true);

    m_buttonStyle->addItem(tr("Icon only"), Qt::ToolButtonIconOnly);
    m_buttonStyle->addItem(tr("Text only"), Qt::ToolButtonTextOnly);
    m_buttonStyle->addItem(tr("Text beside icon"), Qt::ToolButtonTextBesideIcon);
    m_buttonStyle->addItem(tr("Text under icon"), Qt::ToolButtonTextUnderIcon);

    QObject::connect(m_chooseCommand, &QPushButton::clicked, this, &CommandButtonConfigDialog::chooseCommand);
    QObject::connect(m_chooseIcon, &QPushButton::clicked, this, &CommandButtonConfigDialog::chooseIcon);
    QObject::connect(m_iconPreview, &QAbstractButton::clicked, this, &CommandButtonConfigDialog::chooseIcon);

    m_iconPreview->setIconSize({64, 64});
    m_iconPreview->setFixedSize(92, 92);
    m_iconPreview->setToolTip(tr("Choose an icon"));
    m_iconPreview->setFlat(false);
    m_iconPreview->setAutoDefault(false);
    m_iconPreview->setDefault(false);

    auto* commandGroup  = new QGroupBox(tr("Button"), this);
    auto* commandLayout = new QGridLayout(commandGroup);

    auto* hint = new QLabel(tr("Select a command, or enter a raw `$cmdlink` id or alias."), this);
    hint->setWordWrap(true);

    int row{0};
    commandLayout->addWidget(new QLabel(tr("Command") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_command, row, 1);
    commandLayout->addWidget(m_chooseCommand, row++, 2);
    commandLayout->addWidget(new QLabel(tr("Text") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_text, row++, 1, 1, 2);
    commandLayout->addWidget(new QLabel(tr("Display") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_buttonStyle, row++, 1, 1, 2);
    commandLayout->addWidget(hint, row++, 0, 1, 3);
    commandLayout->setColumnStretch(1, 1);

    auto* iconGroup    = new QGroupBox(tr("Icon"), this);
    auto* iconLayout   = new QGridLayout(iconGroup);
    auto* previewLabel = new QLabel(tr("Preview") + u":"_s, this);

    auto* iconHint = new QLabel(
        tr("Choose a built-in icon or custom image. If none is set, the button uses the command's default icon."),
        this);
    iconHint->setWordWrap(true);

    row = 0;
    iconLayout->addWidget(previewLabel, row, 0, Qt::AlignRight | Qt::AlignVCenter);
    iconLayout->addWidget(m_iconPreview, row++, 1, Qt::AlignLeft | Qt::AlignVCenter);
    iconLayout->addWidget(new QLabel(tr("Selection") + u":"_s, this), row, 0);
    iconLayout->addWidget(m_iconDescription, row, 1);
    iconLayout->addWidget(m_chooseIcon, row++, 2);
    iconLayout->addWidget(iconHint, row++, 0, 1, 3);
    iconLayout->setColumnStretch(1, 1);

    auto* layout{contentLayout()};

    row = 0;
    layout->addWidget(commandGroup, row++, 0);
    layout->addWidget(iconGroup, row++, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    m_command->setPlaceholderText(tr("No command selected"));
    m_text->setPlaceholderText(tr("Use command label"));

    loadCurrentConfig();
    updatePreview();
}

void CommandButtonConfigDialog::chooseCommand()
{
    if(m_commandPicker) {
        m_commandPicker->raise();
        m_commandPicker->activateWindow();
        return;
    }

    m_commandPicker = new CommandPickerDialog(m_actionManager, this);
    m_commandPicker->setAttribute(Qt::WA_DeleteOnClose);
    m_commandPicker->setCommandId(m_commandId);

    QObject::connect(m_commandPicker, &QDialog::accepted, this, [this]() {
        m_commandId = m_commandPicker->commandId();
        updateCommandDisplay();
        updatePreview();
    });

    m_commandPicker->open();
}

void CommandButtonConfigDialog::chooseIcon()
{
    if(m_iconPicker) {
        m_iconPicker->raise();
        m_iconPicker->activateWindow();
        return;
    }

    const CommandButton::ConfigData commandIconConfig{
        .commandId       = m_commandId,
        .text            = {},
        .iconName        = {},
        .iconPath        = {},
        .toolButtonStyle = Qt::ToolButtonIconOnly,
    };

    m_iconPicker = new IconPickerDialog(widget()->previewIcon(commandIconConfig), this);
    m_iconPicker->setAttribute(Qt::WA_DeleteOnClose);
    m_iconPicker->setSelection({.themeName = m_iconName, .customPath = m_iconPath});

    QObject::connect(m_iconPicker, &QDialog::accepted, this, [this]() {
        const IconSelection selection = m_iconPicker->selection();
        m_iconName                    = selection.themeName;
        m_iconPath                    = selection.customPath;
        updateIconDisplay();
        updatePreview();
    });

    m_iconPicker->open();
}

void CommandButtonConfigDialog::updateCommandDisplay()
{
    m_command->setText(CommandPickerDialog::displayText(m_actionManager, m_commandId));
    m_command->setToolTip(m_commandId);
    m_commandIconAction->setIcon(CommandPickerDialog::displayIcon(m_actionManager, m_commandId));
}

void CommandButtonConfigDialog::updateIconDisplay()
{
    if(!m_iconPath.isEmpty()) {
        m_iconDescription->setText(tr("Custom image - %1").arg(QFileInfo{m_iconPath}.fileName()));
        m_iconDescription->setToolTip(m_iconPath);
    }
    else if(!m_iconName.isEmpty()) {
        m_iconDescription->setText(m_iconName);
        m_iconDescription->setToolTip(m_iconName);
    }
    else {
        m_iconDescription->setText(tr("Use command icon"));
        m_iconDescription->setToolTip({});
    }
}

void CommandButtonConfigDialog::updatePreview()
{
    const CommandButton::ConfigData previewConfig{
        .commandId = m_commandId,
        .text      = m_text->text(),
        .iconName  = m_iconName,
        .iconPath  = m_iconPath,
    };

    m_iconPreview->setIcon(widget()->previewIcon(previewConfig));
}

void CommandButtonConfigDialog::setConfig(const CommandButton::ConfigData& config)
{
    m_commandId = config.commandId.trimmed();
    m_iconName  = config.iconName.trimmed();
    m_iconPath  = m_iconName.isEmpty() ? config.iconPath.trimmed() : QString{};

    m_text->setText(config.text);
    if(const int index = m_buttonStyle->findData(config.toolButtonStyle); index >= 0) {
        m_buttonStyle->setCurrentIndex(index);
    }
    updateCommandDisplay();
    updateIconDisplay();
    updatePreview();
}

CommandButton::ConfigData CommandButtonConfigDialog::config() const
{
    return {
        .commandId       = m_commandId,
        .text            = m_text->text(),
        .iconName        = m_iconName,
        .iconPath        = m_iconPath,
        .toolButtonStyle = m_buttonStyle->currentData().toInt(),
    };
}
} // namespace Fooyin
