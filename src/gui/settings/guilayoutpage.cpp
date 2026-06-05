/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "guilayoutpage.h"

#include "layouttreemodel.h"

#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/widgetprovider.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QTreeView>

#include <algorithm>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
std::vector<int> indexPath(const QModelIndex& index)
{
    std::vector<int> path;
    for(QModelIndex current = index; current.isValid(); current = current.parent()) {
        path.emplace_back(current.row());
    }
    std::ranges::reverse(path);
    return path;
}

QModelIndex indexFromPath(const QAbstractItemModel* model, const std::vector<int>& path)
{
    QModelIndex parent;
    for(const int row : path) {
        parent = model->index(row, 0, parent);
        if(!parent.isValid()) {
            return {};
        }
    }
    return parent;
}

class TreeSelectionGuard
{
public:
    TreeSelectionGuard(QTreeView* tree, const QAbstractItemModel* model)
        : m_tree{tree}
        , m_model{model}
        , m_path{indexPath(tree->currentIndex())}
    { }

    ~TreeSelectionGuard()
    {
        const QModelIndex restoredIndex = indexFromPath(m_model, m_path);
        if(restoredIndex.isValid()) {
            m_tree->setCurrentIndex(restoredIndex);
        }
    }

    void moveLastRow(int offset)
    {
        if(!m_path.empty()) {
            m_path.back() += offset;
        }
    }

private:
    QTreeView* m_tree;
    const QAbstractItemModel* m_model;
    std::vector<int> m_path;
};

void setEqualButtonWidth(std::initializer_list<QPushButton*> buttons)
{
    int width{0};
    for(const auto* button : buttons) {
        width = std::max(width, button->sizeHint().width());
    }
    for(auto* button : buttons) {
        button->setFixedWidth(width);
    }
}
} // namespace

class GuiLayoutPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiLayoutPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                 WidgetProvider* widgetProvider, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void onContextMenuRequested(const QPoint& pos);
    void onChangeLayout();
    void onSelectionChanged();
    void onCustomMarginsChanged(bool enabled);
    void onCustomSplitterSpacingChanged(bool enabled);
    void updateMarginControls();
    void updateSplitterControls();
    void updateModelMargins();
    void updateModelSplitterSpacing();
    void updateButtonStates();
    void moveSelectionUp();
    void moveSelectionDown();
    void cutSelection();
    void copySelection();
    void pasteAfterSelection();
    void onNewLayout();
    void onDeleteLayout();
    void onRenameLayout();
    void onDuplicateLayout();

    LayoutProvider* m_layoutProvider;
    EditableLayout* m_editableLayout;
    SettingsManager* m_settings;

    QTreeView* m_layoutTree;
    LayoutTreeModel* m_model;
    QGroupBox* m_marginsGroup;
    QCheckBox* m_customMargins;
    QSpinBox* m_leftMargin;
    QSpinBox* m_topMargin;
    QSpinBox* m_rightMargin;
    QSpinBox* m_bottomMargin;
    QGroupBox* m_splitterGroup;
    QCheckBox* m_customSplitterSpacing;
    QSpinBox* m_splitterSpacing;

    QComboBox* m_layoutCombo;
    QPushButton* m_deleteLayout;
    QJsonObject m_clipboardItem;
    bool m_loading{false};
    bool m_updatingMargins{false};
    bool m_updatingSplitterSpacing{false};
};

GuiLayoutPageWidget::GuiLayoutPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                         WidgetProvider* widgetProvider, SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_layoutTree{new QTreeView(this)}
    , m_model{new LayoutTreeModel(widgetProvider, this)}
    , m_marginsGroup{new QGroupBox(tr("Margins"), this)}
    , m_customMargins{new QCheckBox(tr("Use custom margins"), this)}
    , m_leftMargin{new QSpinBox(this)}
    , m_topMargin{new QSpinBox(this)}
    , m_rightMargin{new QSpinBox(this)}
    , m_bottomMargin{new QSpinBox(this)}
    , m_splitterGroup{new QGroupBox(tr("Splitter"), this)}
    , m_customSplitterSpacing{new QCheckBox(tr("Use custom spacing"), this)}
    , m_splitterSpacing{new QSpinBox(this)}
    , m_layoutCombo{new QComboBox(this)}
    , m_deleteLayout{new QPushButton(this)}
{
    m_layoutTree->setHeaderHidden(true);
    m_layoutTree->setModel(m_model);

    for(auto* spinBox : {m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin}) {
        spinBox->setRange(-999, 999);
        spinBox->setSuffix(u" px"_s);
    }

    m_splitterSpacing->setRange(0, 999);
    m_splitterSpacing->setSuffix(u" px"_s);

    auto* marginsLayout = new QGridLayout(m_marginsGroup);

    int row{0};
    marginsLayout->addWidget(m_customMargins, row++, 0, 1, 4);
    marginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, m_marginsGroup), row, 0);
    marginsLayout->addWidget(m_leftMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, m_marginsGroup), row, 2);
    marginsLayout->addWidget(m_topMargin, row++, 3);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, m_marginsGroup), row, 0);
    marginsLayout->addWidget(m_rightMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, m_marginsGroup), row, 2);
    marginsLayout->addWidget(m_bottomMargin, row++, 3);

    auto* splitterLayout = new QGridLayout(m_splitterGroup);
    splitterLayout->addWidget(m_customSplitterSpacing, 0, 0, 1, 3);
    splitterLayout->addWidget(new QLabel(tr("Spacing") + u":"_s, m_splitterGroup), 1, 0);
    splitterLayout->addWidget(m_splitterSpacing, 1, 1);
    splitterLayout->setColumnStretch(2, 1);

    auto* newLayout       = new QPushButton(tr("New"), this);
    auto* renameLayout    = new QPushButton(tr("Rename"), this);
    auto* duplicateLayout = new QPushButton(tr("Duplicate"), this);
    setEqualButtonWidth({newLayout, m_deleteLayout, renameLayout, duplicateLayout});

    auto* topBarLayout = new QHBoxLayout();
    topBarLayout->addWidget(m_layoutCombo, 1);
    topBarLayout->addWidget(newLayout);
    topBarLayout->addWidget(m_deleteLayout);
    topBarLayout->addWidget(renameLayout);
    topBarLayout->addWidget(duplicateLayout);

    auto* layout = new QGridLayout(this);
    layout->addLayout(topBarLayout, 0, 0, 1, 2);
    layout->addWidget(m_layoutTree, 1, 0, 2, 1);
    layout->addWidget(m_marginsGroup, 1, 1, Qt::AlignTop);
    layout->addWidget(m_splitterGroup, 2, 1, Qt::AlignTop);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    m_layoutTree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_layoutTree, &QWidget::customContextMenuRequested, this,
                     &GuiLayoutPageWidget::onContextMenuRequested);

    QObject::connect(m_layoutCombo, &QComboBox::currentIndexChanged, this, &GuiLayoutPageWidget::onChangeLayout);

    QObject::connect(newLayout, &QPushButton::clicked, this, &GuiLayoutPageWidget::onNewLayout);
    QObject::connect(m_deleteLayout, &QPushButton::clicked, this, &GuiLayoutPageWidget::onDeleteLayout);
    QObject::connect(renameLayout, &QPushButton::clicked, this, &GuiLayoutPageWidget::onRenameLayout);
    QObject::connect(duplicateLayout, &QPushButton::clicked, this, &GuiLayoutPageWidget::onDuplicateLayout);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_layoutTree, &QTreeView::expandAll);
    QObject::connect(m_layoutTree->selectionModel(), &QItemSelectionModel::currentChanged, this,
                     &GuiLayoutPageWidget::onSelectionChanged);

    for(auto* spinBox : {m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin}) {
        QObject::connect(spinBox, &QSpinBox::valueChanged, this, &GuiLayoutPageWidget::updateModelMargins);
    }
    QObject::connect(m_customMargins, &QCheckBox::toggled, this, &GuiLayoutPageWidget::onCustomMarginsChanged);
    QObject::connect(m_splitterSpacing, &QSpinBox::valueChanged, this,
                     &GuiLayoutPageWidget::updateModelSplitterSpacing);
    QObject::connect(m_customSplitterSpacing, &QCheckBox::toggled, this,
                     &GuiLayoutPageWidget::onCustomSplitterSpacingChanged);

    updateMarginControls();
    updateSplitterControls();
}

void GuiLayoutPageWidget::load()
{
    m_loading = true;

    m_layoutCombo->clear();

    const auto addLayout = [this](const FyLayout& layout) {
        m_layoutCombo->addItem(layout.name());
    };

    const auto layouts = m_layoutProvider->layouts();
    const auto defaultLayout
        = std::ranges::find_if(layouts, [](const FyLayout& layout) { return layout.name() == u"Default"_s; });
    if(defaultLayout != layouts.cend()) {
        addLayout(*defaultLayout);
    }

    for(const auto& layout : m_layoutProvider->layouts()) {
        if(layout.name() != u"Default"_s) {
            addLayout(layout);
        }
    }

    const QString current = m_layoutProvider->currentLayout().name();
    const int idx         = m_layoutCombo->findText(current);
    if(idx >= 0) {
        m_layoutCombo->setCurrentIndex(idx);
    }

    m_model->populate(m_layoutProvider->currentLayout());
    m_layoutTree->setEnabled(idx >= 0);
    updateMarginControls();
    updateSplitterControls();
    updateButtonStates();

    m_loading = false;
}

void GuiLayoutPageWidget::apply()
{
    const FyLayout layout = m_model->layout();
    if(layout.isValid()) {
        const FyLayout currentLayout = m_layoutProvider->currentLayout();
        if(currentLayout.isValid() && layout.name() == currentLayout.name() && layout.json() == currentLayout.json()) {
            return;
        }

        const TreeSelectionGuard selectionGuard{m_layoutTree, m_model};
        m_editableLayout->changeLayout(layout);
        m_layoutProvider->saveCurrentLayout();
        load();
    }
}

void GuiLayoutPageWidget::reset() { }

void GuiLayoutPageWidget::onContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = m_layoutTree->indexAt(pos);
    if(!index.isValid()) {
        return;
    }
    m_layoutTree->setCurrentIndex(index);

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* moveUp = new QAction(tr("Move up"), menu);
    moveUp->setVisible(m_model->canMoveUp(index));
    QObject::connect(moveUp, &QAction::triggered, this, &GuiLayoutPageWidget::moveSelectionUp);
    menu->addAction(moveUp);

    auto* moveDown = new QAction(tr("Move down"), menu);
    moveDown->setVisible(m_model->canMoveDown(index));
    QObject::connect(moveDown, &QAction::triggered, this, &GuiLayoutPageWidget::moveSelectionDown);
    menu->addAction(moveDown);

    auto* remove = new QAction(tr("Remove"), menu);
    remove->setVisible(m_model->canRemove(index));
    QObject::connect(remove, &QAction::triggered, m_model, [this, index]() { m_model->remove(index); });
    menu->addAction(remove);

    if(!m_model->isDummy(index)) {
        menu->addSeparator();

        auto* cut = new QAction(tr("Cut"), menu);
        cut->setVisible(m_model->canCut(index));
        QObject::connect(cut, &QAction::triggered, this, &GuiLayoutPageWidget::cutSelection);
        menu->addAction(cut);

        auto* copy = new QAction(tr("Copy"), menu);
        copy->setVisible(m_model->canCopy(index));
        QObject::connect(copy, &QAction::triggered, this, &GuiLayoutPageWidget::copySelection);
        menu->addAction(copy);

        if(!m_clipboardItem.empty() && m_model->canPasteAfter(index)) {
            auto* paste = new QAction(tr("Paste"), menu);
            QObject::connect(paste, &QAction::triggered, this, &GuiLayoutPageWidget::pasteAfterSelection);
            menu->addAction(paste);
        }
    }

    menu->popup(m_layoutTree->viewport()->mapToGlobal(pos));
}

void GuiLayoutPageWidget::onChangeLayout()
{
    if(m_loading) {
        return;
    }

    const FyLayout layout = m_layoutProvider->layoutByName(m_layoutCombo->currentText());
    m_model->populate(layout);
    m_layoutTree->setEnabled(layout.isValid());
    updateButtonStates();
    updateMarginControls();
    updateSplitterControls();
}

void GuiLayoutPageWidget::onSelectionChanged()
{
    updateMarginControls();
    updateSplitterControls();
}

void GuiLayoutPageWidget::onCustomMarginsChanged(bool enabled)
{
    if(m_updatingMargins) {
        return;
    }

    if(enabled) {
        updateModelMargins();
    }
    else {
        m_model->clearMargins(m_layoutTree->currentIndex());
    }

    updateMarginControls();
}

void GuiLayoutPageWidget::onCustomSplitterSpacingChanged(bool enabled)
{
    if(m_updatingSplitterSpacing) {
        return;
    }

    if(enabled) {
        updateModelSplitterSpacing();
    }
    else {
        m_model->clearSplitterSpacing(m_layoutTree->currentIndex());
    }

    updateSplitterControls();
}

void GuiLayoutPageWidget::updateMarginControls()
{
    const QModelIndex index = m_layoutTree->currentIndex();
    const bool enabled      = m_model->hasConfigurableMargins(index);
    const bool custom       = m_model->hasCustomMargins(index);
    const QMargins margins  = m_model->margins(index);

    m_updatingMargins = true;
    m_customMargins->setChecked(custom);
    m_leftMargin->setValue(margins.left());
    m_topMargin->setValue(margins.top());
    m_rightMargin->setValue(margins.right());
    m_bottomMargin->setValue(margins.bottom());
    m_updatingMargins = false;

    m_marginsGroup->setEnabled(enabled);
    for(auto* spinBox : {m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin}) {
        spinBox->setEnabled(enabled && custom);
    }
}

void GuiLayoutPageWidget::updateSplitterControls()
{
    const QModelIndex index = m_layoutTree->currentIndex();
    const bool custom       = m_model->hasCustomSplitterSpacing(index);

    m_updatingSplitterSpacing = true;
    m_customSplitterSpacing->setChecked(custom);
    m_splitterSpacing->setValue(m_model->splitterSpacing(index));
    m_updatingSplitterSpacing = false;

    const bool enabled = m_model->hasConfigurableSplitterSpacing(index);

    m_splitterGroup->setVisible(enabled);
    m_splitterSpacing->setEnabled(enabled && custom);
}

void GuiLayoutPageWidget::updateModelMargins()
{
    if(m_updatingMargins || !m_customMargins->isChecked()) {
        return;
    }

    m_model->setMargins(m_layoutTree->currentIndex(),
                        {m_leftMargin->value(), m_topMargin->value(), m_rightMargin->value(), m_bottomMargin->value()});
}

void GuiLayoutPageWidget::updateModelSplitterSpacing()
{
    if(m_updatingSplitterSpacing || !m_customSplitterSpacing->isChecked()) {
        return;
    }

    m_model->setSplitterSpacing(m_layoutTree->currentIndex(), m_splitterSpacing->value());
}

void GuiLayoutPageWidget::updateButtonStates()
{
    const QString name  = m_layoutCombo->currentText();
    const bool canReset = m_layoutProvider->canResetLayout(name);

    m_deleteLayout->setText(canReset ? tr("Reset") : tr("Delete"));
    m_deleteLayout->setEnabled(canReset || m_layoutProvider->canDeleteLayout(name));
}

void GuiLayoutPageWidget::moveSelectionUp()
{
    m_model->moveUp(m_layoutTree->currentIndex());
}

void GuiLayoutPageWidget::moveSelectionDown()
{
    m_model->moveDown(m_layoutTree->currentIndex());
}

void GuiLayoutPageWidget::cutSelection()
{
    const QModelIndex index = m_layoutTree->currentIndex();
    if(!m_model->canCut(index)) {
        return;
    }

    m_clipboardItem = m_model->copyItem(index);
    m_model->remove(index);
}

void GuiLayoutPageWidget::copySelection()
{
    if(!m_model->canCopy(m_layoutTree->currentIndex())) {
        return;
    }

    m_clipboardItem = m_model->copyItem(m_layoutTree->currentIndex());
}

void GuiLayoutPageWidget::pasteAfterSelection()
{
    if(m_clipboardItem.empty() || !m_model->canPasteAfter(m_layoutTree->currentIndex())) {
        return;
    }

    TreeSelectionGuard selectionGuard{m_layoutTree, m_model};
    selectionGuard.moveLastRow(1);
    m_model->pasteAfter(m_layoutTree->currentIndex(), m_clipboardItem);
}

void GuiLayoutPageWidget::onNewLayout()
{
    const QString defaultName = m_layoutProvider->uniqueLayoutName(tr("New Layout"));
    bool success{false};
    const QString name = QInputDialog::getText(this, tr("New Layout"), tr("Layout Name") + u":"_s, QLineEdit::Normal,
                                               defaultName, &success)
                             .trimmed();

    QJsonObject layout;
    layout["Name"_L1] = name;

    if(success && !name.isEmpty() && m_layoutProvider->createLayout(name, FyLayout{name, layout})) {
        load();
        m_layoutCombo->setCurrentText(name);
    }
}

void GuiLayoutPageWidget::onDeleteLayout()
{
    const QString name = m_layoutCombo->currentText();
    if(name.isEmpty()) {
        return;
    }

    if(m_layoutProvider->canResetLayout(name)) {
        const bool wasCurrent = m_layoutProvider->currentLayout().name() == name;
        if(m_layoutProvider->resetLayout(name)) {
            if(wasCurrent) {
                m_editableLayout->changeLayout(m_layoutProvider->currentLayout());
            }
            load();
            m_layoutCombo->setCurrentText(name);
        }
        return;
    }

    QMessageBox msg{QMessageBox::Question, tr("Delete Layout"), tr("Delete layout \"%1\"?").arg(name),
                    QMessageBox::Yes | QMessageBox::No, this};
    msg.setDefaultButton(QMessageBox::No);
    if(msg.exec() != QMessageBox::Yes) {
        return;
    }

    const bool wasCurrent = m_layoutProvider->currentLayout().name() == name;
    if(m_layoutProvider->deleteLayout(name)) {
        if(wasCurrent && m_layoutProvider->currentLayout().isValid()) {
            m_editableLayout->changeLayout(m_layoutProvider->currentLayout());
        }
        load();
    }
}

void GuiLayoutPageWidget::onRenameLayout()
{
    const QString oldName = m_layoutCombo->currentText();
    if(oldName.isEmpty()) {
        return;
    }

    bool success{false};
    const QString newName = QInputDialog::getText(this, tr("Rename Layout"), tr("Layout Name") + u":"_s,
                                                  QLineEdit::Normal, oldName, &success)
                                .trimmed();

    if(success && m_layoutProvider->renameLayout(oldName, newName)) {
        load();
        m_layoutCombo->setCurrentText(newName);
    }
}

void GuiLayoutPageWidget::onDuplicateLayout()
{
    const QString sourceName = m_layoutCombo->currentText();
    if(sourceName.isEmpty()) {
        return;
    }

    const QString defaultName = m_layoutProvider->uniqueLayoutName(tr("%1 Copy").arg(sourceName));
    bool success{false};
    const QString newName = QInputDialog::getText(this, tr("Duplicate Layout"), tr("Layout Name") + u":"_s,
                                                  QLineEdit::Normal, defaultName, &success)
                                .trimmed();

    if(success && m_layoutProvider->duplicateLayout(sourceName, newName)) {
        load();
        m_layoutCombo->setCurrentText(newName);
    }
}

GuiLayoutPage::GuiLayoutPage(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                             WidgetProvider* widgetProvider, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceLayout);
    setName(tr("Layout"));
    setCategory({tr("Interface"), tr("Layout")});
    setWidgetCreator([layoutProvider, editableLayout, settings, widgetProvider] {
        return new GuiLayoutPageWidget(layoutProvider, editableLayout, widgetProvider, settings);
    });
}
} // namespace Fooyin

#include "guilayoutpage.moc"
#include "moc_guilayoutpage.cpp"
