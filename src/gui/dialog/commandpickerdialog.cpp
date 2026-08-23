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
 */

#include "commandpickerdialog.h"

#include "scripting/scriptcommandhandler.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/id.h>

#include <QAction>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <map>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct CommandChoice
{
    QIcon icon;
    QString category;
    QString description;
    QString id;
    QStringList aliases;
};

QString commandLabel(const QString& category, const QString& description)
{
    return category.isEmpty() ? description : u"%1 | %2"_s.arg(category, description);
}

std::vector<CommandChoice> commandChoices(ActionManager* actionManager)
{
    std::map<QString, QStringList> aliasesById;
    std::map<ScriptCommandAliasType, QStringList> specialAliases;

    const auto& aliases = ScriptCommandHandler::scriptCommandAliases();

    for(const auto& alias : aliases) {
        if(alias.type == ScriptCommandAliasType::Action) {
            aliasesById[QString::fromLatin1(alias.actionId)].append(alias.alias.toString());
        }
        else {
            specialAliases[alias.type].append(alias.alias.toString());
        }
    }

    const auto commands = actionManager->commands();

    std::vector<CommandChoice> choices;
    choices.reserve(commands.size() + specialAliases.size());

    for(auto* command : commands) {
        if(!command || !command->action() || command->action()->isSeparator() || command->description().isEmpty()) {
            continue;
        }

        const QString id = command->id().name();
        choices.push_back(CommandChoice{
            .icon        = command->action()->icon().isNull() ? Gui::iconFromTheme(Constants::Icons::Command)
                                                              : command->action()->icon(),
            .category    = command->categories().join(u" / "_s),
            .description = command->description(),
            .id          = id,
            .aliases     = aliasesById[id],
        });
    }

    std::set<ScriptCommandAliasType> addedSpecialCommands;

    for(const auto& alias : aliases) {
        if(alias.type == ScriptCommandAliasType::Action || addedSpecialCommands.contains(alias.type)) {
            continue;
        }

        addedSpecialCommands.emplace(alias.type);

        choices.push_back(CommandChoice{
            .icon        = Gui::iconFromTheme(Constants::Icons::Command),
            .category    = CommandPickerDialog::tr(alias.category),
            .description = CommandPickerDialog::tr(alias.description),
            .id          = alias.alias.toString(),
            .aliases     = specialAliases[alias.type],
        });
    }

    std::ranges::sort(choices, [](const CommandChoice& lhs, const CommandChoice& rhs) {
        const int categoryCompare = QString::localeAwareCompare(lhs.category, rhs.category);
        return categoryCompare == 0 ? QString::localeAwareCompare(lhs.description, rhs.description) < 0
                                    : categoryCompare < 0;
    });

    return choices;
}
} // namespace

CommandPickerDialog::CommandPickerDialog(ActionManager* actionManager, QWidget* parent)
    : QDialog{parent}
    , m_actionManager{actionManager}
    , m_filter{new QLineEdit(this)}
    , m_commands{new QTreeView(this)}
    , m_commandId{new QLineEdit(this)}
    , m_model{new QStandardItemModel(this)}
    , m_proxy{new QSortFilterProxyModel(this)}
{
    setWindowTitle(tr("Choose Command"));
    setMinimumSize(650, 450);

    m_filter->setPlaceholderText(tr("Filter commands"));
    m_filter->setClearButtonEnabled(true);

    m_commandId->setPlaceholderText(tr("Enter a raw `$cmdlink` id or alias"));
    m_commandId->setClearButtonEnabled(true);

    m_model->setHorizontalHeaderLabels({tr("Command"), tr("ID")});
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterRole(SearchRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setRecursiveFilteringEnabled(true);
    m_proxy->setAutoAcceptChildRows(true);

    m_commands->setModel(m_proxy);
    m_commands->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_commands->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commands->setUniformRowHeights(true);
    m_commands->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_commands->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Select"));

    auto* idLayout = new QFormLayout();
    idLayout->addRow(tr("Command ID or alias") + u":"_s, m_commandId);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_filter);
    layout->addWidget(m_commands, 1);
    layout->addLayout(idLayout);
    layout->addWidget(buttons);

    populate();
    m_commands->expandAll();

    QObject::connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setFilterFixedString(text.trimmed());
        m_commands->expandAll();
    });
    QObject::connect(m_commands->selectionModel(), &QItemSelectionModel::currentChanged, this,
                     &CommandPickerDialog::currentChanged);
    QObject::connect(m_commands, &QAbstractItemView::doubleClicked, this, &CommandPickerDialog::activateCurrent);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_actionManager, &ActionManager::commandsChanged, this, [this]() {
        const QString currentId = commandId();
        populate();
        setCommandId(currentId);
    });
}

QString CommandPickerDialog::commandId() const
{
    return m_commandId->text().trimmed();
}

void CommandPickerDialog::setCommandId(const QString& commandId)
{
    m_commandId->setText(commandId.trimmed());
    restoreSelection();
}

QString CommandPickerDialog::displayText(ActionManager* actionManager, const QString& commandId)
{
    QString trimmed = commandId.trimmed();
    if(trimmed.isEmpty()) {
        return {};
    }

    const auto resolved = ScriptCommandHandler::resolveCommand(trimmed);
    if(resolved && !resolved->description.isEmpty()) {
        return commandLabel(resolved->category, resolved->description);
    }

    const QString resolvedId = resolved ? resolved->id : trimmed;
    if(auto* command = actionManager->command(Id{resolvedId}); command && !command->description().isEmpty()) {
        return commandLabel(command->categories().join(u" / "_s), command->description());
    }

    return trimmed;
}

QIcon CommandPickerDialog::displayIcon(ActionManager* actionManager, const QString& commandId)
{
    const QString trimmed = commandId.trimmed();
    if(trimmed.isEmpty()) {
        return {};
    }

    const auto resolved = ScriptCommandHandler::resolveCommand(trimmed);
    if(resolved && resolved->type != ScriptCommandAliasType::Action) {
        return Gui::iconFromTheme(Constants::Icons::Command);
    }

    const QString resolvedId = resolved ? resolved->id : trimmed;
    if(auto* command = actionManager->command(Id{resolvedId}); command && command->action()) {
        const QIcon icon = command->action()->icon();
        return icon.isNull() ? Gui::iconFromTheme(Constants::Icons::Command) : icon;
    }

    return {};
}

void CommandPickerDialog::populate()
{
    m_model->removeRows(0, m_model->rowCount());

    std::map<QString, QStandardItem*> categories;

    const auto choices = commandChoices(m_actionManager);
    for(const auto& choice : choices) {
        QStandardItem* categoryItem{nullptr};

        if(!categories.contains(choice.category)) {
            const QString category = choice.category.isEmpty() ? tr("Other") : choice.category;
            categoryItem           = new QStandardItem(category);
            categoryItem->setEditable(false);
            categoryItem->setData(category, SearchRole);

            m_model->appendRow({categoryItem, new QStandardItem()});
            categories.emplace(choice.category, categoryItem);
        }
        else {
            categoryItem = categories.at(choice.category);
        }

        auto* descriptionItem = new QStandardItem(choice.icon, choice.description);
        descriptionItem->setEditable(false);
        descriptionItem->setData(choice.id, CommandIdRole);
        descriptionItem->setData(
            QStringList{choice.category, choice.description, choice.id, choice.aliases.join(u' ')}.join(u' '),
            SearchRole);
        descriptionItem->setToolTip(commandLabel(choice.category, choice.description));

        auto* idItem = new QStandardItem(choice.id);
        idItem->setEditable(false);
        idItem->setData(choice.id, CommandIdRole);
        idItem->setData(descriptionItem->data(SearchRole), SearchRole);
        categoryItem->appendRow({descriptionItem, idItem});
    }
}

void CommandPickerDialog::restoreSelection()
{
    const auto matches
        = m_model->match(m_model->index(0, 0), CommandIdRole, commandId(), 1, Qt::MatchExactly | Qt::MatchRecursive);
    if(matches.isEmpty()) {
        m_commands->clearSelection();
        m_commands->setCurrentIndex({});
        return;
    }

    const QModelIndex index = m_proxy->mapFromSource(matches.front());
    if(index.isValid()) {
        m_commands->setCurrentIndex(index);
        m_commands->scrollTo(index);
    }
}

void CommandPickerDialog::currentChanged(const QModelIndex& current)
{
    const QString id = current.data(CommandIdRole).toString();
    if(!id.isEmpty()) {
        m_commandId->setText(id);
    }
}

void CommandPickerDialog::activateCurrent(const QModelIndex& index)
{
    if(!index.data(CommandIdRole).toString().isEmpty()) {
        accept();
    }
}
} // namespace Fooyin

#include "moc_commandpickerdialog.cpp"