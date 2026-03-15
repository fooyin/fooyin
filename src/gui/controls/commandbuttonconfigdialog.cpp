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

#include "commandbuttonconfigdialog.h"

#include "scripting/scriptcommandhandler.h"

#include <gui/guiconstants.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include <QAction>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <vector>

using namespace Qt::StringLiterals;

namespace {
QString tr(const char* text)
{
    return Fooyin::CommandButton::tr(text);
}

QString commandLabel(const QString& category, const QString& description)
{
    return category.isEmpty() ? description : u"%1 | %2"_s.arg(category, description);
}

QString imageFilter()
{
    const auto formats = QImageReader::supportedImageFormats();

    QStringList wildcards;

    for(const QByteArray& format : formats) {
        wildcards.emplace_back(u"*.%1"_s.arg(QString::fromLatin1(format).toLower()));
    }

    wildcards.removeDuplicates();
    wildcards.sort();

    const QString imageFiles
        = wildcards.isEmpty() ? tr("All files (*)") : tr("Images") + u" ("_s + wildcards.join(u' ') + u")"_s;

    return imageFiles + u";;"_s + tr("All files (*)");
}
} // namespace

namespace Fooyin {
CommandButtonConfigDialog::CommandButtonConfigDialog(CommandButton* button, ActionManager* actionManager,
                                                     QWidget* parent)
    : WidgetConfigDialog<CommandButton, CommandButton::ConfigData>{button, tr("Configure %1").arg(button->name()),
                                                                   parent}
    , m_actionManager{actionManager}
    , m_command{new ExpandingComboBox(this)}
    , m_text{new QLineEdit(this)}
    , m_buttonStyle{new QComboBox(this)}
    , m_iconPreview{new QPushButton(this)}
    , m_iconPath{new QLineEdit(this)}
    , m_browseIconAction{new QAction(Utils::iconFromTheme(Constants::Icons::Options), {}, this)}
    , m_clearIconAction{new QAction(Utils::iconFromTheme(Constants::Icons::Clear), {}, this)}
{
    m_command->setEditable(true);
    m_command->setInsertPolicy(QComboBox::NoInsert);

    m_buttonStyle->addItem(tr("Icon only"), Qt::ToolButtonIconOnly);
    m_buttonStyle->addItem(tr("Text only"), Qt::ToolButtonTextOnly);
    m_buttonStyle->addItem(tr("Text beside icon"), Qt::ToolButtonTextBesideIcon);
    m_buttonStyle->addItem(tr("Text under icon"), Qt::ToolButtonTextUnderIcon);

    QObject::connect(m_iconPreview, &QAbstractButton::clicked, this, &CommandButtonConfigDialog::browseForIcon);
    QObject::connect(m_browseIconAction, &QAction::triggered, this, &CommandButtonConfigDialog::browseForIcon);
    QObject::connect(m_clearIconAction, &QAction::triggered, this, &CommandButtonConfigDialog::clearIcon);
    QObject::connect(m_command, qOverload<int>(&QComboBox::currentIndexChanged), this,
                     &CommandButtonConfigDialog::updatePreview);
    if(auto* lineEdit = m_command->lineEdit()) {
        QObject::connect(lineEdit, &QLineEdit::textChanged, this, &CommandButtonConfigDialog::updatePreview);
    }
    QObject::connect(m_iconPath, &QLineEdit::textChanged, this, &CommandButtonConfigDialog::updatePreview);

    m_iconPreview->setIconSize({64, 64});
    m_iconPreview->setFixedSize(92, 92);
    m_iconPreview->setToolTip(tr("Click to choose an icon"));
    m_iconPreview->setFlat(false);
    m_iconPreview->setAutoDefault(false);
    m_iconPreview->setDefault(false);

    m_browseIconAction->setToolTip(tr("Browse for icon"));
    m_clearIconAction->setToolTip(tr("Clear custom icon"));
    m_iconPath->addAction(m_clearIconAction, QLineEdit::TrailingPosition);
    m_iconPath->addAction(m_browseIconAction, QLineEdit::TrailingPosition);

    auto* commandGroup  = new QGroupBox(tr("Button"), this);
    auto* commandLayout = new QGridLayout(commandGroup);

    auto* hint = new QLabel(tr("Select a command, or enter a raw `$cmdlink` id or alias."), this);
    hint->setWordWrap(true);

    int row{0};
    commandLayout->addWidget(new QLabel(tr("Command") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_command, row++, 1);
    commandLayout->addWidget(new QLabel(tr("Text") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_text, row++, 1);
    commandLayout->addWidget(new QLabel(tr("Display") + u":"_s, this), row, 0);
    commandLayout->addWidget(m_buttonStyle, row++, 1);
    commandLayout->addWidget(hint, row++, 0, 1, 2);
    commandLayout->setColumnStretch(1, 1);

    auto* iconGroup    = new QGroupBox(tr("Icon"), this);
    auto* iconLayout   = new QGridLayout(iconGroup);
    auto* previewLabel = new QLabel(tr("Preview") + u":"_s, this);

    auto* iconHint = new QLabel(
        tr("Click the image to choose a custom icon. If none is set, the button uses the command's default icon."),
        this);
    iconHint->setWordWrap(true);

    row = 0;
    iconLayout->addWidget(previewLabel, row, 0, Qt::AlignRight | Qt::AlignVCenter);
    iconLayout->addWidget(m_iconPreview, row++, 1, Qt::AlignLeft | Qt::AlignVCenter);
    iconLayout->addWidget(m_iconPath, row++, 0, 1, 3);
    iconLayout->addWidget(iconHint, row++, 0, 1, 3);
    iconLayout->setColumnStretch(1, 1);

    auto* layout{contentLayout()};

    row = 0;
    layout->addWidget(commandGroup, row++, 0);
    layout->addWidget(iconGroup, row++, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    if(auto* lineEdit = m_command->lineEdit()) {
        lineEdit->setPlaceholderText(tr("Enter a command id or alias"));
    }
    m_text->setPlaceholderText(tr("Use command label"));
    m_iconPath->setPlaceholderText(tr("No custom icon selected"));

    const auto commands = buildCommandOptions();
    for(const auto& choice : commands) {
        m_command->addItem(choice.icon, choice.label, choice.id);
        m_command->setItemData(m_command->count() - 1, choice.id, Qt::ToolTipRole);
    }
    m_command->resizeDropDown();

    loadCurrentConfig();
    updatePreview();
}

void CommandButtonConfigDialog::browseForIcon()
{
    const QString selectedIcon
        = QFileDialog::getOpenFileName(this, tr("Select Icon"), m_iconPath->text(), imageFilter());
    if(!selectedIcon.isEmpty()) {
        m_iconPath->setText(selectedIcon);
    }
}

void CommandButtonConfigDialog::clearIcon()
{
    m_iconPath->clear();
}

void CommandButtonConfigDialog::updatePreview()
{
    const CommandButton::ConfigData previewConfig{
        .commandId = currentCommandId(),
        .text      = m_text->text(),
        .iconPath  = m_iconPath->text().trimmed(),
    };

    m_iconPreview->setIcon(widget()->previewIcon(previewConfig));
    m_clearIconAction->setEnabled(!previewConfig.iconPath.isEmpty());
}

std::vector<CommandButtonConfigDialog::CommandOption> CommandButtonConfigDialog::buildCommandOptions() const
{
    std::vector<CommandOption> choices;

    const auto commands = m_actionManager->commands();
    choices.reserve(commands.size());

    for(auto* command : commands) {
        if(!command || !command->action() || command->action()->isSeparator()) {
            continue;
        }

        const QString description = command->description();
        if(description.isEmpty()) {
            continue;
        }

        auto* action           = command->action();
        const QString category = command->categories().join(u" / "_s);

        choices.emplace_back(CommandOption{
            .icon        = action->icon().isNull() ? Utils::iconFromTheme(Constants::Icons::Command) : action->icon(),
            .label       = commandLabel(category, description),
            .id          = command->id().name(),
            .category    = category,
            .description = description,
        });
    }

    std::set<int> specialCommands;
    for(const auto& alias : ScriptCommandHandler::scriptCommandAliases()) {
        if(alias.type == ScriptCommandAliasType::Action || specialCommands.contains(static_cast<int>(alias.type))) {
            continue;
        }

        specialCommands.insert(static_cast<int>(alias.type));

        const QString category    = tr(alias.category);
        const QString description = tr(alias.description);

        choices.emplace_back(CommandOption{
            .icon        = Utils::iconFromTheme(Constants::Icons::Command),
            .label       = commandLabel(category, description),
            .id          = alias.alias.toString(),
            .category    = category,
            .description = description,
        });
    }

    std::ranges::sort(choices, [](const CommandOption& lhs, const CommandOption& rhs) {
        const int categoryCompare = QString::localeAwareCompare(lhs.category, rhs.category);
        if(categoryCompare != 0) {
            return categoryCompare < 0;
        }

        return QString::localeAwareCompare(lhs.description, rhs.description) < 0;
    });

    return choices;
}

QString CommandButtonConfigDialog::currentCommandId() const
{
    QString text = m_command->currentText().trimmed();
    if(text.isEmpty()) {
        return {};
    }

    if(m_command->currentIndex() >= 0 && text == m_command->itemText(m_command->currentIndex())) {
        return m_command->currentData().toString();
    }

    for(int i{0}; i < m_command->count(); ++i) {
        if(text == m_command->itemText(i)) {
            return m_command->itemData(i).toString();
        }
    }

    return text;
}

void CommandButtonConfigDialog::setConfig(const CommandButton::ConfigData& config)
{
    if(const int index = m_command->findData(config.commandId); index >= 0) {
        m_command->setCurrentIndex(index);
    }
    else {
        m_command->setCurrentIndex(-1);
        m_command->setEditText(config.commandId);
    }

    m_iconPath->setText(config.iconPath);
    m_text->setText(config.text);
    if(const int index = m_buttonStyle->findData(config.toolButtonStyle); index >= 0) {
        m_buttonStyle->setCurrentIndex(index);
    }
    updatePreview();
}

CommandButton::ConfigData CommandButtonConfigDialog::config() const
{
    return {
        .commandId       = currentCommandId(),
        .text            = m_text->text(),
        .iconPath        = m_iconPath->text().trimmed(),
        .toolButtonStyle = m_buttonStyle->currentData().toInt(),
    };
}
} // namespace Fooyin
