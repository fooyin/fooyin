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

#include "inputappearanceeditor.h"

#include "style/fooyinstyle.h"

#include <gui/widgets/hintlabel.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSpinBox>
#include <QStyleOptionComboBox>
#include <QStyleOptionFrame>
#include <QStyleOptionSpinBox>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QFormLayout* formLayout(QWidget* parent)
{
    auto* layout = new QFormLayout(parent);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    return layout;
}

QString inputTitle(InputType type)
{
    switch(type) {
        case InputType::LineEdit:
            return InputAppearanceEditor::tr("Line edit");
        case InputType::ComboBox:
            return InputAppearanceEditor::tr("Combo box");
        case InputType::SpinBox:
            return InputAppearanceEditor::tr("Spin box");
    }
    return {};
}

const InputAppearance& inputAppearance(const ThemeAppearance& appearance, InputType type)
{
    switch(type) {
        case InputType::LineEdit:
            return appearance.lineEdit;
        case InputType::ComboBox:
            return appearance.comboBox;
        case InputType::SpinBox:
            return appearance.spinBox;
    }
    return appearance.lineEdit;
}

void applyInputAppearance(ThemeAppearance& theme, InputType type, const InputAppearance& appearance)
{
    switch(type) {
        case InputType::LineEdit:
            theme.lineEdit = appearance;
            break;
        case InputType::ComboBox:
            theme.comboBox = appearance;
            break;
        case InputType::SpinBox:
            theme.spinBox = appearance;
            break;
    }
}

bool readOnlyInput(const QWidget* widget)
{
    if(const auto* edit = qobject_cast<const QLineEdit*>(widget)) {
        return edit->isReadOnly();
    }
    if(const auto* spin = qobject_cast<const QAbstractSpinBox*>(widget)) {
        return spin->isReadOnly();
    }
    if(const auto* combo = qobject_cast<const QComboBox*>(widget)) {
        return combo->isEditable() && combo->lineEdit() && combo->lineEdit()->isReadOnly();
    }
    return false;
}

class InputPreviewStyle : public FooyinStyle
{
public:
    InputPreviewStyle(InputAppearanceEditor* editor, const ThemeAppearance& appearance)
        : FooyinStyle{appearance}
        , m_editor{editor}
    { }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if((element == PE_PanelLineEdit || element == PE_FrameLineEdit) && m_editor
           && m_editor->previewState() != PreviewState::Normal) {
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                auto preview{*frame};
                forceState(preview);
                FooyinStyle::drawPrimitive(element, &preview, painter, widget);
                return;
            }
        }

        FooyinStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                            const QWidget* widget = nullptr) const override
    {
        if(m_editor && m_editor->previewState() != PreviewState::Normal) {
            if(control == CC_ComboBox) {
                if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                    auto preview{*combo};
                    forceState(preview);
                    preview.state &= ~State_On;
                    preview.activeSubControls
                        = active() ? (m_editor->secondaryPreview() ? SC_ComboBoxArrow : SC_ComboBoxEditField) : SC_None;
                    FooyinStyle::drawComplexControl(control, &preview, painter, widget);
                    return;
                }
            }
            if(control == CC_SpinBox) {
                if(const auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
                    auto preview{*spin};
                    forceState(preview);
                    preview.activeSubControls
                        = active() ? (m_editor->secondaryPreview() ? SC_SpinBoxDown : SC_SpinBoxUp) : SC_None;
                    FooyinStyle::drawComplexControl(control, &preview, painter, widget);
                    return;
                }
            }
        }

        FooyinStyle::drawComplexControl(control, option, painter, widget);
    }

private:
    bool active() const
    {
        const auto state = m_editor->previewState();
        return state == PreviewState::Hovered || state == PreviewState::Pressed;
    }

    void forceState(QStyleOption& option) const
    {
        const auto state = m_editor->previewState();
        option.state &= ~(State_MouseOver | State_Sunken | State_HasFocus);

        if(state == PreviewState::Hovered || state == PreviewState::Pressed) {
            option.state |= State_MouseOver;
        }
        if(state == PreviewState::Pressed) {
            option.state |= State_Sunken;
        }
        if(state == PreviewState::Focused) {
            option.state |= State_HasFocus;
        }
    }

    QPointer<InputAppearanceEditor> m_editor;
};
} // namespace

InputAppearanceEditor::InputAppearanceEditor(AppearancePropertyModel* propertyModel, InputType type)
    : ComponentAppearanceEditor{propertyModel, inputAppearanceKey(type), inputTitle(type)}
    , m_type{type}
    , m_previewStyle{std::make_unique<InputPreviewStyle>(this, ThemeAppearance{})}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
    , m_readOnlySample{nullptr}
    , m_previewArea{type == InputType::SpinBox ? new QComboBox(previewWidget()) : nullptr}
    , m_secondaryPreview{false}
{
    std::vector<AppearancePart> parts{
        {.id = u"frame"_s, .title = tr("Frame"), .parentId = {}},
        {.id       = u"surface"_s,
         .title    = tr("Surface"),
         .parentId = {},
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
            {.id = u"hovered"_s, .title = tr("Hovered"), .pageId = u"hovered"_s, .previewState = PreviewState::Hovered},
            {.id = u"focused"_s, .title = tr("Focused"), .pageId = u"focused"_s, .previewState = PreviewState::Focused},
            {.id           = u"readOnly"_s,
             .title        = tr("Read-only"),
             .pageId       = u"readOnly"_s,
             .previewState = PreviewState::ReadOnly},
            {.id           = u"disabled"_s,
             .title        = tr("Disabled"),
             .pageId       = u"disabled"_s,
             .previewState = PreviewState::Disabled}}},
        {.id = u"focus"_s, .title = tr("Focus outline"), .parentId = {}, .previewState = PreviewState::Focused}};
    if(m_type != InputType::LineEdit) {
        parts.push_back({.id           = u"action"_s,
                         .title        = m_type == InputType::ComboBox ? tr("Dropdown button") : tr("Step buttons"),
                         .parentId     = {},
                         .previewState = PreviewState::Hovered});
        parts.push_back({.id       = u"actionSurface"_s,
                         .title    = tr("Surface"),
                         .parentId = u"action"_s,
                         .states   = {{.id           = u"normal"_s,
                                       .title        = tr("Normal"),
                                       .pageId       = u"actionNormal"_s,
                                       .previewState = PreviewState::Normal},
                                      {.id           = u"hovered"_s,
                                       .title        = tr("Hovered"),
                                       .pageId       = u"actionHovered"_s,
                                       .previewState = PreviewState::Hovered},
                                      {.id           = u"pressed"_s,
                                       .title        = tr("Pressed"),
                                       .pageId       = u"actionPressed"_s,
                                       .previewState = PreviewState::Pressed},
                                      {.id           = u"disabled"_s,
                                       .title        = tr("Disabled"),
                                       .pageId       = u"actionDisabled"_s,
                                       .previewState = PreviewState::Disabled}}});
    }
    setParts(std::move(parts));

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());

    auto* previewLayout = formLayout(m_preview);

    const auto addSample = [this, previewLayout](const QString& label, QWidget* widget) {
        widget->setStyle(m_previewStyle.get());
        widget->installEventFilter(this);

        const auto children = widget->findChildren<QLineEdit*>();
        for(auto* child : children) {
            child->setStyle(m_previewStyle.get());
            child->installEventFilter(this);
        }

        m_samples.push_back(widget);
        previewLayout->addRow(label, widget);
    };

    if(type == InputType::LineEdit) {
        auto* normal = new QLineEdit(tr("Editable text"), m_preview);
        addSample(tr("Text"), normal);
        auto* placeholder = new QLineEdit(m_preview);
        placeholder->setPlaceholderText(tr("Search library…"));
        addSample(tr("Placeholder"), placeholder);
        auto* clear = new QLineEdit(tr("Clearable text"), m_preview);
        clear->setClearButtonEnabled(true);
        addSample(tr("Clear button"), clear);
        auto* readOnly = new QLineEdit(tr("Read-only text"), m_preview);
        readOnly->setReadOnly(true);
        m_readOnlySample = readOnly;
        addSample(tr("Read-only"), readOnly);
    }
    else if(type == InputType::ComboBox) {
        for(int index{0}; index < 4; ++index) {
            auto* combo = new QComboBox(m_preview);
            combo->addItems({tr("First item"), tr("Second item"), tr("Third item")});
            combo->setCurrentIndex(1);

            if(index == 1 || index == 2) {
                combo->setEditable(true);
                combo->setEditText(tr("Editable text"));
                combo->lineEdit()->setReadOnly(index == 2);
                if(index == 2) {
                    m_readOnlySample = combo;
                }
            }
            if(index == 3) {
                combo->setItemIcon(1, m_previewStyle->standardIcon(QStyle::SP_DialogApplyButton));
            }

            const std::array labels{tr("Dropdown"), tr("Editable"), tr("Read-only"), tr("With icon")};
            addSample(labels[index], combo);
        }
    }
    else {
        auto* integer = new QSpinBox(m_preview);
        integer->setRange(0, 100);
        integer->setValue(42);
        addSample(tr("Integer"), integer);
        auto* decimal = new QDoubleSpinBox(m_preview);
        decimal->setRange(-100, 100);
        decimal->setValue(12.5);
        addSample(tr("Decimal"), decimal);
        auto* symbols = new QSpinBox(m_preview);
        symbols->setRange(0, 100);
        symbols->setValue(50);
        symbols->setButtonSymbols(QAbstractSpinBox::PlusMinus);
        addSample(tr("± buttons"), symbols);
        auto* minimum = new QSpinBox(m_preview);
        minimum->setRange(0, 100);
        addSample(tr("At minimum"), minimum);
        auto* readOnly = new QSpinBox(m_preview);
        readOnly->setValue(42);
        readOnly->setReadOnly(true);
        m_readOnlySample = readOnly;
        addSample(tr("Read-only"), readOnly);
        auto* noButtons = new QSpinBox(m_preview);
        noButtons->setValue(42);
        noButtons->setButtonSymbols(QAbstractSpinBox::NoButtons);
        addSample(tr("No buttons"), noButtons);
    }

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});

    if(type == InputType::SpinBox) {
        m_previewArea->addItems({tr("Increment"), tr("Decrement")});

        auto* controls = new QWidget(previewWidget());
        formLayout(controls)->addRow(tr("Active area"), m_previewArea);
        canvasLayout->addWidget(controls);

        QObject::connect(m_previewArea, &QComboBox::currentIndexChanged, this, [this](int index) {
            m_secondaryPreview = index == 1;
            for(auto* widget : m_samples) {
                widget->update();
            }
        });
    }

    canvasLayout->addStretch();
    canvasLayout->addWidget(m_preview);
    canvasLayout->addStretch();

    const auto defaults = defaultInputAppearance(type);

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Minimum height"), {}, u"minimumHeight"_s, defaults.minimumHeight, 0, 512, 0);
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);

    auto* frame = addPage(u"frame"_s);
    addNumber(frame, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);

    static const std::array surfaces{std::pair{u"normal"_s, defaults.normal}, std::pair{u"hovered"_s, defaults.hovered},
                                     std::pair{u"focused"_s, defaults.focused},
                                     std::pair{u"readOnly"_s, defaults.readOnly},
                                     std::pair{u"disabled"_s, defaults.disabled}};

    for(const auto& [name, surface] : surfaces) {
        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);

        addBool(page, tr("Draw background"), name, u"backgroundVisible"_s, surface.backgroundVisible);
        addColour(page, tr("Background colour"), name, u"backgroundColour"_s, surface.backgroundColour, group);

        if(type == InputType::ComboBox) {
            addColour(page, tr("Editable background"), name, u"editableBackgroundColour"_s,
                      surface.editableBackgroundColour, group);
        }

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, surface.borderWidth, 0, 16, 2);
        addColour(page, tr("Border colour"), name, u"borderColour"_s, surface.borderColour, group);
    }

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 16, 2);
    auto* inset    = addNumber(focus, tr("Inset"), u"focus"_s, u"spacing"_s, defaults.focus.spacing, 0, 64, 2);
    inset->toolTip = tr("Extra space inside the outer edge of the control before the focus outline");
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    if(type != InputType::LineEdit) {
        auto* action = addPage(u"action"_s);

        if(type == InputType::ComboBox) {
            addChoice(action, tr("Indicator design"), u"action"_s, u"indicatorDesign"_s, u"single"_s,
                      {{tr("Down arrow"), u"single"_s}, {tr("Up and down arrows"), u"paired"_s}});
        }
        if(type == InputType::SpinBox) {
            const auto layoutName = defaults.action.layout == InputAppearance::Action::Layout::SideBySide
                                      ? u"sideBySide"_s
                                      : u"stacked"_s;
            auto* layout          = addChoice(action, tr("Step button layout"), u"action"_s, u"layout"_s, layoutName,
                                              {{tr("Stacked"), u"stacked"_s}, {tr("Side by side"), u"sideBySide"_s}});
            layout->toolTip = tr("Side-by-side buttons sit at the trailing edge and follow the layout direction.");
        }

        auto* filledArrows
            = addBool(action, tr("Fill arrows"), u"action"_s, u"filledArrows"_s, defaults.action.filledArrows);
        filledArrows->toolTip = tr("Draw arrows as solid triangles. Plus and minus symbols are unchanged.");

        auto* width = addNumber(action, tr("Button width"), u"action"_s, u"width"_s, defaults.action.width, 1, 128, 0);
        if(type == InputType::SpinBox) {
            width->toolTip = tr("Width of each step button");
        }

        addNumber(action, tr("Corner radius"), u"action"_s, u"radius"_s, defaults.action.radius, 0, 128, 2);
        addNumber(action, tr("Inset"), u"action"_s, u"inset"_s, defaults.action.inset, 0, 64, 2);
        addNumber(action, tr("Symbol size"), u"action"_s, u"glyphSize"_s, defaults.action.glyphSize, 0, 64, 2);
        addNumber(action, tr("Symbol thickness"), u"action"_s, u"glyphWidth"_s, defaults.action.glyphWidth, 0, 16, 2);

        static const std::array states{std::pair{u"actionNormal"_s, defaults.actionNormal},
                                       std::pair{u"actionHovered"_s, defaults.actionHovered},
                                       std::pair{u"actionPressed"_s, defaults.actionPressed},
                                       std::pair{u"actionDisabled"_s, defaults.actionDisabled}};

        for(const auto& [name, state] : states) {
            auto* page       = addPage(name);
            const auto group = stateColourGroup(name);

            addColour(page, tr("Background colour"), name, u"backgroundColour"_s, state.backgroundColour, group);

            auto* opacity   = addNumber(page, tr("Background opacity"), name, u"backgroundOpacity"_s,
                                        state.backgroundOpacity, 0, 100, 0);
            opacity->suffix = tr(" %");

            addNumber(page, tr("Border width"), name, u"borderWidth"_s, state.borderWidth, 0, 16, 2);
            addColour(page, tr("Border colour"), name, u"borderColour"_s, state.borderColour, group);
            addColour(page, tr("Symbol colour"), name, u"foregroundColour"_s, state.foregroundColour, group);

            if(type == InputType::ComboBox) {
                addColour(page, tr("Editable symbol colour"), name, u"editableForegroundColour"_s,
                          state.editableForegroundColour, group);
            }
        }
    }

    preparePreview(m_preview);
}

InputAppearanceEditor::~InputAppearanceEditor()
{
    setPreviewStyle(m_preview, nullptr);
}

bool InputAppearanceEditor::secondaryPreview() const
{
    return m_secondaryPreview;
}

void InputAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    if(m_type == InputType::ComboBox) {
        m_secondaryPreview = page.startsWith("action"_L1);
        for(auto* widget : m_samples) {
            widget->update();
        }
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool InputAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() != QEvent::MouseButtonPress) {
        return ComponentAppearanceEditor::eventFilter(watched, event);
    }

    auto* target      = qobject_cast<QWidget*>(watched);
    const auto sample = std::ranges::find_if(
        m_samples, [target](auto* widget) { return target && (widget == target || widget->isAncestorOf(target)); });
    if(sample != m_samples.cend()) {
        auto* widget{*sample};
        const auto position = target->mapTo(widget, static_cast<QMouseEvent*>(event)->position().toPoint());

        bool action{false};
        if(const auto* combo = qobject_cast<QComboBox*>(widget)) {
            QStyleOptionComboBox option;
            option.initFrom(combo);
            option.frame    = combo->hasFrame();
            option.editable = combo->isEditable();
            action          = combo->style()
                                  ->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, combo)
                                  .contains(position);
        }
        else if(const auto* spin = qobject_cast<QAbstractSpinBox*>(widget)) {
            QStyleOptionSpinBox option;
            option.initFrom(spin);
            option.frame         = spin->hasFrame();
            option.buttonSymbols = spin->buttonSymbols();
            const auto up   = spin->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, spin);
            const auto down = spin->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, spin);
            action          = up.contains(position) || down.contains(position);
            if(action) {
                m_previewArea->setCurrentIndex(down.contains(position) ? 1 : 0);
            }
        }

        const auto state = !action && previewState() != PreviewState::Disabled && readOnlyInput(widget)
                             ? PreviewState::ReadOnly
                             : previewState();
        const auto part  = previewPage(action ? u"actionSurface"_s : u"surface"_s, state);
        Q_EMIT partSelected(part);
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void InputAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void InputAppearanceEditor::updatePreview()
{
    const auto resolved = resolveInputAppearance(QJsonObject{{key(), overrides()}}, m_type);
    setValidationError(resolved ? QString{} : tr("%1 appearance: %2").arg(title(), resolved.error()));
    if(!resolved || inputAppearance(m_previewStyle->appearance(), m_type) == *resolved) {
        return;
    }

    ThemeAppearance appearance;
    applyInputAppearance(appearance, m_type, *resolved);

    auto style = std::make_unique<InputPreviewStyle>(this, appearance);
    setPreviewStyle(m_preview, style.get());
    m_previewStyle = std::move(style);
}

void InputAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* widget : m_samples) {
        const bool readOnly = state == PreviewState::ReadOnly || widget == m_readOnlySample;
        if(m_type == InputType::LineEdit) {
            auto* edit = static_cast<QLineEdit*>(widget);
            edit->setReadOnly(readOnly);
        }
        else if(m_type == InputType::ComboBox) {
            if(auto* edit = static_cast<QComboBox*>(widget)->lineEdit()) {
                edit->setReadOnly(readOnly);
            }
        }
        else {
            static_cast<QAbstractSpinBox*>(widget)->setReadOnly(readOnly);
        }

        widget->setEnabled(state != PreviewState::Disabled);
        widget->update();

        const auto children = widget->findChildren<QLineEdit*>();
        for(auto* child : children) {
            child->update();
        }
    }
}
} // namespace Fooyin
