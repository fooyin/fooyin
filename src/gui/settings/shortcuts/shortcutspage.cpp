/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Gui::Settings {
class ShortcutInput : public Utils::ExpandableInput
{
    Q_OBJECT

public:
    explicit ShortcutInput(QWidget* parent = nullptr)
        : ExpandableInput{Utils::ExpandableInput::ClearButton | Utils::ExpandableInput::CustomWidget, parent}
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

class ShortcutsPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit ShortcutsPageWidget(Utils::ActionManager* actionManager);

    void apply() override;
    void reset() override;

private:
    void selectionChanged();
    void shortcutChanged();
    void shortcutDeleted(const QString& text);

    Utils::ActionManager* m_actionManager;

    QTreeView* m_shortcutTable;
    ShortcutsModel* m_model;
    QGroupBox* m_shortcutBox;
    Utils::ExpandableInputBox* m_inputBox;
};

ShortcutsPageWidget::ShortcutsPageWidget(Utils::ActionManager* actionManager)
    : m_actionManager{actionManager}
    , m_shortcutTable{new QTreeView(this)}
    , m_model{new ShortcutsModel(this)}
    , m_shortcutBox{new QGroupBox(this)}
    , m_inputBox{new Utils::ExpandableInputBox(
          tr("Shortcuts"), Utils::ExpandableInput::ClearButton | Utils::ExpandableInput::CustomWidget, this)}
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
    m_inputBox->setInputWidget([this](QWidget* parent) {
        auto* input = new ShortcutInput(parent);
        QObject::connect(input, &Utils::ExpandableInput::textChanged, this, &ShortcutsPageWidget::shortcutChanged);
        return input;
    });

    groupLayout->addWidget(m_inputBox);

    layout->addWidget(m_shortcutBox, 1, 0);

    QObject::connect(m_shortcutTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ShortcutsPageWidget::selectionChanged);
    QObject::connect(m_inputBox, &Utils::ExpandableInputBox::blockDeleted, this, &ShortcutsPageWidget::shortcutDeleted);

    m_shortcutBox->setDisabled(true);
}

void ShortcutsPageWidget::apply()
{
    m_model->processQueue();
}

void ShortcutsPageWidget::reset() { }

void ShortcutsPageWidget::selectionChanged()
{
    m_inputBox->clearBlocks();

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

    auto* command        = index.data(ShortcutItem::Command).value<Utils::Command*>();
    const auto shortcuts = command->shortcuts();

    for(const auto& shortcut : shortcuts) {
        auto* input = new ShortcutInput(this);
        input->setShortcut(shortcut);
        QObject::connect(input, &Utils::ExpandableInput::textChanged, this, &ShortcutsPageWidget::shortcutChanged);
        m_inputBox->addInput(input);
    }

    if(shortcuts.empty()) {
        m_inputBox->addEmptyBlock();
    }

    m_shortcutBox->setDisabled(false);
}

void ShortcutsPageWidget::shortcutChanged()
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        return;
    }

    Utils::ShortcutList shortcuts;

    const auto inputs = m_inputBox->blocks();
    for(Utils::ExpandableInput* input : inputs) {
        const QString text = input->text();
        if(!text.isEmpty() && !shortcuts.contains(text)) {
            shortcuts.append(text);
        }
    }

    const QModelIndex index = selected.front();
    auto* command           = index.data(ShortcutItem::Command).value<Utils::Command*>();
    m_model->shortcutChanged(command, shortcuts);
}

void ShortcutsPageWidget::shortcutDeleted(const QString& text)
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();

    if(selected.empty()) {
        return;
    }

    const QModelIndex index = selected.front();
    auto* command           = index.data(ShortcutItem::Command).value<Utils::Command*>();
    m_model->shortcutDeleted(command, text);
}

ShortcutsPage::ShortcutsPage(Utils::ActionManager* actionManager, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Shortcuts);
    setName(tr("Shortcuts"));
    setCategory({tr("Shortcuts")});
    setWidgetCreator([actionManager] { return new ShortcutsPageWidget(actionManager); });
}
} // namespace Fy::Gui::Settings

#include "shortcutspage.moc"
