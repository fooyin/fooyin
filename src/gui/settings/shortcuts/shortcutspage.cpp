/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "shortcutspage.h"

#include "shortcutsmodel.h"

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/expandableinputbox.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QTreeView>

namespace Fooyin {
class ShortcutInput : public ExpandableInput
{
    Q_OBJECT

public:
    explicit ShortcutInput(QWidget* parent = nullptr)
        : ExpandableInput{ExpandableInput::ClearButton | ExpandableInput::CustomWidget, parent}
        , m_shortcut{new QKeySequenceEdit(this)}
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_shortcut);

        m_shortcut->setClearButtonEnabled(true);

        QObject::connect(m_shortcut, &QKeySequenceEdit::keySequenceChanged, this,
                         [this](const QKeySequence& shortcut) { emit textChanged(shortcut.toString()); });
    }

    [[nodiscard]] QString text() const override
    {
        return m_shortcut->keySequence().toString();
    }

    void setShortcut(const QKeySequence& shortcut)
    {
        m_shortcut->setKeySequence(shortcut);
    }

private:
    QKeySequenceEdit* m_shortcut;
};

class ShortcutsPageWidget : public SettingsPageWidget
{
public:
    explicit ShortcutsPageWidget(ActionManager* actionManager);

    void apply() override;
    void reset() override;

private:
    void updateCurrentShortcuts(const ShortcutList& shortcuts);
    void selectionChanged();
    void shortcutChanged();
    void shortcutDeleted(const QString& text);
    void resetCurrentShortcut();

    ActionManager* m_actionManager;

    QTreeView* m_shortcutTable;
    ShortcutsModel* m_model;
    QGroupBox* m_shortcutBox;
    ExpandableInputBox* m_inputBox;
};

ShortcutsPageWidget::ShortcutsPageWidget(ActionManager* actionManager)
    : m_actionManager{actionManager}
    , m_shortcutTable{new QTreeView(this)}
    , m_model{new ShortcutsModel(this)}
    , m_shortcutBox{new QGroupBox(this)}
    , m_inputBox{
          new ExpandableInputBox(tr("Shortcuts"), ExpandableInput::ClearButton | ExpandableInput::CustomWidget, this)}
{
    m_shortcutTable->setModel(m_model);
    m_shortcutTable->hideColumn(1);
    m_shortcutTable->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_shortcutTable, 0, 0);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_shortcutTable, &QTreeView::expandAll);

    m_model->populate(m_actionManager);

    auto* groupLayout = new QVBoxLayout(m_shortcutBox);

    m_inputBox->setMaximum(3);
    auto* resetShortcut = new QPushButton(tr("Reset"), this);
    m_inputBox->addBoxWidget(resetShortcut);
    m_inputBox->setInputWidget([this](QWidget* parent) {
        auto* input = new ShortcutInput(parent);
        QObject::connect(input, &ExpandableInput::textChanged, this, &ShortcutsPageWidget::shortcutChanged);
        return input;
    });

    groupLayout->addWidget(m_inputBox);

    layout->addWidget(m_shortcutBox, 1, 0);

    QObject::connect(resetShortcut, &QAbstractButton::clicked, this, &ShortcutsPageWidget::resetCurrentShortcut);
    QObject::connect(m_shortcutTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ShortcutsPageWidget::selectionChanged);
    QObject::connect(m_inputBox, &ExpandableInputBox::blockDeleted, this, &ShortcutsPageWidget::shortcutDeleted);

    m_shortcutBox->setDisabled(true);
}

void ShortcutsPageWidget::apply()
{
    m_model->processQueue();
}

void ShortcutsPageWidget::reset()
{
    const auto commands = m_actionManager->commands();
    for(Command* command : commands) {
        m_model->shortcutChanged(command, command->defaultShortcuts());
    }

    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();
    if(!selected.empty()) {
        const QModelIndex index = selected.front();
        auto* command           = index.data(ShortcutItem::ActionCommand).value<Command*>();
        updateCurrentShortcuts(command->defaultShortcuts());
    }

    apply();
}

void ShortcutsPageWidget::updateCurrentShortcuts(const ShortcutList& shortcuts)
{
    m_inputBox->clearBlocks();

    for(const auto& shortcut : shortcuts) {
        auto* input = new ShortcutInput(this);
        input->setShortcut(shortcut);
        QObject::connect(input, &ExpandableInput::textChanged, this, &ShortcutsPageWidget::shortcutChanged);
        m_inputBox->addInput(input);
    }

    if(shortcuts.empty()) {
        m_inputBox->addEmptyBlock();
    }
}

void ShortcutsPageWidget::selectionChanged()
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        m_shortcutBox->setDisabled(true);
        return;
    }

    const QModelIndex index = selected.front();

    if(index.data(ShortcutItem::IsCategory).toBool()) {
        m_shortcutBox->setDisabled(true);
        return;
    }

    auto* command        = index.data(ShortcutItem::ActionCommand).value<Command*>();
    const auto shortcuts = command->shortcuts();

    updateCurrentShortcuts(shortcuts);

    m_shortcutBox->setDisabled(false);
}

void ShortcutsPageWidget::shortcutChanged()
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        return;
    }

    ShortcutList shortcuts;

    const auto inputs = m_inputBox->blocks();
    for(ExpandableInput* input : inputs) {
        const QString text = input->text();
        if(!text.isEmpty() && !shortcuts.contains(text)) {
            shortcuts.append(text);
        }
    }

    const QModelIndex index = selected.front();
    auto* command           = index.data(ShortcutItem::ActionCommand).value<Command*>();
    m_model->shortcutChanged(command, shortcuts);
}

void ShortcutsPageWidget::shortcutDeleted(const QString& text)
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        return;
    }

    const QModelIndex index = selected.front();
    auto* command           = index.data(ShortcutItem::ActionCommand).value<Command*>();
    m_model->shortcutDeleted(command, text);
}

void ShortcutsPageWidget::resetCurrentShortcut()
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        return;
    }

    const QModelIndex index = selected.front();
    auto* command           = index.data(ShortcutItem::ActionCommand).value<Command*>();

    m_model->shortcutChanged(command, command->defaultShortcuts());
    updateCurrentShortcuts(command->defaultShortcuts());
}

ShortcutsPage::ShortcutsPage(ActionManager* actionManager, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Shortcuts);
    setName(tr("Shortcuts"));
    setCategory({tr("Shortcuts")});
    setWidgetCreator([actionManager] { return new ShortcutsPageWidget(actionManager); });
}
} // namespace Fooyin

#include "shortcutspage.moc"
