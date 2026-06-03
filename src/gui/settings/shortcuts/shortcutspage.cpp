/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include <gui/iconloader.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QTreeView>

using namespace Qt::StringLiterals;

constexpr auto MaxShortcuts = 3;

namespace Fooyin {
class ShortcutsFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if(filterRegularExpression().pattern().isEmpty()) {
            return true;
        }

        const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        if(matchesRow(sourceIndex) || ancestorMatches(sourceParent)) {
            return true;
        }

        const int rowCount = sourceModel()->rowCount(sourceIndex);
        for(int row{0}; row < rowCount; ++row) {
            if(filterAcceptsRow(row, sourceIndex)) {
                return true;
            }
        }

        return false;
    }

private:
    [[nodiscard]] bool matchesRow(const QModelIndex& sourceIndex) const
    {
        if(!sourceIndex.isValid()) {
            return false;
        }

        const auto expression = filterRegularExpression();

        const int columnCount = sourceModel()->columnCount(sourceIndex.parent());
        for(int column{0}; column < columnCount; ++column) {
            const QModelIndex columnIndex = sourceIndex.siblingAtColumn(column);

            if(sourceModel()->data(columnIndex, Qt::DisplayRole).toString().contains(expression)) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool ancestorMatches(QModelIndex sourceParent) const
    {
        while(sourceParent.isValid()) {
            if(matchesRow(sourceParent)) {
                return true;
            }
            sourceParent = sourceParent.parent();
        }

        return false;
    }
};

class ShortcutsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ShortcutsPageWidget(ActionManager* actionManager);

    void load() override;
    void apply() override;
    void reset() override;

    [[nodiscard]] QString validationError() const override;

private:
    struct ShortcutRow
    {
        QWidget* widget;
        QKeySequenceEdit* input;
        QToolButton* removeButton;
    };

    [[nodiscard]] Command* selectedCommand() const;
    void updateCurrentShortcuts(const ShortcutList& shortcuts);
    void addShortcutInput(const QKeySequence& shortcut = {}, bool focus = false);
    void removeShortcutInput(QKeySequenceEdit* input);
    void clearShortcutInputs();
    void updateShortcutButtons();
    void updateConflictState();
    void selectionChanged();
    void shortcutChanged();
    void resetCurrentShortcut();
    void reassignConflicts();
    void repopulateShortcuts();

    ActionManager* m_actionManager;

    QLineEdit* m_filter;
    QTreeView* m_shortcutTable;
    ShortcutsModel* m_model;
    ShortcutsFilterModel* m_proxyModel;
    QGroupBox* m_shortcutBox;
    QWidget* m_shortcutRows;
    QVBoxLayout* m_shortcutRowsLayout;
    QPushButton* m_resetShortcut;
    QPushButton* m_addShortcut;
    QLabel* m_conflictLabel;
    QPushButton* m_reassignButton;
    std::vector<ShortcutRow> m_shortcutRowsData;
};

ShortcutsPageWidget::ShortcutsPageWidget(ActionManager* actionManager)
    : m_actionManager{actionManager}
    , m_filter{new QLineEdit(this)}
    , m_shortcutTable{new QTreeView(this)}
    , m_model{new ShortcutsModel(this)}
    , m_proxyModel{new ShortcutsFilterModel(this)}
    , m_shortcutBox{new QGroupBox(this)}
    , m_shortcutRows{new QWidget(this)}
    , m_shortcutRowsLayout{new QVBoxLayout(m_shortcutRows)}
    , m_resetShortcut{new QPushButton(tr("Reset to default"), this)}
    , m_addShortcut{new QPushButton(Gui::iconFromTheme(Constants::Icons::Add), tr("Add shortcut"), this)}
    , m_conflictLabel{new QLabel(this)}
    , m_reassignButton{new QPushButton(tr("Overwrite Shortcut"), this)}
{
    m_filter->setPlaceholderText(tr("Filter shortcuts"));
    m_filter->setClearButtonEnabled(true);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_shortcutTable->setModel(m_proxyModel);
    m_shortcutTable->hideColumn(1);
    m_shortcutTable->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_filter, 0, 0);
    layout->addWidget(m_shortcutTable, 1, 0);
    layout->setRowStretch(1, 1);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_shortcutTable, &QTreeView::expandAll);

    auto* groupLayout = new QVBoxLayout(m_shortcutBox);

    auto* shortcutHeader = new QHBoxLayout();
    auto* shortcutLabel  = new QLabel(tr("Shortcuts"), this);
    shortcutHeader->addWidget(shortcutLabel);
    shortcutHeader->addStretch(1);

    m_shortcutRowsLayout->setContentsMargins(0, 0, 0, 0);

    auto* shortcutActions = new QHBoxLayout();
    shortcutActions->addWidget(m_addShortcut);
    shortcutActions->addStretch(1);
    shortcutActions->addWidget(m_resetShortcut);

    groupLayout->addLayout(shortcutHeader);
    groupLayout->addWidget(m_shortcutRows);
    groupLayout->addLayout(shortcutActions);

    m_conflictLabel->setWordWrap(true);
    m_conflictLabel->hide();
    groupLayout->addWidget(m_conflictLabel);

    m_reassignButton->hide();
    groupLayout->addWidget(m_reassignButton, 0, Qt::AlignLeft);

    layout->addWidget(m_shortcutBox, 2, 0);

    QObject::connect(m_resetShortcut, &QAbstractButton::clicked, this, &ShortcutsPageWidget::resetCurrentShortcut);
    QObject::connect(m_addShortcut, &QAbstractButton::clicked, this, [this]() { addShortcutInput({}, true); });
    QObject::connect(m_reassignButton, &QAbstractButton::clicked, this, &ShortcutsPageWidget::reassignConflicts);
    QObject::connect(m_shortcutTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ShortcutsPageWidget::selectionChanged);
    QObject::connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxyModel->setFilterRegularExpression(
            QRegularExpression{QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption});
        m_shortcutTable->expandAll();
        selectionChanged();
    });
    QObject::connect(m_actionManager, &ActionManager::commandsChanged, this, &ShortcutsPageWidget::repopulateShortcuts);

    m_shortcutBox->setDisabled(true);
}

void ShortcutsPageWidget::load()
{
    repopulateShortcuts();
}

void ShortcutsPageWidget::repopulateShortcuts()
{
    m_model->populate(m_actionManager);
    updateConflictState();
    selectionChanged();
}

void ShortcutsPageWidget::apply()
{
    m_model->processQueue();
    m_actionManager->saveSettings();
}

void ShortcutsPageWidget::reset()
{
    const auto commands = m_actionManager->commands();
    for(Command* command : commands) {
        command->setShortcut(command->defaultShortcuts());
    }
}

QString ShortcutsPageWidget::validationError() const
{
    return m_model->firstConflictError();
}

Command* ShortcutsPageWidget::selectedCommand() const
{
    const auto selected = m_shortcutTable->selectionModel()->selectedIndexes();
    if(selected.empty()) {
        return nullptr;
    }

    const QModelIndex index = m_proxyModel->mapToSource(selected.front());
    if(index.data(ShortcutItem::IsCategory).toBool()) {
        return nullptr;
    }

    return index.data(ShortcutItem::ActionCommand).value<Command*>();
}

void ShortcutsPageWidget::updateCurrentShortcuts(const ShortcutList& shortcuts)
{
    clearShortcutInputs();

    for(const auto& shortcut : shortcuts) {
        addShortcutInput(shortcut);
    }

    if(shortcuts.empty()) {
        addShortcutInput();
    }

    updateShortcutButtons();
}

void ShortcutsPageWidget::addShortcutInput(const QKeySequence& shortcut, const bool focus)
{
    if(static_cast<int>(m_shortcutRowsData.size()) >= MaxShortcuts) {
        return;
    }

    auto* rowWidget = new QWidget(m_shortcutRows);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* input = new QKeySequenceEdit(rowWidget);
    input->setClearButtonEnabled(true);
    input->setKeySequence(shortcut);

    auto* removeButton = new QToolButton(rowWidget);
    removeButton->setIcon(Gui::iconFromTheme(Constants::Icons::Remove));
    removeButton->setToolTip(tr("Remove shortcut"));

    rowLayout->addWidget(input);
    rowLayout->addWidget(removeButton);

    m_shortcutRowsLayout->addWidget(rowWidget);
    m_shortcutRowsData.emplace_back(rowWidget, input, removeButton);

    QObject::connect(input, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutsPageWidget::shortcutChanged);
    QObject::connect(removeButton, &QAbstractButton::clicked, this, [this, input]() { removeShortcutInput(input); });

    updateShortcutButtons();

    if(focus) {
        input->setFocus();
    }
}

void ShortcutsPageWidget::removeShortcutInput(QKeySequenceEdit* input)
{
    ShortcutList shortcuts;
    for(const auto& row : m_shortcutRowsData) {
        if(row.input == input || row.input->keySequence().isEmpty()) {
            continue;
        }
        if(!shortcuts.contains(row.input->keySequence())) {
            shortcuts.append(row.input->keySequence());
        }
    }

    updateCurrentShortcuts(shortcuts);
    shortcutChanged();
}

void ShortcutsPageWidget::clearShortcutInputs()
{
    for(const auto& row : m_shortcutRowsData) {
        m_shortcutRowsLayout->removeWidget(row.widget);
        row.widget->hide();
        row.widget->deleteLater();
    }

    m_shortcutRowsData.clear();
}

void ShortcutsPageWidget::updateShortcutButtons()
{
    const bool allRowsHaveShortcuts
        = std::ranges::all_of(m_shortcutRowsData, [](const auto& row) { return !row.input->keySequence().isEmpty(); });

    m_addShortcut->setEnabled(allRowsHaveShortcuts && std::cmp_less(m_shortcutRowsData.size(), MaxShortcuts));

    const bool canRemove = m_shortcutRowsData.size() > 1;
    for(const auto& row : m_shortcutRowsData) {
        row.removeButton->setEnabled(canRemove);
    }
}

void ShortcutsPageWidget::updateConflictState()
{
    Command* command = selectedCommand();
    if(!command) {
        m_conflictLabel->hide();
        m_reassignButton->hide();
        return;
    }

    const QString conflicts = m_model->conflictDescription(command);
    if(conflicts.isEmpty()) {
        m_conflictLabel->hide();
        m_reassignButton->hide();
        return;
    }

    m_conflictLabel->setText(tr("Duplicate shortcuts") + u":\n%1"_s.arg(conflicts));
    m_conflictLabel->show();
    m_reassignButton->show();
}

void ShortcutsPageWidget::selectionChanged()
{
    Command* command = selectedCommand();
    if(!command) {
        m_shortcutBox->setDisabled(true);
        updateConflictState();
        return;
    }

    const auto shortcuts = m_model->shortcuts(command);

    updateCurrentShortcuts(shortcuts);
    m_shortcutBox->setDisabled(false);
    updateConflictState();
}

void ShortcutsPageWidget::shortcutChanged()
{
    Command* command = selectedCommand();
    if(!command) {
        return;
    }

    ShortcutList shortcuts;

    for(const auto& row : m_shortcutRowsData) {
        const QKeySequence shortcut = row.input->keySequence();
        if(!shortcut.isEmpty() && !shortcuts.contains(shortcut)) {
            shortcuts.append(shortcut);
        }
    }

    m_model->shortcutChanged(command, shortcuts);
    updateShortcutButtons();
    updateConflictState();
}

void ShortcutsPageWidget::resetCurrentShortcut()
{
    Command* command = selectedCommand();
    if(!command) {
        return;
    }

    m_model->shortcutChanged(command, command->defaultShortcuts());
    updateCurrentShortcuts(command->defaultShortcuts());
    updateConflictState();
}

void ShortcutsPageWidget::reassignConflicts()
{
    Command* command = selectedCommand();
    if(!command) {
        return;
    }

    m_model->reassignConflicts(command);
    updateConflictState();
}

ShortcutsPage::ShortcutsPage(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Shortcuts);
    setName(tr("Shortcuts"));
    setCategory({tr("Shortcuts")});
    setRelativePosition(SettingsPageRelativePosition::After, ::Fooyin::Constants::Page::PlaylistGeneral);
    setWidgetCreator([actionManager] { return new ShortcutsPageWidget(actionManager); });
}
} // namespace Fooyin

#include "moc_shortcutspage.cpp"
#include "shortcutspage.moc"
