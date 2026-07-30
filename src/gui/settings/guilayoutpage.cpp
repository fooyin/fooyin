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

#include <core/constants.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/theme/fytheme.h>
#include <gui/widgetprovider.h>
#include <utils/jsonutils.h>
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
#include <optional>
#include <ranges>
#include <unordered_map>
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

QString arrayItemId(const QJsonValue& value)
{
    if(!value.isObject()) {
        return {};
    }

    const auto object = value.toObject();
    if(object.size() != 1 || !object.constBegin()->isObject()) {
        return {};
    }

    const QString id = object.constBegin()->toObject().value("ID"_L1).toString();
    return id.isEmpty() ? QString{} : object.constBegin().key() + QLatin1String{Constants::UnitSeparator} + id;
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
    struct LayoutDraft
    {
        FyLayout baseline;
        FyLayout layout;
        bool applyTheme{false};
        bool applyWindowSize{false};
    };

    void refreshLayouts(const QString& selectedName);
    void showLayout(const QString& name);
    void saveDisplayedDraft();
    void mergeExternalLayout(const FyLayout& layout);
    void onCurrentLayoutChanged(const FyLayout& layout);
    void onLayoutChanged(const FyLayout& layout);
    void onLayoutAdded(const FyLayout& layout);
    void onLayoutRemoved(const QString& name);
    [[nodiscard]] FyLayout finaliseDraft(const LayoutDraft& draft) const;

    void onContextMenuRequested(const QPoint& pos);
    void onChangeLayout();
    void onSelectionChanged();
    void onCustomMarginsChanged(bool enabled);
    void onCustomSplitterSpacingChanged(bool enabled);
    void onSplitterLockChanged(bool locked);
    void updateMarginControls();
    void updateSplitterControls();
    void updateMetadataControls();
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
    QLabel* m_splitterSpacingLabel;
    QSpinBox* m_splitterSpacing;
    QCheckBox* m_lockSplitterSize;
    QGroupBox* m_metadataGroup;
    QCheckBox* m_applyTheme;
    QCheckBox* m_applyWindowSize;

    QComboBox* m_layoutCombo;
    QPushButton* m_deleteLayout;
    QJsonObject m_clipboardItem;
    std::unordered_map<QString, LayoutDraft> m_drafts;
    QString m_displayedLayoutName;
    bool m_loading{false};
    bool m_applying{false};
    bool m_updatingMargins{false};
    bool m_updatingSplitterSpacing{false};
    bool m_updatingSplitterLock{false};
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
    , m_splitterSpacingLabel{new QLabel(tr("Spacing") + u":"_s, m_splitterGroup)}
    , m_splitterSpacing{new QSpinBox(this)}
    , m_lockSplitterSize{new QCheckBox(this)}
    , m_metadataGroup{new QGroupBox(tr("Layout options"), this)}
    , m_applyTheme{new QCheckBox(tr("Restore theme when switching to this layout"), this)}
    , m_applyWindowSize{new QCheckBox(tr("Restore window size when switching to this layout"), this)}
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
    splitterLayout->addWidget(m_splitterSpacingLabel, 1, 0);
    splitterLayout->addWidget(m_splitterSpacing, 1, 1);
    splitterLayout->addWidget(m_lockSplitterSize, 2, 0, 1, 3);
    splitterLayout->setColumnStretch(2, 1);

    auto* metadataLayout = new QGridLayout(m_metadataGroup);
    metadataLayout->addWidget(m_applyTheme, 0, 0);
    metadataLayout->addWidget(m_applyWindowSize, 1, 0);

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
    layout->addWidget(m_layoutTree, 1, 0, 3, 1);
    layout->addWidget(m_marginsGroup, 1, 1, Qt::AlignTop);
    layout->addWidget(m_splitterGroup, 2, 1, Qt::AlignTop);
    layout->addWidget(m_metadataGroup, 3, 1, Qt::AlignTop);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(3, 1);

    m_layoutTree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_layoutTree, &QWidget::customContextMenuRequested, this,
                     &GuiLayoutPageWidget::onContextMenuRequested);

    QObject::connect(m_layoutCombo, &QComboBox::currentIndexChanged, this, &GuiLayoutPageWidget::onChangeLayout);
    QObject::connect(m_layoutProvider, &LayoutProvider::currentLayoutChanged, this,
                     &GuiLayoutPageWidget::onCurrentLayoutChanged);
    QObject::connect(m_layoutProvider, &LayoutProvider::layoutChanged, this, &GuiLayoutPageWidget::onLayoutChanged);
    QObject::connect(m_layoutProvider, &LayoutProvider::layoutAdded, this, &GuiLayoutPageWidget::onLayoutAdded);
    QObject::connect(m_layoutProvider, &LayoutProvider::layoutRemoved, this, &GuiLayoutPageWidget::onLayoutRemoved);

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
    QObject::connect(m_lockSplitterSize, &QCheckBox::toggled, this, &GuiLayoutPageWidget::onSplitterLockChanged);

    updateMarginControls();
    updateSplitterControls();
    updateMetadataControls();
}

void GuiLayoutPageWidget::load()
{
    m_drafts.clear();
    refreshLayouts(m_layoutProvider->currentLayout().name());
}

void GuiLayoutPageWidget::refreshLayouts(const QString& selectedName)
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

    const int idx = m_layoutCombo->findText(selectedName);
    if(idx >= 0) {
        m_layoutCombo->setCurrentIndex(idx);
    }

    showLayout(idx >= 0 ? selectedName : QString{});
    m_layoutTree->setEnabled(idx >= 0);
    updateButtonStates();

    m_loading = false;
}

void GuiLayoutPageWidget::showLayout(const QString& name)
{
    m_displayedLayoutName = name;

    if(name.isEmpty()) {
        m_model->populate({});
    }
    else {
        const auto draft = m_drafts.find(name);
        if(draft != m_drafts.end()) {
            m_model->populate(draft->second.layout);
        }
        else {
            const FyLayout layout = m_layoutProvider->layoutByName(name);
            m_drafts.emplace(name, LayoutDraft{
                                       .baseline        = layout,
                                       .layout          = layout,
                                       .applyTheme      = layout.appliesTheme(),
                                       .applyWindowSize = layout.appliesWindowSize(),
                                   });
            m_model->populate(layout);
        }
    }

    updateMarginControls();
    updateSplitterControls();
    updateMetadataControls();

    if(const auto draft = m_drafts.find(name); draft != m_drafts.end()) {
        m_applyTheme->setChecked(draft->second.applyTheme);
        m_applyWindowSize->setChecked(draft->second.applyWindowSize);
    }
}

void GuiLayoutPageWidget::saveDisplayedDraft()
{
    if(m_loading || m_displayedLayoutName.isEmpty()) {
        return;
    }

    const FyLayout layout = m_model->layout();
    if(!layout.isValid()) {
        return;
    }

    auto draft = m_drafts.find(m_displayedLayoutName);
    if(draft == m_drafts.end()) {
        const FyLayout baseline = m_layoutProvider->layoutByName(m_displayedLayoutName);
        draft                   = m_drafts
                                      .emplace(m_displayedLayoutName,
                                               LayoutDraft{
                                                   .baseline        = baseline,
                                                   .layout          = layout,
                                                   .applyTheme      = m_applyTheme->isChecked(),
                                                   .applyWindowSize = m_applyWindowSize->isChecked(),
                                               })
                                      .first;
    }

    draft->second.layout          = layout;
    draft->second.applyTheme      = m_applyTheme->isChecked();
    draft->second.applyWindowSize = m_applyWindowSize->isChecked();
}

void GuiLayoutPageWidget::mergeExternalLayout(const FyLayout& layout)
{
    if(!layout.isValid()) {
        return;
    }

    const auto draft = m_drafts.find(layout.name());
    if(draft == m_drafts.end()) {
        return;
    }

    auto& state = draft->second;
    const auto mergedJson
        = Utils::mergeJsonThreeWay(state.baseline.json(), layout.json(), state.layout.json(), arrayItemId).toObject();

    if(state.applyTheme == state.baseline.appliesTheme()) {
        state.applyTheme = layout.appliesTheme();
    }
    if(state.applyWindowSize == state.baseline.appliesWindowSize()) {
        state.applyWindowSize = layout.appliesWindowSize();
    }

    state.baseline = layout;
    state.layout   = FyLayout{layout.name(), mergedJson};
}

void GuiLayoutPageWidget::onCurrentLayoutChanged(const FyLayout& layout)
{
    if(m_applying) {
        return;
    }

    saveDisplayedDraft();
    mergeExternalLayout(layout);
    refreshLayouts(layout.name());
}

void GuiLayoutPageWidget::onLayoutChanged(const FyLayout& layout)
{
    if(m_applying) {
        return;
    }

    saveDisplayedDraft();
    mergeExternalLayout(layout);

    if(layout.name() == m_displayedLayoutName) {
        const TreeSelectionGuard selectionGuard{m_layoutTree, m_model};
        showLayout(layout.name());
    }
}

void GuiLayoutPageWidget::onLayoutAdded(const FyLayout& /*layout*/)
{
    if(!m_applying) {
        saveDisplayedDraft();
        refreshLayouts(m_displayedLayoutName);
    }
}

void GuiLayoutPageWidget::onLayoutRemoved(const QString& name)
{
    if(m_applying) {
        return;
    }

    saveDisplayedDraft();
    m_drafts.erase(name);
    const QString selected
        = name == m_displayedLayoutName ? m_layoutProvider->currentLayout().name() : m_displayedLayoutName;
    refreshLayouts(selected);
}

FyLayout GuiLayoutPageWidget::finaliseDraft(const LayoutDraft& draft) const
{
    FyLayout layout{draft.layout};

    if(draft.applyTheme) {
        const auto theme = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
        layout.removeTheme();
        layout.setAppliesTheme(true);
        if(theme.isValid()) {
            layout.saveTheme(theme);
        }
    }
    else {
        layout.removeTheme();
    }

    if(draft.applyWindowSize) {
        layout.saveWindowSize();
    }
    else {
        layout.removeWindowSize();
    }

    return layout;
}

void GuiLayoutPageWidget::apply()
{
    saveDisplayedDraft();

    const QString currentName = m_layoutProvider->currentLayout().name();

    std::vector<FyLayout> layouts;
    layouts.reserve(m_drafts.size());

    for(const auto& draft : m_drafts | std::views::values) {
        const bool changed = draft.layout.json() != draft.baseline.json()
                          || draft.applyTheme != draft.baseline.appliesTheme()
                          || draft.applyWindowSize != draft.baseline.appliesWindowSize();
        if(!changed) {
            continue;
        }

        FyLayout layout = finaliseDraft(draft);
        if(layout.isValid()) {
            layouts.push_back(std::move(layout));
        }
    }

    m_applying = true;

    for(const auto& layout : layouts) {
        if(layout.name() != currentName) {
            m_layoutProvider->saveLayout(layout);
        }
    }
    if(const auto current = std::ranges::find(layouts, currentName, &FyLayout::name); current != layouts.end()) {
        const TreeSelectionGuard selectionGuard{m_layoutTree, m_model};
        m_editableLayout->changeLayout(*current);
        m_layoutProvider->saveCurrentLayout();
    }

    m_applying = false;
    load();
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

    saveDisplayedDraft();

    const QString name = m_layoutCombo->currentText();
    showLayout(name);
    m_layoutTree->setEnabled(m_model->layout().isValid());
    updateButtonStates();
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

void GuiLayoutPageWidget::onSplitterLockChanged(bool locked)
{
    if(m_updatingSplitterLock) {
        return;
    }

    m_model->setSplitterItemLocked(m_layoutTree->currentIndex(), locked);
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
    const QModelIndex index   = m_layoutTree->currentIndex();
    const bool spacingEnabled = m_model->hasConfigurableSplitterSpacing(index);
    const bool customSpacing  = m_model->hasCustomSplitterSpacing(index);

    m_updatingSplitterSpacing = true;
    m_customSplitterSpacing->setChecked(customSpacing);
    m_splitterSpacing->setValue(m_model->splitterSpacing(index));
    m_updatingSplitterSpacing = false;

    m_customSplitterSpacing->setVisible(spacingEnabled);
    m_splitterSpacingLabel->setVisible(spacingEnabled);
    m_splitterSpacing->setVisible(spacingEnabled);
    m_splitterSpacing->setEnabled(customSpacing);

    const bool lockEnabled = m_model->hasConfigurableSplitterLock(index);
    m_lockSplitterSize->setVisible(lockEnabled);

    if(lockEnabled) {
        const bool lockWidth = m_model->splitterLockOrientation(index) == Qt::Horizontal;
        m_lockSplitterSize->setText(lockWidth ? tr("Lock width") : tr("Lock height"));
        m_lockSplitterSize->setStatusTip(
            lockWidth
                ? tr("Keep the width unchanged during automatic resizing; splitter handles can still resize it")
                : tr("Keep the height unchanged during automatic resizing; splitter handles can still resize it"));

        const bool locked      = m_model->isSplitterItemLocked(index);
        m_updatingSplitterLock = true;
        m_lockSplitterSize->setChecked(locked);
        m_updatingSplitterLock = false;
        m_lockSplitterSize->setEnabled(locked || m_model->canLockSplitterItem(index));
    }

    m_splitterGroup->setVisible(spacingEnabled || lockEnabled);
}

void GuiLayoutPageWidget::updateMetadataControls()
{
    const FyLayout layout = m_model->layout();
    const bool enabled    = layout.isValid();

    m_metadataGroup->setEnabled(enabled);
    m_applyTheme->setChecked(enabled && layout.appliesTheme());
    m_applyWindowSize->setChecked(enabled && layout.appliesWindowSize());
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

    const QJsonObject layout{
        {u"Name"_s, name}, {u"Version"_s, 1}, {u"Widgets"_s, QJsonArray{QJsonObject{{u"Playlist"_s, QJsonObject{}}}}}};

    if(success && !name.isEmpty() && m_layoutProvider->createLayout(name, FyLayout{name, layout})) {
        refreshLayouts(name);
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
            m_drafts.erase(name);
            refreshLayouts(name);
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
        m_drafts.erase(name);
        refreshLayouts(m_layoutProvider->currentLayout().name());
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

    saveDisplayedDraft();
    const auto oldDraft = m_drafts.find(oldName);
    const std::optional<LayoutDraft> renamedDraft
        = oldDraft == m_drafts.end() ? std::nullopt : std::optional{oldDraft->second};

    if(success && m_layoutProvider->renameLayout(oldName, newName)) {
        if(renamedDraft) {
            auto state      = *renamedDraft;
            auto json       = state.layout.json();
            json["Name"_L1] = newName;
            state.baseline  = m_layoutProvider->layoutByName(newName);
            state.layout    = FyLayout{newName, json};
            m_drafts.insert_or_assign(newName, std::move(state));
        }
        refreshLayouts(newName);
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

    saveDisplayedDraft();
    const auto source = m_drafts.find(sourceName);
    const FyLayout sourceLayout
        = source == m_drafts.end() ? m_layoutProvider->layoutByName(sourceName) : source->second.layout;

    if(success && m_layoutProvider->createLayout(newName, sourceLayout)) {
        refreshLayouts(newName);
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
