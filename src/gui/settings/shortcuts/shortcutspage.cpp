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
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
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
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>

#include <initializer_list>

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
        QKeySequenceEdit* input;
        QCheckBox* global;
        QComboBox* target;
        QToolButton* removeButton;
    };

    [[nodiscard]] Command* selectedCommand() const;
    void updateCurrentBindings(const ShortcutBindingList& bindings);
    void addShortcutInput(const ShortcutBinding& binding = {}, bool focus = false);
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
    QGridLayout* m_shortcutRowsLayout;
    QLabel* m_shortcutsLabel;
    QLabel* m_targetLabel;
    QPushButton* m_resetShortcut;
    QPushButton* m_addShortcut;
    QFrame* m_systemShortcutSeparator;
    QWidget* m_systemShortcutRow;
    QCheckBox* m_globalShortcut;
    QLabel* m_systemTargetLabel;
    QComboBox* m_systemTarget;
    QPushButton* m_configureGlobalShortcuts;
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
    , m_shortcutRowsLayout{new QGridLayout(m_shortcutRows)}
    , m_shortcutsLabel{new QLabel(this)}
    , m_targetLabel{new QLabel(tr("Target"), this)}
    , m_resetShortcut{new QPushButton(tr("Reset to default"), this)}
    , m_addShortcut{new QPushButton(Gui::iconFromTheme(Constants::Icons::Add), tr("Add shortcut"), this)}
    , m_systemShortcutSeparator{new QFrame(this)}
    , m_systemShortcutRow{new QWidget(this)}
    , m_globalShortcut{new QCheckBox(tr("Enable system shortcuts"), this)}
    , m_systemTargetLabel{new QLabel(tr("Target") + u":"_s, this)}
    , m_systemTarget{new QComboBox(this)}
    , m_configureGlobalShortcuts{new QPushButton(tr("Configure…"), this)}
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
    layout->setContentsMargins({});

    layout->addWidget(m_filter, 0, 0);
    layout->addWidget(m_shortcutTable, 1, 0);
    layout->setRowStretch(1, 1);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_shortcutTable, &QTreeView::expandAll);

    auto* shortcutHeader = new QHBoxLayout();
    auto* shortcutLabel  = new QLabel(tr("Shortcuts"), this);
    shortcutHeader->addWidget(shortcutLabel);
    shortcutHeader->addStretch(1);
    shortcutHeader->addWidget(m_resetShortcut);

    m_shortcutRowsLayout->setContentsMargins({});
    m_shortcutRowsLayout->setColumnStretch(0, 1);
    m_shortcutRowsLayout->addWidget(m_shortcutsLabel, 0, 0, 1, 2);
    m_shortcutRowsLayout->addWidget(m_targetLabel, 0, 2);
    m_shortcutRowsLayout->addWidget(m_addShortcut, MaxShortcuts + 1, 0, 1, 2, Qt::AlignLeft);

    m_systemShortcutSeparator->setFrameShape(QFrame::HLine);
    m_systemShortcutSeparator->setFrameShadow(QFrame::Sunken);

    m_globalShortcut->setToolTip(tr("Key combinations are managed in your system settings."));
    m_configureGlobalShortcuts->setToolTip(tr("Open system shortcut settings."));

    m_systemTarget->addItem(tr("Focused widget"), static_cast<int>(ShortcutTarget::FocusedWidget));
    m_systemTarget->addItem(tr("Playing track"), static_cast<int>(ShortcutTarget::NowPlaying));
    m_systemTarget->setToolTip(tr("Choose which tracks the system shortcut applies to."));

    auto* systemShortcutLayout = new QHBoxLayout(m_systemShortcutRow);
    systemShortcutLayout->setContentsMargins({});
    systemShortcutLayout->addWidget(m_globalShortcut);
    systemShortcutLayout->addStretch(1);
    systemShortcutLayout->addWidget(m_systemTargetLabel);
    systemShortcutLayout->addWidget(m_systemTarget);
    systemShortcutLayout->addWidget(m_configureGlobalShortcuts);

    auto* groupLayout = new QVBoxLayout(m_shortcutBox);
    groupLayout->addLayout(shortcutHeader);
    groupLayout->addWidget(m_shortcutRows);
    groupLayout->addWidget(m_systemShortcutSeparator);
    groupLayout->addWidget(m_systemShortcutRow);

    m_conflictLabel->setWordWrap(true);
    m_conflictLabel->hide();
    groupLayout->addWidget(m_conflictLabel);

    m_reassignButton->hide();
    groupLayout->addWidget(m_reassignButton, 0, Qt::AlignLeft);

    layout->addWidget(m_shortcutBox, 2, 0);

    QObject::connect(m_resetShortcut, &QAbstractButton::clicked, this, &ShortcutsPageWidget::resetCurrentShortcut);
    QObject::connect(m_addShortcut, &QAbstractButton::clicked, this, [this]() { addShortcutInput({}, true); });
    QObject::connect(m_globalShortcut, &QCheckBox::toggled, this, [this](bool checked) {
        if(Command* command = selectedCommand()) {
            m_model->globalShortcutRegistrationChanged(command, checked);
            updateShortcutButtons();
        }
    });
    QObject::connect(m_systemTarget, &QComboBox::currentIndexChanged, this, [this] {
        if(Command* command = selectedCommand()) {
            m_model->globalShortcutTargetChanged(command,
                                                 static_cast<ShortcutTarget>(m_systemTarget->currentData().toInt()));
        }
    });
    QObject::connect(m_configureGlobalShortcuts, &QAbstractButton::clicked, m_actionManager,
                     &ActionManager::configureGlobalShortcuts);
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
    QObject::connect(m_actionManager, &ActionManager::globalShortcutManagementChanged, this,
                     &ShortcutsPageWidget::repopulateShortcuts);
    QObject::connect(m_actionManager, &ActionManager::globalShortcutConfigurationAvailabilityChanged, this,
                     &ShortcutsPageWidget::selectionChanged);
    QObject::connect(m_model, &ShortcutsModel::bindingsSynchronised, this, [this](Command* command) {
        if(command == selectedCommand()) {
            updateCurrentBindings(m_model->bindings(command));
            updateConflictState();
        }
    });

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
    shortcutChanged();
    m_model->processQueue();
    m_actionManager->saveSettings();
    selectionChanged();
}

void ShortcutsPageWidget::reset()
{
    const auto commands = m_actionManager->commands();
    for(Command* command : commands) {
        command->setShortcut(command->defaultShortcuts());

        const auto shortcuts = command->shortcuts();
        for(const auto& key : shortcuts) {
            command->setShortcutTarget(key, ShortcutTarget::FocusedWidget);
        }
        command->setGlobalShortcutTarget(ShortcutTarget::FocusedWidget);
        command->setGlobalShortcuts({});
        command->setGlobalShortcutRegistered(false);
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

void ShortcutsPageWidget::updateCurrentBindings(const ShortcutBindingList& bindings)
{
    clearShortcutInputs();

    for(const auto& binding : bindings) {
        addShortcutInput(binding);
    }

    if(bindings.empty()) {
        addShortcutInput();
    }

    updateShortcutButtons();
}

void ShortcutsPageWidget::addShortcutInput(const ShortcutBinding& binding, const bool focus)
{
    if(static_cast<int>(m_shortcutRowsData.size()) >= MaxShortcuts) {
        return;
    }

    auto* input = new QKeySequenceEdit(m_shortcutRows);
    input->setClearButtonEnabled(true);
    input->setKeySequence(binding.shortcut);

    auto* global = new QCheckBox(tr("Global"), m_shortcutRows);
    global->setChecked(binding.scope == ShortcutScope::Global);

    auto* target = new QComboBox(m_shortcutRows);
    target->addItem(tr("Focused widget"), static_cast<int>(ShortcutTarget::FocusedWidget));
    target->addItem(tr("Playing track"), static_cast<int>(ShortcutTarget::NowPlaying));
    target->setCurrentIndex(target->findData(static_cast<int>(binding.target)));
    target->setToolTip(tr("Choose which tracks this shortcut applies to."));

    auto* removeButton = new QToolButton(m_shortcutRows);
    removeButton->setIcon(Gui::iconFromTheme(Constants::Icons::Remove));
    removeButton->setToolTip(tr("Remove shortcut"));

    const int rowIndex = static_cast<int>(m_shortcutRowsData.size()) + 1;
    m_shortcutRowsLayout->addWidget(input, rowIndex, 0);
    m_shortcutRowsLayout->addWidget(global, rowIndex, 1);
    m_shortcutRowsLayout->addWidget(target, rowIndex, 2);
    m_shortcutRowsLayout->addWidget(removeButton, rowIndex, 3);
    m_shortcutRowsData.emplace_back(input, global, target, removeButton);

    QObject::connect(input, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutsPageWidget::shortcutChanged);
    QObject::connect(target, &QComboBox::currentIndexChanged, this, &ShortcutsPageWidget::shortcutChanged);
    QObject::connect(global, &QCheckBox::toggled, this, [this, global](bool checked) {
        if(checked) {
            for(const auto& row : m_shortcutRowsData) {
                if(row.global != global && row.global->isChecked()) {
                    const QSignalBlocker blocker{row.global};
                    row.global->setChecked(false);
                }
            }
        }
        shortcutChanged();
    });
    QObject::connect(removeButton, &QAbstractButton::clicked, this, [this, input]() { removeShortcutInput(input); });

    updateShortcutButtons();

    if(focus) {
        input->setFocus();
    }
}

void ShortcutsPageWidget::removeShortcutInput(QKeySequenceEdit* input)
{
    ShortcutBindingList bindings;
    for(const auto& row : m_shortcutRowsData) {
        if(row.input == input || row.input->keySequence().isEmpty()) {
            continue;
        }
        const ShortcutBinding binding{.shortcut = row.input->keySequence(),
                                      .scope
                                      = row.global->isChecked() ? ShortcutScope::Global : ShortcutScope::Application,
                                      .target = static_cast<ShortcutTarget>(row.target->currentData().toInt())};
        if(std::ranges::find(bindings, binding) == bindings.cend()) {
            bindings.push_back(binding);
        }
    }

    updateCurrentBindings(bindings);
    shortcutChanged();
}

void ShortcutsPageWidget::clearShortcutInputs()
{
    for(const auto& row : m_shortcutRowsData) {
        for(auto* widget : std::initializer_list<QWidget*>{row.input, row.global, row.target, row.removeButton}) {
            m_shortcutRowsLayout->removeWidget(widget);
            widget->hide();
            widget->deleteLater();
        }
    }

    m_shortcutRowsData.clear();
}

void ShortcutsPageWidget::updateShortcutButtons()
{
    Command* command                    = selectedCommand();
    const auto management               = m_actionManager->globalShortcutManagement();
    const bool available                = management != GlobalShortcutManagement::Unavailable;
    const bool systemManaged            = management == GlobalShortcutManagement::SystemManaged;
    const bool eligible                 = command && command->actionForContext(Constants::Context::Global);
    const bool globalShortcutRegistered = m_model->isGlobalShortcutRegistered(command);
    const bool configurationAvailable   = m_actionManager->globalShortcutConfigurationAvailable();

    const bool allRowsHaveShortcuts
        = std::ranges::all_of(m_shortcutRowsData, [](const auto& row) { return !row.input->keySequence().isEmpty(); });

    m_addShortcut->setEnabled(command && allRowsHaveShortcuts
                              && std::cmp_less(m_shortcutRowsData.size(), MaxShortcuts));
    m_resetShortcut->setEnabled(command != nullptr);

    bool showTarget      = command && command->supportsShortcutTarget(ShortcutTarget::NowPlaying);
    const bool canRemove = m_shortcutRowsData.size() > 1;
    for(const auto& row : m_shortcutRowsData) {
        row.global->setVisible(!systemManaged);
        row.global->setEnabled(available && eligible);
        row.input->setEnabled(command != nullptr);

        const bool supportsTarget = command && command->supportsShortcutTarget(ShortcutTarget::NowPlaying);
        row.target->setVisible(supportsTarget
                               || row.target->currentData().toInt() == static_cast<int>(ShortcutTarget::NowPlaying));
        showTarget |= !row.target->isHidden();
        row.target->setEnabled(command != nullptr);

        if(auto* targetModel = qobject_cast<QStandardItemModel*>(row.target->model())) {
            targetModel->item(1)->setEnabled(supportsTarget);
        }

        if(!available) {
            row.global->setToolTip(tr("Global shortcuts are not available on this platform"));
        }
        else if(!eligible) {
            row.global->setToolTip(tr("This action is only available within an application context"));
        }
        else {
            row.global->setToolTip({});
        }
        row.input->setToolTip({});

        row.removeButton->setEnabled(command && canRemove);
    }

    const bool showSystemTarget = systemManaged && command
                               && (command->supportsShortcutTarget(ShortcutTarget::NowPlaying)
                                   || m_model->globalShortcutTarget(command) == ShortcutTarget::NowPlaying);
    m_shortcutsLabel->setText(systemManaged ? tr("Application shortcuts") : tr("Shortcut"));
    m_shortcutsLabel->setVisible(systemManaged || showTarget);
    m_targetLabel->setVisible(showTarget || showSystemTarget);
    m_systemTargetLabel->setVisible(showSystemTarget);
    m_systemTargetLabel->setEnabled(eligible && globalShortcutRegistered);
    m_systemTarget->setVisible(showSystemTarget);
    m_systemTarget->setEnabled(eligible && globalShortcutRegistered);

    if(auto* targetModel = qobject_cast<QStandardItemModel*>(m_systemTarget->model())) {
        targetModel->item(1)->setEnabled(command && command->supportsShortcutTarget(ShortcutTarget::NowPlaying));
    }
    {
        const QSignalBlocker blocker{m_systemTarget};
        m_systemTarget->setCurrentIndex(
            m_systemTarget->findData(static_cast<int>(m_model->globalShortcutTarget(command))));
    }

    m_globalShortcut->setVisible(systemManaged);
    m_globalShortcut->setEnabled(systemManaged && eligible);

    {
        const QSignalBlocker blocker{m_globalShortcut};
        m_globalShortcut->setChecked(globalShortcutRegistered);
    }

    m_systemShortcutSeparator->setVisible(systemManaged);
    m_systemShortcutRow->setVisible(systemManaged);
    m_configureGlobalShortcuts->setVisible(configurationAvailable);
    m_configureGlobalShortcuts->setEnabled(systemManaged && configurationAvailable);
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
        m_shortcutBox->setEnabled(m_actionManager->globalShortcutManagement() == GlobalShortcutManagement::SystemManaged
                                  && m_actionManager->globalShortcutConfigurationAvailable());
        updateShortcutButtons();
        updateConflictState();
        return;
    }

    const auto bindings = m_model->bindings(command);

    updateCurrentBindings(bindings);
    m_shortcutBox->setDisabled(false);
    updateConflictState();
}

void ShortcutsPageWidget::shortcutChanged()
{
    Command* command = selectedCommand();
    if(!command) {
        return;
    }

    ShortcutBindingList bindings;
    ShortcutList usedShortcuts;

    for(const auto& row : m_shortcutRowsData) {
        const QKeySequence shortcut = row.input->keySequence();
        if(!shortcut.isEmpty() && !usedShortcuts.contains(shortcut)) {
            usedShortcuts.append(shortcut);
            bindings.push_back({.shortcut = shortcut,
                                .scope  = row.global->isChecked() ? ShortcutScope::Global : ShortcutScope::Application,
                                .target = static_cast<ShortcutTarget>(row.target->currentData().toInt())});
        }
    }

    m_model->bindingsChanged(command, bindings);
    updateShortcutButtons();
    updateConflictState();
}

void ShortcutsPageWidget::resetCurrentShortcut()
{
    Command* command = selectedCommand();
    if(!command) {
        return;
    }

    ShortcutBindingList bindings;
    for(const auto& shortcut : command->defaultShortcuts()) {
        bindings.emplace_back(shortcut, ShortcutScope::Application);
    }
    m_model->bindingsChanged(command, bindings);
    if(m_actionManager->globalShortcutManagement() == GlobalShortcutManagement::SystemManaged) {
        m_model->globalShortcutRegistrationChanged(command, false);
        m_model->globalShortcutTargetChanged(command, ShortcutTarget::FocusedWidget);
    }
    updateCurrentBindings(bindings);
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
