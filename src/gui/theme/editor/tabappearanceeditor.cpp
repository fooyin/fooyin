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

#include "tabappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "style/tabstyle.h"
#include "theme/tabappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
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

class TabPreview : public QTabBar
{
public:
    TabPreview(TabAppearanceEditor* editor, QWidget* parent)
        : QTabBar{parent}
        , m_editor{editor}
    {
        setExpanding(false);
        setMovable(true);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if(!m_editor || m_editor->previewState() == PreviewState::Normal) {
            QTabBar::paintEvent(event);
            return;
        }

        QStylePainter painter{this};

        const auto drawTab = [&](int index) {
            if(!isTabVisible(index)) {
                return;
            }

            QStyleOptionTab option;
            initStyleOption(&option, index);

            const auto state = m_editor->previewState();
            option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus
                              | QStyle::State_KeyboardFocusChange);
            const bool active = !m_editor->closePreview()
                             && (m_editor->selectedPreview() ? index == currentIndex()
                                                             : index != currentIndex() && isTabEnabled(index));
            if(active && (option.state & QStyle::State_Enabled)) {
                if(state == PreviewState::Hovered || state == PreviewState::Pressed) {
                    option.state |= QStyle::State_MouseOver;
                }
                if(state == PreviewState::Pressed) {
                    option.state |= QStyle::State_Sunken;
                }
                if(state == PreviewState::Focused) {
                    option.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
                }
            }

            painter.drawControl(QStyle::CE_TabBarTab, option);
        };

        for(int index{0}; index < count(); ++index) {
            if(index != currentIndex()) {
                drawTab(index);
            }
        }
        if(currentIndex() >= 0) {
            drawTab(currentIndex());
        }
    }

private:
    QPointer<TabAppearanceEditor> m_editor;
};

class TabWidgetPreview : public QTabWidget
{
public:
    TabWidgetPreview(TabAppearanceEditor* editor, QWidget* parent)
        : QTabWidget{parent}
    {
        setTabBar(new TabPreview(editor, this));
    }
};

class TabPreviewStyle : public FooyinStyle
{
public:
    TabPreviewStyle(TabAppearanceEditor* editor, const ThemeAppearance& appearance)
        : FooyinStyle{appearance}
        , m_editor{editor}
    { }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if(element == PE_IndicatorTabClose && m_editor && m_editor->previewState() != PreviewState::Normal) {
            auto preview{*option};
            preview.state &= ~(State_MouseOver | State_Sunken);

            const auto state = m_editor->previewState();
            if(m_editor->closePreview() && (state == PreviewState::Hovered || state == PreviewState::Pressed)) {
                preview.state |= State_MouseOver;
            }
            if(m_editor->closePreview() && state == PreviewState::Pressed) {
                preview.state |= State_Sunken;
            }

            FooyinStyle::drawPrimitive(element, &preview, painter, widget);
            return;
        }
        FooyinStyle::drawPrimitive(element, option, painter, widget);
    }

private:
    QPointer<TabAppearanceEditor> m_editor;
};
} // namespace

TabAppearanceEditor::TabAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"tab"_s, tr("Tabs")}
    , m_previewStyle{std::make_unique<TabPreviewStyle>(this, ThemeAppearance{})}
    , m_preview{new QWidget(previewWidget())}
    , m_tabs{new TabWidgetPreview(this, m_preview)}
    , m_bar{new TabPreview(this, m_preview)}
    , m_selectedPreview{true}
    , m_closePreview{false}
{
    setParts(
        {{.id = u"strip"_s, .title = tr("Tab strip"), .parentId = {}},
         {.id = u"pane"_s, .title = tr("Pane border"), .parentId = {}},
         {.id = u"shape"_s, .title = tr("Shape"), .parentId = {}},
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
             {.id           = u"selected"_s,
              .title        = tr("Selected"),
              .pageId       = u"selected"_s,
              .previewState = PreviewState::Normal},
             {.id                = u"selectedHovered"_s,
              .title             = tr("Selected and hovered"),
              .pageId            = u"selectedHovered"_s,
              .previewState      = PreviewState::Hovered,
              .inheritanceGroups = {u"selected"_s, u"normal"_s}},
             {.id           = u"disabled"_s,
              .title        = tr("Disabled"),
              .pageId       = u"disabled"_s,
              .previewState = PreviewState::Disabled},
             {.id                = u"selectedDisabled"_s,
              .title             = tr("Selected and disabled"),
              .pageId            = u"selectedDisabled"_s,
              .previewState      = PreviewState::Disabled,
              .inheritanceGroups = {u"selected"_s, u"normal"_s}}}},
         {.id = u"accent"_s, .title = tr("Selected accent"), .parentId = {}},
         {.id = u"focus"_s, .title = tr("Focus outline"), .parentId = {}, .previewState = PreviewState::Focused},
         {.id = u"close"_s, .title = tr("Close button"), .parentId = {}},
         {.id       = u"closeSurface"_s,
          .title    = tr("Surface"),
          .parentId = u"close"_s,
          .states   = {{.id           = u"normal"_s,
                        .title        = tr("Normal"),
                        .pageId       = u"close.normal"_s,
                        .previewState = PreviewState::Normal},
                       {.id           = u"hovered"_s,
                        .title        = tr("Hovered"),
                        .pageId       = u"close.hovered"_s,
                        .previewState = PreviewState::Hovered},
                       {.id           = u"pressed"_s,
                        .title        = tr("Pressed"),
                        .pageId       = u"close.pressed"_s,
                        .previewState = PreviewState::Pressed},
                       {.id           = u"disabled"_s,
                        .title        = tr("Disabled"),
                        .pageId       = u"close.disabled"_s,
                        .previewState = PreviewState::Disabled}}}});

    m_preview->setAutoFillBackground(true);

    const std::array labels{tr("Main"), tr("Icons"), tr("Disabled")};
    for(const auto& label : labels) {
        auto* page   = new QWidget(m_tabs);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(tr("Content of the selected tab"), page));
        layout->addWidget(new QCheckBox(tr("An option"), page));
        layout->addStretch();
        m_tabs->addTab(page, label);
        m_bar->addTab(label);
    }

    const auto icon = m_previewStyle->standardIcon(QStyle::SP_DialogApplyButton);
    m_tabs->setTabIcon(0, icon);
    m_bar->setTabIcon(0, icon);
    m_tabs->setTabIcon(1, icon);
    m_bar->setTabIcon(1, icon);

    m_tabs->setTabEnabled(2, false);
    m_bar->setTabEnabled(2, false);

    m_tabs->setTabsClosable(true);
    m_bar->setTabsClosable(true);

    m_tabs->tabBar()->installEventFilter(this);
    m_bar->installEventFilter(this);

    auto* previewLayout = new QVBoxLayout(m_preview);
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(m_tabs, 1);

    auto* standalone       = new QWidget(m_preview);
    auto* standaloneLayout = new QVBoxLayout(standalone);
    standaloneLayout->setContentsMargins({});
    standaloneLayout->addWidget(new QLabel(tr("Standalone tab bar"), standalone));
    standaloneLayout->addWidget(m_bar);

    previewLayout->addWidget(standalone);

    auto* position = new QComboBox(previewWidget());
    position->addItems({tr("Top"), tr("Bottom"), tr("Left"), tr("Right")});

    QObject::connect(position, &QComboBox::currentIndexChanged, this, [this, standalone](int index) {
        static constexpr std::array positions{QTabWidget::North, QTabWidget::South, QTabWidget::West, QTabWidget::East};
        static constexpr std::array shapes{QTabBar::RoundedNorth, QTabBar::RoundedSouth, QTabBar::RoundedWest,
                                           QTabBar::RoundedEast};
        m_tabs->setTabPosition(positions[index]);
        m_bar->setShape(shapes[index]);
        standalone->setVisible(index < 2);
        refreshPreview();
    });

    auto* controls       = new QWidget(previewWidget());
    auto* controlsLayout = formLayout(controls);
    controlsLayout->addRow(tr("Position"), position);

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addWidget(controls);
    canvasLayout->addWidget(m_preview, 1);

    const TabAppearance defaults;

    auto* strip = addPage(u"strip"_s);
    addBrush(strip, tr("Background"), u"strip"_s, u"backgroundColour"_s, defaults.strip.backgroundColour);
    auto* stripOpacity   = addNumber(strip, tr("Background opacity"), u"strip"_s, u"backgroundOpacity"_s,
                                     defaults.strip.backgroundOpacity, 0, 100, 0);
    stripOpacity->suffix = tr(" %");
    addNumber(strip, tr("Corner radius"), u"strip"_s, u"radius"_s, defaults.strip.radius, 0, 128, 2);
    addNumber(strip, tr("Leading padding"), u"strip"_s, u"leadingPadding"_s, defaults.strip.leadingPadding, 0, 128, 0);
    addNumber(strip, tr("Trailing padding"), u"strip"_s, u"trailingPadding"_s, defaults.strip.trailingPadding, 0, 128,
              0);
    auto* outerPadding
        = addNumber(strip, tr("Outer padding"), u"strip"_s, u"outerPadding"_s, defaults.strip.outerPadding, 0, 128, 0);
    outerPadding->toolTip = tr("Space between the tabs and the edge away from the page");
    strip->addHint(
        new HintLabel(tr("The background covers the full tab strip, including unused space behind tabs."), strip));

    auto* dimensions = addPage({});
    addChoice(dimensions, tr("Tab design"), {}, u"design"_s,
              defaults.design == TabAppearance::Design::Connected ? u"connected"_s : u"flat"_s,
              {{tr("Flat"), u"flat"_s}, {tr("Connected"), u"connected"_s}});
    const auto alignmentName = defaults.alignment == TabAppearance::Alignment::Centre   ? u"center"_s
                             : defaults.alignment == TabAppearance::Alignment::Trailing ? u"trailing"_s
                                                                                        : u"leading"_s;
    auto* alignment
        = addChoice(dimensions, tr("Tab alignment"), {}, u"alignment"_s, alignmentName,
                    {{tr("Leading"), u"leading"_s}, {tr("Centred"), u"center"_s}, {tr("Trailing"), u"trailing"_s}});
    alignment->toolTip = tr("Position of the tab strip along its edge when spare space is available. "
                            "Tabs configured to expand continue to fill their available space.");
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Minimum thickness"), {}, u"minimumThickness"_s, defaults.minimumThickness, 0, 512, 0);

    auto* shape = addPage(u"shape"_s);
    addNumber(shape, tr("Inset"), {}, u"inset"_s, defaults.inset, 0, 128, 2);
    addNumber(shape, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);

    static const std::array surfaces{std::pair{u"normal"_s, defaults.normal},
                                     std::pair{u"hovered"_s, defaults.hovered},
                                     std::pair{u"pressed"_s, defaults.pressed},
                                     std::pair{u"selected"_s, defaults.selected},
                                     std::pair{u"selectedHovered"_s, defaults.selectedHovered},
                                     std::pair{u"disabled"_s, defaults.disabled},
                                     std::pair{u"selectedDisabled"_s, defaults.selectedDisabled}};

    for(const auto& [name, surface] : surfaces) {
        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);

        addBrush(page, tr("Background"), name, u"backgroundColour"_s, surface.backgroundColour, group);

        auto* opacity   = addNumber(page, tr("Background opacity"), name, u"backgroundOpacity"_s,
                                    surface.backgroundOpacity, 0, 100, 0);
        opacity->suffix = tr(" %");

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, surface.borderWidth, 0, 32, 2);
        addColour(page, tr("Border colour"), name, u"borderColour"_s, surface.borderColour, group);
        addColour(page, tr("Text colour"), name, u"foregroundColour"_s, surface.foregroundColour, group);
        addBool(page, tr("Tint icon"), name, u"tintIcon"_s, surface.tintIcon);
        addColour(page, tr("Icon colour"), name, u"iconColour"_s, surface.iconColour, group);
    }

    auto* accent   = addPage(u"accent"_s);
    auto* width    = addNumber(accent, tr("Width"), u"accent"_s, u"width"_s, defaults.accent.width, 0, 32, 2);
    width->toolTip = tr("Zero hides the selected-tab accent");
    addNumber(accent, tr("End margin"), u"accent"_s, u"margin"_s, defaults.accent.margin, 0, 128, 2);
    addNumber(accent, tr("Inset"), u"accent"_s, u"inset"_s, defaults.accent.inset, 0, 128, 2);
    addChoice(accent, tr("Edge"), u"accent"_s, u"edge"_s,
              defaults.accent.edge == TabAppearance::Accent::Edge::Outer ? u"outer"_s : u"page"_s,
              {{tr("Page edge"), u"page"_s}, {tr("Outer edge"), u"outer"_s}});
    addBool(accent, tr("Rounded ends"), u"accent"_s, u"rounded"_s, defaults.accent.rounded);
    addColour(accent, tr("Colour"), u"accent"_s, u"colour"_s, defaults.accent.colour);

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    addNumber(focus, tr("Inset"), u"focus"_s, u"inset"_s, defaults.focus.inset, 0, 128, 2);
    addNumber(focus, tr("Corner radius"), u"focus"_s, u"radius"_s, defaults.focus.radius, 0, 128, 2);
    addNumber(focus, tr("Page-edge gap"), u"focus"_s, u"pageGap"_s, defaults.focus.pageGap, 0, 128, 2);
    addBool(focus, tr("Keyboard focus only"), u"focus"_s, u"keyboardOnly"_s, defaults.focus.keyboardOnly);
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    auto* close = addPage(u"close"_s);
    addNumber(close, tr("Button size"), u"close"_s, u"extent"_s, defaults.close.extent, 1, 128, 0);
    addNumber(close, tr("Inset"), u"close"_s, u"inset"_s, defaults.close.inset, 0, 64, 2);
    addNumber(close, tr("Corner radius"), u"close"_s, u"radius"_s, defaults.close.radius, 0, 128, 2);
    addNumber(close, tr("Cross size"), u"close"_s, u"glyphSize"_s, defaults.close.glyphSize, 0, 64, 2);
    addNumber(close, tr("Cross thickness"), u"close"_s, u"glyphWidth"_s, defaults.close.glyphWidth, 0, 16, 2);
    close->addHint(new HintLabel(tr("Close buttons appear only on tab bars that enable them."), close));

    static const std::array closeStates{
        std::pair{u"normal"_s, defaults.close.normal}, std::pair{u"hovered"_s, defaults.close.hovered},
        std::pair{u"pressed"_s, defaults.close.pressed}, std::pair{u"disabled"_s, defaults.close.disabled}};

    for(const auto& [name, state] : closeStates) {
        const QString group = u"close."_s + name;
        auto* page          = addPage(group);

        const auto paletteGroup = stateColourGroup(group);
        addBrush(page, tr("Background"), group, u"backgroundColour"_s, state.backgroundColour, paletteGroup);

        auto* opacity   = addNumber(page, tr("Background opacity"), group, u"backgroundOpacity"_s,
                                    state.backgroundOpacity, 0, 100, 0);
        opacity->suffix = tr(" %");

        addColour(page, tr("Cross colour"), group, u"foregroundColour"_s, state.foregroundColour, paletteGroup);
    }

    auto* pane = addPage(u"pane"_s);
    auto* paneWidth
        = addNumber(pane, tr("Border width"), u"pane"_s, u"borderWidth"_s, defaults.pane.borderWidth, 0, 32, 2);
    paneWidth->toolTip = tr("Zero hides the pane border");
    addNumber(pane, tr("Corner radius"), u"pane"_s, u"radius"_s, defaults.pane.radius, 0, 128, 2);
    addColour(pane, tr("Border colour"), u"pane"_s, u"borderColour"_s, defaults.pane.borderColour);
    auto* paneOpacity
        = addNumber(pane, tr("Border opacity"), u"pane"_s, u"borderOpacity"_s, defaults.pane.borderOpacity, 0, 100, 0);
    paneOpacity->suffix = tr(" %");
    addBrush(pane, tr("Background"), u"pane"_s, u"backgroundColour"_s, defaults.pane.backgroundColour);
    auto* backgroundOpacity   = addNumber(pane, tr("Background opacity"), u"pane"_s, u"backgroundOpacity"_s,
                                          defaults.pane.backgroundOpacity, 0, 100, 0);
    backgroundOpacity->suffix = tr(" %");
    const auto reliefStyle    = defaults.pane.reliefStyle == TabAppearance::Pane::ReliefStyle::Sunken  ? u"sunken"_s
                              : defaults.pane.reliefStyle == TabAppearance::Pane::ReliefStyle::Classic ? u"classic"_s
                                                                                                       : u"raised"_s;
    addChoice(pane, tr("Relief style"), u"pane"_s, u"reliefStyle"_s, reliefStyle,
              {{tr("Classic"), u"classic"_s}, {tr("Raised"), u"raised"_s}, {tr("Sunken"), u"sunken"_s}});
    auto* relief
        = addNumber(pane, tr("Relief strength"), u"pane"_s, u"reliefOpacity"_s, defaults.pane.reliefOpacity, 0, 100, 0);
    relief->suffix = tr(" %");
    pane->addHint(new HintLabel(tr("The border surrounds the tab widget’s page area. "
                                   "Connected tabs share the pane’s background and border."),
                                pane));

    preparePreview(m_preview);
}

TabAppearanceEditor::~TabAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

bool TabAppearanceEditor::selectedPreview() const
{
    return m_selectedPreview;
}

bool TabAppearanceEditor::closePreview() const
{
    return m_closePreview;
}

void TabAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    m_selectedPreview = page != u"normal"_s && page != u"hovered"_s && page != u"disabled"_s;
    m_closePreview    = page.startsWith("close"_L1);

    refreshPreview();
    if(updateState) {
        requestPreviewState(page);
    }
}

bool TabAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        const auto* target = qobject_cast<QWidget*>(watched);
        for(const auto* bar : {m_bar, m_tabs->tabBar()}) {
            if(!target || (target != bar && !bar->isAncestorOf(target))) {
                continue;
            }

            for(int index{0}; index < bar->count(); ++index) {
                if(target == bar->tabButton(index, QTabBar::LeftSide)
                   || target == bar->tabButton(index, QTabBar::RightSide)) {
                    const auto state = previewState();
                    Q_EMIT partSelected(!target->isEnabled()                               ? u"close.disabled"_s
                                        : m_closePreview && state == PreviewState::Pressed ? u"close.pressed"_s
                                        : m_closePreview && state == PreviewState::Hovered ? u"close.hovered"_s
                                                                                           : u"close.normal"_s);
                    return ComponentAppearanceEditor::eventFilter(watched, event);
                }
            }

            const int index = bar->tabAt(target->mapTo(bar, static_cast<QMouseEvent*>(event)->position().toPoint()));
            if(index >= 0) {
                const bool selected = index == bar->currentIndex();
                const auto state    = !bar->isEnabled() ? PreviewState::Disabled
                                    : m_closePreview    ? PreviewState::Normal
                                                        : previewState();
                Q_EMIT partSelected(state == PreviewState::Disabled || !bar->isTabEnabled(index)
                                        ? (selected ? u"selectedDisabled"_s : u"disabled"_s)
                                    : state == PreviewState::Pressed ? u"pressed"_s
                                    : state == PreviewState::Hovered ? (selected ? u"selectedHovered"_s : u"hovered"_s)
                                    : selected                       ? u"selected"_s
                                                                     : u"normal"_s);
            }
            else {
                Q_EMIT partSelected(u"strip"_s);
            }

            return ComponentAppearanceEditor::eventFilter(watched, event);
        }

        if(target && (target == m_tabs || m_tabs->isAncestorOf(target))) {
            Q_EMIT partSelected(u"pane"_s);
        }
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void TabAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void TabAppearanceEditor::updatePreview()
{
    const auto resolved = resolveTabAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Tab appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().tab == *resolved) {
        return;
    }

    auto style = std::make_unique<TabPreviewStyle>(this, ThemeAppearance{.tab = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void TabAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_tabs->setEnabled(state != PreviewState::Disabled);
    m_bar->setEnabled(state != PreviewState::Disabled);
    refreshPreview();
}

void TabAppearanceEditor::refreshPreview()
{
    m_tabs->tabBar()->update();
    m_bar->update();

    const auto children = m_preview->findChildren<QWidget*>();
    for(auto* child : children) {
        child->update();
    }
}

void TabAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    m_tabs->setStyle(style);
    m_bar->setStyle(style);

    const auto children = m_preview->findChildren<QWidget*>();
    for(auto* child : children) {
        child->setStyle(style);
    }

    for(auto* bar : {m_tabs->tabBar(), m_bar}) {
        Style::Tab::polish(bar);
        QEvent change{QEvent::StyleChange};
        QCoreApplication::sendEvent(bar, &change);
    }
}
} // namespace Fooyin
