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

#include "sliderappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/sliderappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QStylePainter>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
class SliderPreview : public QSlider
{
public:
    SliderPreview(Qt::Orientation orientation, QWidget* parent)
        : QSlider{orientation, parent}
    {
        setRange(0, 100);
        setValue(40);
    }

    [[nodiscard]] QString partAt(const QPoint& position) const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);

        const auto control = style()->hitTestComplexControl(QStyle::CC_Slider, &option, position, this);
        if(control == QStyle::SC_SliderHandle) {
            return u"normal"_s;
        }
        if(control == QStyle::SC_SliderGroove) {
            return u"groove.normal"_s;
        }

        return {};
    }

    void setPreviewState(PreviewState state)
    {
        m_state = state;
        setEnabled(state != PreviewState::Disabled);
        update();
    }

    void setGroovePreview(bool groove)
    {
        m_groovePreview = groove;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionSlider option;
        initStyleOption(&option);

        option.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
        if(tickPosition() != QSlider::NoTicks) {
            option.subControls |= QStyle::SC_SliderTickmarks;
        }

        switch(m_state) {
            case PreviewState::Checked:
                option.state |= QStyle::State_On;
                break;
            case PreviewState::Normal:
                option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
                option.activeSubControls = QStyle::SC_None;
                break;
            case PreviewState::Hovered:
                option.state |= QStyle::State_MouseOver;
                option.activeSubControls = m_groovePreview ? QStyle::SC_SliderGroove : QStyle::SC_SliderHandle;
                break;
            case PreviewState::Pressed:
                option.state |= QStyle::State_Sunken;
                option.state |= QStyle::State_MouseOver;
                option.activeSubControls = m_groovePreview ? QStyle::SC_SliderGroove : QStyle::SC_SliderHandle;
                break;
            case PreviewState::Focused:
                option.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
                break;
            case PreviewState::DefaultButton:
            case PreviewState::Disabled:
            case PreviewState::ReadOnly:
                break;
        }

        QStylePainter painter{this};
        painter.drawComplexControl(QStyle::CC_Slider, option);
    }

private:
    PreviewState m_state{PreviewState::Normal};
    bool m_groovePreview{false};
};

SliderAppearanceEditor::SliderAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"slider"_s, tr("Slider")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
    , m_horizontal{new SliderPreview(Qt::Horizontal, m_preview)}
    , m_vertical{new SliderPreview(Qt::Vertical, m_preview)}
{
    setParts(
        {{.id       = u"groove"_s,
          .title    = tr("Groove"),
          .parentId = {},
          .states   = {{.id           = u"normal"_s,
                        .title        = tr("Normal"),
                        .pageId       = u"groove.normal"_s,
                        .previewState = PreviewState::Normal},
                       {.id           = u"hovered"_s,
                        .title        = tr("Hovered"),
                        .pageId       = u"groove.hovered"_s,
                        .previewState = PreviewState::Hovered},
                       {.id           = u"pressed"_s,
                        .title        = tr("Pressed"),
                        .pageId       = u"groove.pressed"_s,
                        .previewState = PreviewState::Pressed},
                       {.id           = u"disabled"_s,
                        .title        = tr("Disabled"),
                        .pageId       = u"groove.disabled"_s,
                        .previewState = PreviewState::Disabled}}},
         {.id       = u"handle"_s,
          .title    = tr("Handle"),
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
    m_horizontal->setStyle(m_previewStyle.get());
    m_vertical->setStyle(m_previewStyle.get());
    m_vertical->setMinimumHeight(m_preview->fontMetrics().height() * 5);

    auto* horizontalLabel = new QLabel(tr("Horizontal"), m_preview);
    auto* verticalLabel   = new QLabel(tr("Vertical"), m_preview);
    horizontalLabel->setAlignment(Qt::AlignHCenter);
    verticalLabel->setAlignment(Qt::AlignHCenter);

    auto* previewLayout = new QGridLayout(m_preview);
    previewLayout->addWidget(horizontalLabel, 0, 0);
    previewLayout->addWidget(verticalLabel, 0, 1, Qt::AlignHCenter);
    previewLayout->addWidget(m_horizontal, 1, 0);
    previewLayout->addWidget(m_vertical, 1, 1, Qt::AlignHCenter);
    previewLayout->setColumnStretch(0, 1);

    m_horizontal->installEventFilter(this);
    m_vertical->installEventFilter(this);

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addWidget(m_preview);

    const SliderAppearance defaults;

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Interaction size"), {}, u"handleExtent"_s, defaults.handleExtent, 1, 256, 0);
    addNumber(dimensions, tr("Tick offset"), {}, u"tickmarkOffset"_s, defaults.tickmarkOffset, 0, 64, 0);
    dimensions->addHint(new HintLabel(
        tr("The interaction size controls slider spacing and the area used to interact with the slider."), dimensions));

    static const std::array grooveDefaults{std::pair{u"groove.normal"_s, defaults.groove.normal},
                                           std::pair{u"groove.hovered"_s, defaults.groove.hovered},
                                           std::pair{u"groove.pressed"_s, defaults.groove.pressed},
                                           std::pair{u"groove.disabled"_s, defaults.groove.disabled}};

    for(const auto& [name, state] : grooveDefaults) {
        auto* page = addPage(name);

        auto* thickness    = addNumber(page, tr("Thickness"), name, u"thickness"_s, state.thickness, 0, 64, 0);
        thickness->toolTip = tr("Drawn thickness of the groove, limited by the interaction size. "
                                "The interaction area stays the same for every state.");

        addNumber(page, tr("Corner radius"), name, u"radius"_s, state.radius, 0, 128, 2);

        const auto group = stateColourGroup(name);
        addBrush(page, tr("Background"), name, u"backgroundColour"_s, state.backgroundColour, group);
        addBrush(page, tr("Filled background"), name, u"filledColour"_s, state.filledColour, group);
    }

    const std::array handleDefaults{std::pair{u"normal"_s, defaults.normal}, std::pair{u"hovered"_s, defaults.hovered},
                                    std::pair{u"pressed"_s, defaults.pressed},
                                    std::pair{u"disabled"_s, defaults.disabled}};

    for(const auto& [name, state] : handleDefaults) {
        auto* page = addPage(name);

        auto* inset    = addNumber(page, tr("Inset"), name, u"inset"_s, state.inset, 0, 128, 2);
        inset->toolTip = tr("Space between the handle and its interaction area. "
                            "Smaller values draw a larger handle.");

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, state.borderWidth, 0, 32, 2);

        auto* radius   = addNumber(page, tr("Corner radius"), name, u"cornerRadius"_s, state.cornerRadius, 0, 100, 0);
        radius->suffix = tr(" %");

        addBrush(page, tr("Background"), name, u"backgroundColour"_s, state.backgroundColour, stateColourGroup(name));
        addColour(page, tr("Border colour"), name, u"borderColour"_s, state.borderColour, stateColourGroup(name));
    }

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    preparePreview(m_preview);
}

SliderAppearanceEditor::~SliderAppearanceEditor()
{
    setPreviewStyle(m_preview, nullptr);
}

void SliderAppearanceEditor::selectPart(const QString& part, bool updatePreviewState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    m_horizontal->setGroovePreview(page.startsWith(u"groove."_s));
    m_vertical->setGroovePreview(page.startsWith(u"groove."_s));

    if(updatePreviewState) {
        requestPreviewState(page);
    }
}

bool SliderAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress && (watched == m_horizontal || watched == m_vertical)) {
        const auto* mouse  = static_cast<QMouseEvent*>(event);
        const auto* slider = watched == m_horizontal ? m_horizontal : m_vertical;

        auto part = slider->partAt(mouse->position().toPoint());
        if(part == u"normal"_s || part == u"groove.normal"_s) {
            const bool groove = part.startsWith(u"groove."_s);
            part              = previewState() == PreviewState::Focused && !groove
                                  ? u"focus"_s
                                  : previewPage(groove ? u"groove"_s : u"handle"_s, previewState());
        }
        if(!part.isEmpty()) {
            Q_EMIT partSelected(part);
        }
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void SliderAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void SliderAppearanceEditor::updatePreview()
{
    const auto resolved = resolveSliderAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Slider appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->sliderAppearance() == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.slider = *resolved});
    setPreviewStyle(m_preview, style.get());
    m_previewStyle = std::move(style);
}

void SliderAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_horizontal->setPreviewState(state);
    m_vertical->setPreviewState(state);
}
} // namespace Fooyin
