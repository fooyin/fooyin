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

#include "pushbuttonappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/pushbuttonappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QEvent>
#include <QGroupBox>
#include <QMenu>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class PushButtonPreview : public QPushButton
{
public:
    PushButtonPreview(const QString& text, bool defaultButton, QWidget* parent)
        : QPushButton{text, parent}
        , m_defaultButton{defaultButton}
    {
        setAutoDefault(false);
    }

    void setPreviewState(PreviewState state)
    {
        m_state = state;
        setEnabled(state != PreviewState::Disabled);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);

        if(m_defaultButton) {
            option.features |= QStyleOptionButton::DefaultButton;
        }

        switch(m_state) {
            case PreviewState::Checked:
                option.state |= QStyle::State_On;
                break;
            case PreviewState::Normal:
                option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
                break;
            case PreviewState::Hovered:
                option.state |= QStyle::State_MouseOver;
                break;
            case PreviewState::Pressed:
                option.state |= QStyle::State_Sunken;
                option.state |= QStyle::State_MouseOver;
                break;
            case PreviewState::Focused:
                option.state |= QStyle::State_HasFocus;
                break;
            case PreviewState::DefaultButton:
                option.state &= ~QStyle::State_On;
                option.features |= QStyleOptionButton::DefaultButton;
                break;
            case PreviewState::Disabled:
            case PreviewState::ReadOnly:
                break;
        }

        QStylePainter painter{this};
        painter.drawControl(QStyle::CE_PushButton, option);
    }

private:
    PreviewState m_state{PreviewState::Normal};
    bool m_defaultButton;
};

PushButtonAppearanceEditor::PushButtonAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ButtonAppearanceEditor{propertyModel, u"pushButton"_s, tr("Push button")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
    , m_buttons{new PushButtonPreview(tr("Button with a longer label"), false, m_preview),
                new PushButtonPreview(tr("Default button"), true, m_preview),
                new PushButtonPreview(tr("Checkable button"), false, m_preview),
                new PushButtonPreview(tr("Button with an icon"), false, m_preview),
                new PushButtonPreview(tr("Flat button"), false, m_preview),
                new PushButtonPreview(tr("Button with a menu"), false, m_preview)}
{
    setParts(
        {{.id = u"shape"_s, .title = tr("Shape"), .parentId = {}},
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
              .previewState = PreviewState::Disabled},
             {.id           = u"checked"_s,
              .title        = tr("Checked"),
              .pageId       = u"checked"_s,
              .previewState = PreviewState::Checked},
             {.id           = u"defaultButton"_s,
              .title        = tr("Default"),
              .pageId       = u"defaultButton"_s,
              .previewState = PreviewState::DefaultButton}}},
         {.id = u"focus"_s, .title = tr("Focus"), .parentId = {}, .previewState = PreviewState::Focused}});

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());

    m_buttons[2]->setCheckable(true);
    m_buttons[2]->setChecked(true);
    m_buttons[3]->setIcon(m_previewStyle->standardIcon(QStyle::SP_DialogApplyButton));
    m_buttons[4]->setFlat(true);

    auto* menu = new QMenu(m_buttons[5]);
    menu->addAction(tr("First action"));
    menu->addAction(tr("Second action"));
    m_buttons[5]->setMenu(menu);

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

    const PushButtonAppearance defaults;

    auto* dimensions = addPage({});
    auto* minimumWidth
        = addNumber(dimensions, tr("Minimum width"), {}, u"minimumWidth"_s, defaults.minimumWidth, 0, 512, 0);
    minimumWidth->toolTip = tr("Minimum width for buttons with text");
    addNumber(dimensions, tr("Minimum height"), {}, u"minimumHeight"_s, defaults.minimumHeight, 0, 256, 0);
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);

    auto* shape = addPage(u"shape"_s);
    addNumber(shape, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);

    static const std::array surfaceDefaults{
        std::pair{u"normal"_s, defaults.normal},   std::pair{u"hovered"_s, defaults.hovered},
        std::pair{u"pressed"_s, defaults.pressed}, std::pair{u"disabled"_s, defaults.disabled},
        std::pair{u"checked"_s, defaults.checked}, std::pair{u"defaultButton"_s, defaults.defaultButton}};

    for(const auto& [name, surface] : surfaceDefaults) {
        auto* page = addPage(name);
        addButtonSurfaceProperties(page, name, surface);
    }

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    auto* spacing    = addNumber(focus, tr("Spacing"), u"focus"_s, u"spacing"_s, defaults.focus.spacing, 0, 64, 2);
    spacing->toolTip = tr("Space between the button border and its focus outline");
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    preparePreview(m_preview);
}

PushButtonAppearanceEditor::~PushButtonAppearanceEditor()
{
    setPreviewStyle(m_preview, nullptr);
}

void PushButtonAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool PushButtonAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    const auto button = std::ranges::find_if(m_buttons, [watched](auto* candidate) { return watched == candidate; });
    if(event->type() == QEvent::MouseButtonPress && button != m_buttons.cend()) {
        const auto state = previewState() != PreviewState::Normal ? previewState()
                         : button == m_buttons.begin() + 1        ? PreviewState::DefaultButton
                         : (*button)->isChecked()                 ? PreviewState::Checked
                                                                  : PreviewState::Normal;
        const auto part  = state == PreviewState::Focused ? u"focus"_s : previewPage(u"surface"_s, state);
        Q_EMIT partSelected(part);
    }
    return ButtonAppearanceEditor::eventFilter(watched, event);
}

void PushButtonAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void PushButtonAppearanceEditor::updatePreview()
{
    const auto resolved = resolvePushButtonAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Push button appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().pushButton == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.pushButton = *resolved});
    setPreviewStyle(m_preview, style.get());
    m_previewStyle = std::move(style);
}

void PushButtonAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* button : m_buttons) {
        button->setPreviewState(state);
    }
}
} // namespace Fooyin
