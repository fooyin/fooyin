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

#include "themeappearancedialog.h"

#include "appearancepropertypanel.h"
#include "checkappearanceeditor.h"
#include "componentappearanceeditor.h"
#include "groupboxappearanceeditor.h"
#include "headerappearanceeditor.h"
#include "inputappearanceeditor.h"
#include "itemviewappearanceeditor.h"
#include "menuappearanceeditor.h"
#include "progressbarappearanceeditor.h"
#include "pushbuttonappearanceeditor.h"
#include "scrollbarappearanceeditor.h"
#include "sliderappearanceeditor.h"
#include "splitterappearanceeditor.h"
#include "style/fooyinstyle.h"
#include "tabappearanceeditor.h"
#include "toolbarappearanceeditor.h"
#include "toolbuttonappearanceeditor.h"
#include "tooltipappearanceeditor.h"

#include <core/coresettings.h>
#include <gui/widgets/hintlabel.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <array>
#include <iterator>
#include <map>

using namespace Qt::StringLiterals;

constexpr auto ComponentRole = Qt::UserRole;
constexpr auto PartRole      = Qt::UserRole + 1;

constexpr auto DialogStateGroup    = "ThemeAppearanceDialog"_L1;
constexpr auto DockState           = "ThemeAppearanceDialog/DockState"_L1;
constexpr auto DockStateVersion    = 1;
constexpr auto PropertyColumnWidth = "ThemeAppearanceDialog/PropertyColumnWidth"_L1;
constexpr auto ComponentsDockWidth = 250;
constexpr auto PropertiesDockWidth = 320;

namespace Fooyin {
ThemeAppearanceDialog::ThemeAppearanceDialog(QJsonObject appearance, const QPalette& palette, const QFont& font,
                                             QWidget* parent)
    : QDialog{parent}
    , m_appearance{std::move(appearance)}
    , m_workspace{new QMainWindow(this)}
    , m_componentsDock{new QDockWidget(tr("Components"), m_workspace)}
    , m_propertiesDock{new QDockWidget(tr("Properties"), m_workspace)}
    , m_components{new QTreeWidget(this)}
    , m_componentFilter{new QLineEdit(this)}
    , m_previews{new QStackedWidget(this)}
    , m_propertyModel{new AppearancePropertyModel(this)}
    , m_properties{new AppearancePropertyPanel(m_propertyModel, this)}
    , m_generalPreview{new QWidget(this)}
    , m_generalProperties{new AppearancePropertyPage(m_propertyModel, this)}
    , m_propertyTitle{new QLabel(this)}
    , m_stateControls{new QWidget(this)}
    , m_propertyState{new QComboBox(m_stateControls)}
    , m_resetPart{new QPushButton(this)}
    , m_resetComponent{new QPushButton(tr("Reset component overrides"), this)}
    , m_error{new HintLabel(this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
    , m_activeEditor{nullptr}
    , m_selectingPreview{false}
{
    setWindowTitle(tr("Theme Editor"));

    m_components->setHeaderHidden(true);
    m_components->setSelectionMode(QAbstractItemView::SingleSelection);
    m_components->setMinimumWidth(fontMetrics().horizontalAdvance(tr("Components")) + 40);
    m_componentFilter->setPlaceholderText(tr("Filter…"));
    m_componentFilter->setClearButtonEnabled(true);

    QObject::connect(m_componentFilter, &QLineEdit::textChanged, this, &ThemeAppearanceDialog::filterComponents);
    QObject::connect(m_components, &QTreeWidget::currentItemChanged, this, &ThemeAppearanceDialog::selectItem);

    auto* navigation       = new QWidget(this);
    auto* navigationLayout = new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(0, 4, 0, 0);
    navigationLayout->addWidget(m_componentFilter);
    navigationLayout->addWidget(m_components, 1);

    auto* canvas       = new QWidget(this);
    auto* canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setContentsMargins({});
    canvasLayout->addWidget(m_previews, 1);
    canvas->setMinimumWidth(240);

    m_propertyTitle->setWordWrap(true);

    auto* propertyStateLayout = new QHBoxLayout(m_stateControls);
    propertyStateLayout->setContentsMargins({});
    propertyStateLayout->addWidget(new QLabel(tr("State") + u":"_s, m_stateControls));
    propertyStateLayout->addWidget(m_propertyState, 1);

    QObject::connect(m_propertyState, &QComboBox::currentIndexChanged, this, &ThemeAppearanceDialog::selectState);

    auto* properties       = new QWidget(this);
    auto* propertiesLayout = new QVBoxLayout(properties);
    propertiesLayout->setContentsMargins({});
    propertiesLayout->addWidget(m_propertyTitle);
    propertiesLayout->addWidget(m_stateControls);
    propertiesLayout->addWidget(m_properties, 1);

    m_error->setType(HintLabel::Type::Warning);
    m_error->setWordWrap(true);

    QObject::connect(m_resetComponent, &QPushButton::clicked, this, [this]() {
        if(m_activeEditor) {
            m_activeEditor->setOverrides({});
        }
        else {
            m_generalProperties->resetOverrides();
        }
    });
    QObject::connect(m_resetPart, &QPushButton::clicked, this, [this] {
        if(m_activeEditor) {
            if(auto* page = m_activeEditor->currentPropertyPage()) {
                page->resetOverrides();
            }
        }
    });

    auto* inspector       = new QWidget(this);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(4, 4, 4, 4);
    inspectorLayout->addWidget(properties, 1);
    inspectorLayout->addWidget(m_error);
    inspectorLayout->addWidget(m_resetPart);
    inspectorLayout->addWidget(m_resetComponent);
    inspector->setMinimumWidth(fontMetrics().horizontalAdvance(tr("Background colour")) + 170);

    m_workspace->setWindowFlags(Qt::Widget);
    m_workspace->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks
                                | QMainWindow::AllowTabbedDocks);
    m_workspace->setContextMenuPolicy(Qt::NoContextMenu);
    m_workspace->setCentralWidget(canvas);

    m_componentsDock->setWidget(navigation);
    m_propertiesDock->setWidget(inspector);

    for(auto* dock : {m_componentsDock, m_propertiesDock}) {
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    }
    m_workspace->addDockWidget(Qt::LeftDockWidgetArea, m_componentsDock);
    m_workspace->addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    const auto* resetLayout = m_buttons->addButton(tr("Reset panel layout"), QDialogButtonBox::ResetRole);
    QObject::connect(resetLayout, &QPushButton::clicked, this, &ThemeAppearanceDialog::resetPanelLayout);

    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_workspace, 1);
    layout->addWidget(m_buttons);

    m_propertyModel->setColourPalette(palette);
    addGeneralPage(palette, font);
    addComponent(std::make_unique<CheckAppearanceEditor>(m_propertyModel, false), palette, font);
    addComponent(std::make_unique<CheckAppearanceEditor>(m_propertyModel, true), palette, font);
    addComponent(std::make_unique<GroupBoxAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<InputAppearanceEditor>(m_propertyModel, InputType::ComboBox), palette, font);
    addComponent(std::make_unique<InputAppearanceEditor>(m_propertyModel, InputType::LineEdit), palette, font);
    auto* itemViews = addComponent(std::make_unique<ItemViewAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<HeaderAppearanceEditor>(m_propertyModel), palette, font, itemViews);
    addComponent(std::make_unique<MenuAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<PushButtonAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<ProgressBarAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<ScrollBarAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<SliderAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<SplitterAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<InputAppearanceEditor>(m_propertyModel, InputType::SpinBox), palette, font);
    addComponent(std::make_unique<TabAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<ToolButtonAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<ToolBarAppearanceEditor>(m_propertyModel), palette, font);
    addComponent(std::make_unique<ToolTipAppearanceEditor>(m_propertyModel), palette, font);

    m_components->expandAll();

    for(int item{0}; item < m_components->topLevelItemCount(); ++item) {
        m_components->topLevelItem(item)->setExpanded(false);
    }

    updateValidation();

    resize(Utils::proportionateSize(this, 0.5, 0.5));
    m_workspace->resizeDocks({m_componentsDock, m_propertiesDock}, {ComponentsDockWidth, PropertiesDockWidth},
                             Qt::Horizontal);

    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings, DialogStateGroup);
    if(const auto state = stateSettings.value(DockState).toByteArray(); !state.isEmpty()) {
        m_workspace->restoreState(state, DockStateVersion);
    }
    for(auto* dock : {m_componentsDock, m_propertiesDock}) {
        if(dock->isFloating()) {
            dock->hide();
        }
    }
    m_properties->view()->header()->resizeSection(0,
                                                  std::max(60, stateSettings.value(PropertyColumnWidth, 170).toInt()));
}

ThemeAppearanceDialog::~ThemeAppearanceDialog()
{
    const auto children = m_generalPreview->findChildren<QWidget*>();
    for(auto* widget : children) {
        widget->setStyle(nullptr);
    }
    m_generalPreview->setStyle(nullptr);
}

QJsonObject ThemeAppearanceDialog::appearance() const
{
    auto appearance{m_appearance};

    for(const auto& editor : m_editors) {
        const auto overrides = editor->overrides();
        if(overrides.isEmpty()) {
            appearance.remove(editor->key());
        }
        else {
            appearance.insert(editor->key(), overrides);
        }
    }

    return appearance;
}

void ThemeAppearanceDialog::done(int result)
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings, DialogStateGroup);
    stateSettings.setValue(DockState, m_workspace->saveState(DockStateVersion));
    stateSettings.setValue(PropertyColumnWidth, m_properties->view()->header()->sectionSize(0));

    QDialog::done(result);
}

void ThemeAppearanceDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    for(auto* dock : {m_componentsDock, m_propertiesDock}) {
        if(dock->isFloating()) {
            dock->show();
        }
    }
}

void ThemeAppearanceDialog::hideEvent(QHideEvent* event)
{
    for(auto* dock : {m_componentsDock, m_propertiesDock}) {
        if(dock->isFloating()) {
            dock->hide();
        }
    }
    QDialog::hideEvent(event);
}

void ThemeAppearanceDialog::keyPressEvent(QKeyEvent* event)
{
    if((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_previews->isAncestorOf(focusWidget())) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ThemeAppearanceDialog::addGeneralPage(const QPalette& palette, const QFont& font)
{
    auto* item = new QTreeWidgetItem(m_components, {tr("General")});
    item->setData(0, ComponentRole, -1);

    m_previews->addWidget(m_generalPreview);

    m_generalProperties->addProperty(
        {.label            = tr("State transition duration"),
         .group            = {},
         .key              = u"transitionDuration"_s,
         .defaultValue     = ThemeAppearance{}.transitionDuration,
         .type             = AppearanceProperty::Type::Number,
         .minimum          = 0,
         .maximum          = 400,
         .suffix           = tr(" ms"),
         .specialValueText = tr("Off"),
         .toolTip          = tr("Duration of colour changes when hovering, pressing or focusing controls.\n"
                                "Set to zero to disable these transitions.")});
    m_generalProperties->addProperty({.label        = tr("Layout"),
                                      .group        = {},
                                      .key          = u"layout"_s,
                                      .defaultValue = QJsonValue::Undefined,
                                      .type         = AppearanceProperty::Type::Group});

    for(const auto& [label, key] : std::array{
            std::pair{tr("Horizontal spacing"), u"horizontalSpacing"_s},
            std::pair{tr("Vertical spacing"), u"verticalSpacing"_s}, std::pair{tr("Left margin"), u"leftMargin"_s},
            std::pair{tr("Top margin"), u"topMargin"_s}, std::pair{tr("Right margin"), u"rightMargin"_s},
            std::pair{tr("Bottom margin"), u"bottomMargin"_s}}) {
        m_generalProperties->addProperty({.label            = label,
                                          .group            = u"layout"_s,
                                          .key              = key,
                                          .defaultValue     = -1,
                                          .type             = AppearanceProperty::Type::Number,
                                          .minimum          = -1,
                                          .maximum          = 128,
                                          .suffix           = tr(" px"),
                                          .specialValueText = tr("Inherited"),
                                          .toolTip          = tr("Applies to layouts without an explicit value.\n"
                                                                 "Inherited uses the default style metric.")});
    }

    m_generalProperties->setOverrides(m_appearance);
    m_generalProperties->addHint(new HintLabel(
        tr("Layout settings apply where spacing and margins are not set explicitly."), m_generalProperties));

    QObject::connect(m_generalProperties, &AppearancePropertyPage::propertiesEdited, this,
                     [this](const std::vector<AppearancePropertyEdit>& edits) {
                         for(const auto& edit : edits) {
                             m_appearance
                                 = updateAppearanceProperty(std::move(m_appearance), edit.group, edit.key, edit.value);
                         }
                         m_generalProperties->setOverrides(m_appearance);
                         updateValidation();
                     });

    auto* frame   = new QGroupBox(tr("Preview"), m_generalPreview);
    auto* samples = new QWidget(frame);

    auto* button = new QPushButton(tr("Play selection"), samples);
    button->setAutoDefault(false);

    auto* check = new QCheckBox(tr("Enable ReplayGain"), samples);

    auto* input = new QLineEdit(samples);
    input->setPlaceholderText(tr("Search library…"));

    auto* sampleLayout = new QFormLayout(samples);
    sampleLayout->addRow(tr("Button"), button);
    sampleLayout->addRow(tr("Checkbox"), check);
    sampleLayout->addRow(tr("Text input"), input);

    m_generalPreview->setPalette(palette);
    m_generalPreview->setFont(font);
    m_generalPreview->setAutoFillBackground(true);

    auto* sampleContainer = new QVBoxLayout(frame);
    sampleContainer->addStretch();
    sampleContainer->addWidget(samples, 0, Qt::AlignCenter);
    sampleContainer->addStretch();

    auto* previewLayout = new QVBoxLayout(m_generalPreview);
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(frame);
    m_properties->setPage(m_generalProperties);
}

QTreeWidgetItem* ThemeAppearanceDialog::addComponent(std::unique_ptr<ComponentAppearanceEditor> editor,
                                                     const QPalette& palette, const QFont& font,
                                                     QTreeWidgetItem* parent)
{
    editor->setOverrides(m_appearance.value(editor->key()).toObject());
    editor->setPreviewPalette(palette, font);

    const int index = static_cast<int>(m_editors.size());

    auto* component = editor.get();
    m_previews->addWidget(component->previewWidget());

    auto* root = new QTreeWidgetItem({component->title()});
    if(parent) {
        parent->addChild(root);
    }
    else {
        int row{1};
        while(row < m_components->topLevelItemCount()
              && QString::localeAwareCompare(m_components->topLevelItem(row)->text(0), component->title()) < 0) {
            ++row;
        }
        m_components->insertTopLevelItem(row, root);
    }
    root->setData(0, ComponentRole, index);
    root->setData(0, PartRole, QString{});

    std::map<QString, QTreeWidgetItem*> items{{QString{}, root}};
    const auto& parts = component->parts();
    for(const auto& part : parts) {
        auto* item = new QTreeWidgetItem(items.at(part.parentId), {part.title});
        item->setData(0, ComponentRole, index);
        item->setData(0, PartRole, part.id);
        items.emplace(part.id, item);
    }

    QObject::connect(component, &ComponentAppearanceEditor::changed, this, &ThemeAppearanceDialog::updateValidation);
    QObject::connect(component, &ComponentAppearanceEditor::partSelected, this,
                     [this, component](const QString& part) { selectPreviewPart(component, part); });
    QObject::connect(component, &ComponentAppearanceEditor::previewStateRequested, this, [this, component](auto state) {
        if(component == m_activeEditor) {
            component->setPreviewState(state);
        }
    });

    m_editors.push_back(std::move(editor));
    return root;
}

void ThemeAppearanceDialog::selectItem(QTreeWidgetItem* item)
{
    if(!item) {
        return;
    }

    const int component = item->data(0, ComponentRole).toInt();
    if(component < 0) {
        m_activeEditor = nullptr;
        m_previews->setCurrentWidget(m_generalPreview);
        m_properties->setPage(m_generalProperties);
        m_propertyTitle->setText(tr("General"));
        m_stateControls->hide();
        m_resetPart->hide();
        m_resetComponent->setText(tr("Reset general overrides"));
        updateValidation();
        return;
    }

    m_resetComponent->setText(tr("Reset component overrides"));
    m_activeEditor = m_editors.at(component).get();

    m_previews->setCurrentWidget(m_activeEditor->previewWidget());

    const auto part = item->data(0, PartRole).toString();
    m_activePart    = {.id = part, .title = item->text(0), .parentId = {}};

    const auto& parts = m_activeEditor->parts();
    if(const auto entry = std::ranges::find(parts, part, &AppearancePart::id); entry != parts.end()) {
        m_activePart = *entry;
    }

    {
        const QSignalBlocker blocker{m_propertyState};

        m_propertyState->clear();
        for(const auto& state : m_activePart.states) {
            m_propertyState->addItem(state.title, state.pageId);
        }

        if(!m_activePart.states.empty()) {
            const auto state = std::ranges::find(m_activePart.states, m_selectedState, &AppearanceState::id);
            const auto index
                = state == m_activePart.states.end() ? 0 : std::distance(m_activePart.states.begin(), state);
            m_propertyState->setCurrentIndex(static_cast<int>(index));
        }
    }

    m_stateControls->setVisible(!m_activePart.states.empty());
    m_resetPart->setVisible(
        std::ranges::any_of(parts, [](const AppearancePart& entry) { return !entry.states.empty(); }));

    selectState();

    QStringList path;
    for(const auto* ancestor = item; ancestor; ancestor = ancestor->parent()) {
        path.prepend(ancestor->text(0));
    }
    m_propertyTitle->setText(path.join(u" → "_s));

    updateValidation();
}

void ThemeAppearanceDialog::selectPreviewPart(ComponentAppearanceEditor* editor, const QString& part)
{
    m_componentFilter->clear();

    QString target{part};
    QString stateId;
    for(const auto& entry : editor->parts()) {
        const auto state = std::ranges::find(entry.states, part, &AppearanceState::pageId);
        if(state != entry.states.end()) {
            target  = entry.id;
            stateId = state->id;
            break;
        }
    }

    for(QTreeWidgetItemIterator it{m_components}; *it; ++it) {
        const int component = (*it)->data(0, ComponentRole).toInt();
        if(component >= 0 && m_editors.at(component).get() == editor && (*it)->data(0, PartRole).toString() == target) {
            for(auto* parent = (*it)->parent(); parent; parent = parent->parent()) {
                parent->setExpanded(true);
            }

            m_selectingPreview = true;

            if(!stateId.isEmpty()) {
                m_selectedState = std::move(stateId);
            }

            const bool alreadySelected = m_components->currentItem() == *it;
            m_components->setCurrentItem(*it);
            if(alreadySelected) {
                selectItem(*it);
            }

            m_components->scrollToItem(*it);
            m_selectingPreview = false;
            break;
        }
    }
}

void ThemeAppearanceDialog::filterComponents(const QString& text)
{
    const auto filterItems = [&](auto&& self, QTreeWidgetItem* item, bool parentMatches) -> bool {
        const bool matches = parentMatches || item->text(0).contains(text, Qt::CaseInsensitive);

        bool visible{matches};
        for(int child{0}; child < item->childCount(); ++child) {
            visible = self(self, item->child(child), matches) || visible;
        }
        item->setHidden(!visible);
        return visible;
    };

    for(int item{0}; item < m_components->topLevelItemCount(); ++item) {
        filterItems(filterItems, m_components->topLevelItem(item), false);
    }
    if(!text.isEmpty()) {
        m_components->expandAll();
    }
}

void ThemeAppearanceDialog::updateValidation()
{
    updateTransitions();
    updateStateOverrides();

    QString firstError;
    for(const auto& editor : m_editors) {
        firstError = editor->validationError();
        if(!firstError.isEmpty()) {
            break;
        }
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(firstError.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setToolTip(firstError);

    const auto error = m_activeEditor ? m_activeEditor->validationError() : QString{};
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty());
}

void ThemeAppearanceDialog::updateTransitions()
{
    if(const auto resolved = resolveThemeAppearance(appearance());
       resolved && (!m_generalStyle || m_generalStyle->appearance() != *resolved)) {
        auto style = std::make_unique<FooyinStyle>(*resolved);

        const auto children = m_generalPreview->findChildren<QWidget*>();
        for(auto* widget : children) {
            widget->setStyle(style.get());
        }
        m_generalPreview->setStyle(style.get());
        m_generalStyle = std::move(style);
    }

    const int duration = m_appearance.value("transitionDuration"_L1).toInt(ThemeAppearance{}.transitionDuration);

    const auto children = m_previews->findChildren<QWidget*>();
    for(const auto* widget : children) {
        if(widget->testAttribute(Qt::WA_SetStyle)) {
            if(auto* style = qobject_cast<FooyinStyle*>(widget->style())) {
                style->setTransitionDuration(duration);
            }
        }
    }
}

void ThemeAppearanceDialog::selectState()
{
    if(!m_activeEditor) {
        return;
    }

    QString page{m_activePart.id};
    if(!m_activePart.states.empty()) {
        const int index = m_propertyState->currentIndex();
        if(index < 0) {
            return;
        }

        const auto& state = m_activePart.states.at(index);
        m_selectedState   = state.id;
        page              = state.pageId;
    }

    m_activeEditor->selectPart(page, !m_selectingPreview);
    m_properties->setPage(m_activeEditor->currentPropertyPage());
    updateStateOverrides();
}

void ThemeAppearanceDialog::updateStateOverrides()
{
    if(!m_activeEditor) {
        return;
    }

    const QSignalBlocker blocker{m_propertyState};

    for(int index{0}; index < m_propertyState->count(); ++index) {
        const bool overridden = m_activeEditor->hasGroupOverrides(m_propertyState->itemData(index).toString());

        QFont font{m_propertyState->font()};
        font.setBold(overridden);

        m_propertyState->setItemData(index, font, Qt::FontRole);
        m_propertyState->setItemData(index, overridden ? tr("Contains state overrides") : tr("No state overrides"),
                                     Qt::ToolTipRole);
    }

    const bool stateful = !m_activePart.states.empty();
    m_resetPart->setText(stateful ? tr("Reset state overrides") : tr("Reset part overrides"));

    const auto* page = m_activeEditor->currentPropertyPage();
    m_resetPart->setEnabled(page && page->hasOverrides());
}

void ThemeAppearanceDialog::resetPanelLayout()
{
    for(auto* dock : {m_componentsDock, m_propertiesDock}) {
        dock->setFloating(false);
        m_workspace->removeDockWidget(dock);
    }

    m_workspace->addDockWidget(Qt::LeftDockWidgetArea, m_componentsDock);
    m_workspace->addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    m_componentsDock->show();
    m_propertiesDock->show();

    m_workspace->resizeDocks({m_componentsDock, m_propertiesDock}, {ComponentsDockWidth, PropertiesDockWidth},
                             Qt::Horizontal);
}
} // namespace Fooyin
