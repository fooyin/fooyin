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

#include "toolbuttonappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/toolbuttonappearance.h"

#include <gui/widgets/hintlabel.h>
#include <gui/widgets/toolbutton.h>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ToolButtonPreview : public ToolButton
{
public:
    explicit ToolButtonPreview(QWidget* parent)
        : ToolButton{parent}
    {
        setFocusPolicy(Qt::StrongFocus);
        setMenuIndicatorHidden(false);
    }

    [[nodiscard]] QString partAt(const QPoint& position) const
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);

        if(popupMode() == QToolButton::MenuButtonPopup
           && style()
                  ->subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, this)
                  .contains(position)) {
            return u"menu"_s;
        }

        return isChecked() ? u"checked"_s : u"normal"_s;
    }

    void setPreviewState(PreviewState state)
    {
        m_state = state;
        setEnabled(state != PreviewState::Disabled);
        update();
    }

    void setMenuActive(bool active)
    {
        m_menuActive = active;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);

        switch(m_state) {
            case PreviewState::Checked:
                option.state |= QStyle::State_On;
                break;
            case PreviewState::Normal:
                option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
                option.activeSubControls = QStyle::SC_None;
                break;
            case PreviewState::Hovered:
                option.activeSubControls = m_menuActive && popupMode() == QToolButton::MenuButtonPopup
                                             ? QStyle::SC_ToolButtonMenu
                                             : QStyle::SC_ToolButton;
                option.state |= QStyle::State_MouseOver;
                break;
            case PreviewState::Pressed:
                option.activeSubControls = m_menuActive && popupMode() == QToolButton::MenuButtonPopup
                                             ? QStyle::SC_ToolButtonMenu
                                             : QStyle::SC_ToolButton;
                option.state |= QStyle::State_MouseOver;
                option.state |= QStyle::State_Sunken;
                break;
            case PreviewState::Focused:
                option.state |= QStyle::State_HasFocus;
                break;
            case PreviewState::DefaultButton:
            case PreviewState::Disabled:
            case PreviewState::ReadOnly:
                break;
        }

        QStylePainter painter{this};
        painter.drawComplexControl(QStyle::CC_ToolButton, option);
    }

private:
    PreviewState m_state{PreviewState::Normal};
    bool m_menuActive{false};
};

ToolButtonAppearanceEditor::ToolButtonAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ButtonAppearanceEditor{propertyModel, u"toolButton"_s, tr("Tool button")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
    , m_buttons{new ToolButtonPreview(m_preview), new ToolButtonPreview(m_preview), new ToolButtonPreview(m_preview),
                new ToolButtonPreview(m_preview), new ToolButtonPreview(m_preview), new ToolButtonPreview(m_preview)}
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
              .previewState = PreviewState::Checked}}},
         {.id = u"focus"_s, .title = tr("Focus"), .parentId = {}, .previewState = PreviewState::Focused},
         {.id = u"menu"_s, .title = tr("Menu"), .parentId = {}, .previewState = PreviewState::Hovered},
         {.id       = u"menuSurface"_s,
          .title    = tr("Surface"),
          .parentId = u"menu"_s,
          .states   = {{.id                = u"normal"_s,
                        .title             = tr("Normal"),
                        .pageId            = u"menu.normal"_s,
                        .previewState      = PreviewState::Normal,
                        .inheritanceGroups = {u"normal"_s}},
                       {.id                = u"hovered"_s,
                        .title             = tr("Hovered"),
                        .pageId            = u"menu.hovered"_s,
                        .previewState      = PreviewState::Hovered,
                        .inheritanceGroups = {u"menu.normal"_s, u"hovered"_s, u"normal"_s}},
                       {.id                = u"pressed"_s,
                        .title             = tr("Pressed"),
                        .pageId            = u"menu.pressed"_s,
                        .previewState      = PreviewState::Pressed,
                        .inheritanceGroups = {u"menu.normal"_s, u"pressed"_s, u"normal"_s}},
                       {.id                = u"disabled"_s,
                        .title             = tr("Disabled"),
                        .pageId            = u"menu.disabled"_s,
                        .previewState      = PreviewState::Disabled,
                        .inheritanceGroups = {u"menu.normal"_s, u"disabled"_s, u"normal"_s}},
                       {.id                = u"checked"_s,
                        .title             = tr("Checked"),
                        .pageId            = u"menu.checked"_s,
                        .previewState      = PreviewState::Checked,
                        .inheritanceGroups = {u"menu.normal"_s, u"checked"_s, u"normal"_s}}}}});

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());

    const std::array labels{tr("Raised"), tr("Flat"), tr("Checked"), tr("Icon only"), tr("Menu"), tr("Split menu")};

    auto* previewLayout = new QGridLayout(m_preview);

    for(size_t index{0}; index < m_buttons.size(); ++index) {
        auto* button = m_buttons[index];
        button->setStyle(m_previewStyle.get());
        button->setAutoRaise(index != 0 && index != 5);
        button->setText(tr("Button"));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(m_previewStyle->standardIcon(QStyle::SP_DialogApplyButton));
        button->installEventFilter(this);

        previewLayout->addWidget(new QLabel(labels[index], m_preview), static_cast<int>(index), 0);
        previewLayout->addWidget(button, static_cast<int>(index), 1, Qt::AlignLeft);
    }

    previewLayout->setColumnStretch(1, 1);

    m_buttons[2]->setCheckable(true);
    m_buttons[2]->setChecked(true);
    m_buttons[3]->setToolButtonStyle(Qt::ToolButtonIconOnly);

    for(auto* button : {m_buttons[4], m_buttons[5]}) {
        auto* menu = new QMenu(button);
        menu->addAction(tr("First action"));
        menu->addAction(tr("Second action"));
        button->setMenu(menu);
    }

    m_buttons[4]->setPopupMode(QToolButton::InstantPopup);
    m_buttons[5]->setPopupMode(QToolButton::MenuButtonPopup);

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addStretch();
    canvasLayout->addWidget(m_preview);
    canvasLayout->addStretch();

    const ToolButtonAppearance defaults;

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Minimum width"), {}, u"minimumWidth"_s, defaults.minimumWidth, 0, 512, 0);
    addNumber(dimensions, tr("Minimum height"), {}, u"minimumHeight"_s, defaults.minimumHeight, 0, 256, 0);
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);

    auto* shape = addPage(u"shape"_s);
    addNumber(shape, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    addBool(shape, tr("Draw background on flat buttons"), {}, u"flatBackground"_s, defaults.flatBackground);
    addBool(shape, tr("Draw border on flat buttons"), {}, u"flatBorder"_s, defaults.flatBorder);

    addSurfacePages(false);

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    auto* spacing    = addNumber(focus, tr("Spacing"), u"focus"_s, u"spacing"_s, defaults.focus.spacing, 0, 64, 2);
    spacing->toolTip = tr("Space between the button border and its focus outline");
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    auto* menu         = addPage(u"menu"_s);
    auto* menuWidth    = addNumber(menu, tr("Menu area width"), u"menu"_s, u"indicatorWidth"_s,
                                   defaults.menu.indicatorWidth, 8, 128, 0);
    menuWidth->toolTip = tr("Width of the separate menu area on split buttons");
    addBool(menu, tr("Show separator"), u"menu"_s, u"separatorVisible"_s, defaults.menu.separatorVisible);
    addNumber(menu, tr("Separator width"), u"menu"_s, u"separatorWidth"_s, defaults.menu.separatorWidth, 0, 32, 2);
    addNumber(menu, tr("Separator inset"), u"menu"_s, u"separatorInset"_s, defaults.menu.separatorInset, 0, 128, 2);
    addColour(menu, tr("Separator colour"), u"menu"_s, u"separatorColour"_s, defaults.menu.separatorColour);

    addSurfacePages(true);

    preparePreview(m_preview);
}

ToolButtonAppearanceEditor::~ToolButtonAppearanceEditor()
{
    setPreviewStyle(m_preview, nullptr);
}

void ToolButtonAppearanceEditor::selectPart(const QString& part, bool updatePreviewState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    for(auto* button : m_buttons) {
        button->setMenuActive(page == u"menu"_s || page.startsWith(u"menu."_s));
    }
    if(updatePreviewState) {
        requestPreviewState(page);
    }
}

bool ToolButtonAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        const auto button
            = std::ranges::find_if(m_buttons, [watched](auto* candidate) { return watched == candidate; });
        if(button != m_buttons.cend()) {
            const bool menu  = (*button)->partAt(static_cast<QMouseEvent*>(event)->position().toPoint()) == u"menu"_s;
            const auto state = previewState() == PreviewState::Normal && (*button)->isChecked() ? PreviewState::Checked
                                                                                                : previewState();
            const auto part  = state == PreviewState::Focused
                                 ? u"focus"_s
                                 : previewPage(menu ? u"menuSurface"_s : u"surface"_s, state);
            Q_EMIT partSelected(part);
        }
    }
    return ButtonAppearanceEditor::eventFilter(watched, event);
}

void ToolButtonAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void ToolButtonAppearanceEditor::updatePreview()
{
    const auto resolved = resolveToolButtonAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Tool button appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().toolButton == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.toolButton = *resolved});
    setPreviewStyle(m_preview, style.get());
    m_previewStyle = std::move(style);
}

void ToolButtonAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* button : m_buttons) {
        button->setPreviewState(state);
    }
}

void ToolButtonAppearanceEditor::addSurfacePages(bool menu)
{
    const ToolButtonAppearance defaults;

    static const std::array surfaceDefaults{
        std::pair{u"normal"_s, defaults.normal}, std::pair{u"hovered"_s, defaults.hovered},
        std::pair{u"pressed"_s, defaults.pressed}, std::pair{u"disabled"_s, defaults.disabled},
        std::pair{u"checked"_s, defaults.checked}};

    for(const auto& [name, surface] : surfaceDefaults) {
        const auto stateName = menu ? u"menu."_s + name : name;

        auto* page = addPage(stateName);
        addButtonSurfaceProperties(page, stateName, surface, menu);
    }
}
} // namespace Fooyin
