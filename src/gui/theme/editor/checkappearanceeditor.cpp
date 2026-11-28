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

#include "checkappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/checkappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QEvent>
#include <QGroupBox>
#include <QRadioButton>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QVBoxLayout>

#include <array>
#include <type_traits>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
template <typename Button>
class CheckPreview : public Button
{
public:
    CheckPreview(ComponentAppearanceEditor* editor, QWidget* parent)
        : Button{parent}
        , m_editor{editor}
    { }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionButton option;
        this->initStyleOption(&option);

        const auto state = m_editor ? m_editor->previewState() : PreviewState::Normal;

        if(state != PreviewState::Normal) {
            option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
        }
        if(state == PreviewState::Hovered || state == PreviewState::Pressed) {
            option.state |= QStyle::State_MouseOver;
        }
        if(state == PreviewState::Pressed) {
            option.state |= QStyle::State_Sunken;
        }
        if(state == PreviewState::Focused) {
            option.state |= QStyle::State_HasFocus;
        }

        QStylePainter painter{this};
        painter.drawControl(std::is_same_v<Button, QRadioButton> ? QStyle::CE_RadioButton : QStyle::CE_CheckBox,
                            option);
    }

private:
    QPointer<ComponentAppearanceEditor> m_editor;
};
} // namespace

CheckAppearanceEditor::CheckAppearanceEditor(AppearancePropertyModel* propertyModel, bool radio)
    : ComponentAppearanceEditor{propertyModel, radio ? u"radioButton"_s : u"checkBox"_s,
                                radio ? tr("Radio button") : tr("Checkbox")}
    , m_radio{radio}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
{
    setParts(
        {{.id = u"indicator"_s, .title = tr("Indicator"), .parentId = {}},
         {.id = u"mark"_s, .title = m_radio ? tr("Dot") : tr("Checkmark"), .parentId = {}},
         {.id       = u"surface"_s,
          .title    = tr("Surface"),
          .parentId = {},
          .states
          = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
             {.id           = u"hovered"_s,
              .title        = tr("Hovered"),
              .pageId       = u"hovered"_s,
              .previewState = PreviewState::Hovered},
             {.id           = u"pressed"_s,
              .title        = tr("Pressed"),
              .pageId       = u"pressed"_s,
              .previewState = PreviewState::Pressed},
             {.id           = u"disabled"_s,
              .title        = tr("Disabled"),
              .pageId       = u"disabled"_s,
              .previewState = PreviewState::Disabled}}},
         {.id = u"focus"_s, .title = tr("Focus"), .parentId = {}, .previewState = PreviewState::Focused}});

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());

    if(radio) {
        const std::array labels{tr("First option"), tr("Selected option"), tr("Option with a longer label")};
        for(const auto& label : labels) {
            auto* button = new CheckPreview<QRadioButton>(this, m_preview);
            button->setText(label);
            m_buttons.push_back(button);
        }
        m_buttons[1]->setChecked(true);
    }
    else {
        const std::array labels{tr("Unchecked checkbox"), tr("Checked checkbox"), tr("Partially checked"),
                                tr("Checkbox with a longer label")};
        for(const auto& label : labels) {
            auto* button = new CheckPreview<QCheckBox>(this, m_preview);
            button->setText(label);
            m_buttons.push_back(button);
        }
        m_buttons[1]->setChecked(true);

        auto* partial = static_cast<QCheckBox*>(m_buttons[2]);
        partial->setTristate(true);
        partial->setCheckState(Qt::PartiallyChecked);
    }

    auto* previewLayout = new QVBoxLayout(m_preview);

    for(auto* button : m_buttons) {
        button->setStyle(m_previewStyle.get());
        button->installEventFilter(this);
        previewLayout->addWidget(button);
    }

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addStretch();
    canvasLayout->addWidget(m_preview);
    canvasLayout->addStretch();

    const auto defaults = radio ? defaultRadioButtonAppearance() : CheckAppearance{};

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Indicator size"), {}, u"indicatorExtent"_s, defaults.indicatorExtent, 1, 256, 0);
    addNumber(dimensions, tr("Label spacing"), {}, u"labelSpacing"_s, defaults.labelSpacing, 0, 128, 0);

    auto* indicator = addPage(u"indicator"_s);
    addNumber(indicator, tr("Inset"), {}, u"inset"_s, defaults.inset, 0, 128, 2);
    if(!radio) {
        addNumber(indicator, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    }
    indicator->addHint(new HintLabel(tr("The inset leaves space around the indicator body. "
                                        "Borders and focus outlines may require more space."),
                                     indicator));

    auto* mark = addPage(u"mark"_s);
    addNumber(mark, radio ? tr("Dot diameter") : tr("Checkmark size"), {}, u"markSize"_s, defaults.markSize, 0, 128, 2);
    if(!radio) {
        addNumber(mark, tr("Checkmark thickness"), {}, u"markWidth"_s, defaults.markWidth, 0, 32, 2);
        mark->addHint(
            new HintLabel(tr("Partially checked checkboxes use the same colour and thickness for their dash."), mark));
    }
    mark->addHint(new HintLabel(tr("Marks shrink to fit inside the indicator when necessary."), mark));

    static const std::array stateDefaults{
        std::pair{u"normal"_s, defaults.normal}, std::pair{u"hovered"_s, defaults.hovered},
        std::pair{u"pressed"_s, defaults.pressed}, std::pair{u"disabled"_s, defaults.disabled}};

    for(const auto& [name, state] : stateDefaults) {
        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, state.borderWidth, 0, 32, 2);
        addColour(page, tr("Unchecked background"), name, u"backgroundColour"_s, state.backgroundColour, group);
        addColour(page, tr("Checked background"), name, u"checkedBackgroundColour"_s, state.checkedBackgroundColour,
                  group);
        addColour(page, tr("Unchecked border"), name, u"borderColour"_s, state.borderColour, group);
        addColour(page, tr("Checked border"), name, u"checkedBorderColour"_s, state.checkedBorderColour, group);
        addColour(page, radio ? tr("Dot colour") : tr("Checkmark colour"), name, u"markColour"_s, state.markColour,
                  group);
        addColour(page, tr("Unchecked tint"), name, u"tintColour"_s, state.tintColour, group);
        addColour(page, tr("Checked tint"), name, u"checkedTintColour"_s, state.checkedTintColour, group);

        auto* tintOpacity = addNumber(page, tr("Tint opacity"), name, u"tintOpacity"_s, state.tintOpacity, 0, 100, 0);

        tintOpacity->suffix = tr(" %");
        addColour(page, tr("Halo colour"), name, u"haloColour"_s, state.haloColour, group);

        auto* haloOpacity   = addNumber(page, tr("Halo opacity"), name, u"haloOpacity"_s, state.haloOpacity, 0, 100, 0);
        haloOpacity->suffix = tr(" %");
    }

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    addNumber(focus, tr("Spacing"), u"focus"_s, u"spacing"_s, defaults.focus.spacing, 0, 64, 2);
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    preparePreview(m_preview);
}

CheckAppearanceEditor::~CheckAppearanceEditor()
{
    setPreviewStyle(m_preview, nullptr);
}

void CheckAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool CheckAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress
       && std::ranges::any_of(m_buttons, [watched](auto* button) { return watched == button; })) {
        const auto state = previewState();
        const auto part  = state == PreviewState::Focused ? u"focus"_s : previewPage(u"surface"_s, state);
        Q_EMIT partSelected(part);
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void CheckAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void CheckAppearanceEditor::updatePreview()
{
    const auto resolved = m_radio ? resolveRadioButtonAppearance(QJsonObject{{key(), overrides()}})
                                  : resolveCheckBoxAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(
        resolved ? QString{}
                 : (m_radio ? tr("Radio button appearance: %1") : tr("Checkbox appearance: %1")).arg(resolved.error()));
    if(!resolved
       || (m_radio ? m_previewStyle->appearance().radioButton : m_previewStyle->appearance().checkBox) == *resolved) {
        return;
    }

    ThemeAppearance appearance;
    (m_radio ? appearance.radioButton : appearance.checkBox) = *resolved;
    auto style                                               = std::make_unique<FooyinStyle>(appearance);
    setPreviewStyle(m_preview, style.get());
    m_previewStyle = std::move(style);
}

void CheckAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* button : m_buttons) {
        button->setEnabled(state != PreviewState::Disabled);
        button->update();
    }
}
} // namespace Fooyin
