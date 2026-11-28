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

#include "menuappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/menuappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QActionGroup>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStyleOptionMenuItem>
#include <QStylePainter>
#include <QVBoxLayout>

#include <array>
#include <type_traits>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
template <typename Base>
class MenuPreview : public Base
{
public:
    MenuPreview(MenuAppearanceEditor* editor, QWidget* parent)
        : Base{parent}
        , m_editor{editor}
    {
        if constexpr(std::is_same_v<Base, QMenu>) {
            this->setWindowFlags(Qt::Widget);
        }
        this->setFocusPolicy(Qt::StrongFocus);
        this->setMouseTracking(true);
    }

    void ensureSelection()
    {
        if(!m_highlighted || !m_highlighted->isEnabled() || m_highlighted->isSeparator()) {
            for(auto* action : this->actions()) {
                if(action->isEnabled() && !action->isSeparator()) {
                    m_highlighted = action;
                    break;
                }
            }
        }
        this->update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStylePainter painter{this};

        QStyleOption panel;
        panel.initFrom(this);

        if constexpr(std::is_same_v<Base, QMenu>) {
            painter.drawPrimitive(QStyle::PE_PanelMenu, panel);
        }
        else {
            painter.drawControl(QStyle::CE_MenuBarEmptyArea, panel);
        }

        const auto actions = this->actions();
        for(auto* action : actions) {
            const QRect geometry = this->actionGeometry(action);
            if(!action->isVisible() || geometry.isEmpty()) {
                continue;
            }

            QStyleOptionMenuItem option;
            this->initStyleOption(&option, action);

            option.rect = geometry;

            const auto* highlighted = m_editor && m_editor->previewState() != PreviewState::Normal
                                        ? m_highlighted.data()
                                        : this->activeAction();
            option.state.setFlag(QStyle::State_Selected, action == highlighted);

            painter.drawControl(std::is_same_v<Base, QMenu> ? QStyle::CE_MenuItem : QStyle::CE_MenuBarItem, option);
        }
    }

public:
    void selectAction(QAction* action)
    {
        m_highlighted = action;
        this->update();
    }

private:
    QPointer<MenuAppearanceEditor> m_editor;
    QPointer<QAction> m_highlighted;
};

class MenuPreviewStyle : public FooyinStyle
{
public:
    MenuPreviewStyle(MenuAppearanceEditor* editor, const ThemeAppearance& appearance)
        : FooyinStyle{appearance}
        , m_editor{editor}
    { }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget = nullptr) const override
    {
        if(m_editor && m_editor->previewState() != PreviewState::Normal
           && (element == CE_MenuItem || element == CE_MenuBarItem)) {
            if(const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                auto forced{*item};
                const auto state = m_editor->previewState();
                const bool active
                    = (forced.state & State_Selected) && ((element == CE_MenuBarItem) == m_editor->barPreview());
                forced.state
                    &= ~(State_Selected | State_MouseOver | State_Sunken | State_HasFocus | State_KeyboardFocusChange);
                if(active && (forced.state & State_Enabled) && state != PreviewState::Normal
                   && state != PreviewState::Disabled) {
                    forced.state |= State_Selected | State_MouseOver;
                    if(state == PreviewState::Pressed) {
                        forced.state |= State_Sunken;
                    }
                    if(state == PreviewState::Focused) {
                        forced.state |= State_HasFocus | State_KeyboardFocusChange;
                    }
                }
                FooyinStyle::drawControl(element, &forced, painter, widget);
                return;
            }
        }
        FooyinStyle::drawControl(element, option, painter, widget);
    }

private:
    QPointer<MenuAppearanceEditor> m_editor;
};

void fillMenu(QMenu* menu, const QStyle& style)
{
    auto* open = menu->addAction(style.standardIcon(QStyle::SP_DirOpenIcon), MenuAppearanceEditor::tr("&Open…"));
    open->setShortcut(QKeySequence::Open);
    menu->addSeparator();
    menu->addSection(MenuAppearanceEditor::tr("Playback"));

    auto* play = menu->addAction(style.standardIcon(QStyle::SP_MediaPlay), MenuAppearanceEditor::tr("&Play"));
    menu->setDefaultAction(play);

    auto* shuffle = menu->addAction(MenuAppearanceEditor::tr("Shuffle"));
    shuffle->setCheckable(true);
    shuffle->setChecked(true);

    auto* repeat = new QActionGroup(menu);

    for(const auto& title : {MenuAppearanceEditor::tr("Repeat all"), MenuAppearanceEditor::tr("Repeat one")}) {
        auto* action = menu->addAction(title);
        action->setCheckable(true);
        repeat->addAction(action);
    }

    const auto repeatAction = repeat->actions();
    repeatAction.front()->setChecked(true);

    auto* more = menu->addMenu(MenuAppearanceEditor::tr("More"));
    more->addAction(MenuAppearanceEditor::tr("Next track"));
    more->addAction(MenuAppearanceEditor::tr("Previous track"));
    menu->addAction(MenuAppearanceEditor::tr("Unavailable"))->setEnabled(false);
}
} // namespace

MenuAppearanceEditor::MenuAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"menu"_s, tr("Menus")}
    , m_previewStyle{std::make_unique<MenuPreviewStyle>(this, ThemeAppearance{})}
    , m_preview{new QWidget(previewWidget())}
    , m_menu{new MenuPreview<QMenu>(this, m_preview)}
    , m_bar{new MenuPreview<QMenuBar>(this, m_preview)}
    , m_barPreview{false}
{
    m_preview->setAutoFillBackground(true);
    m_bar->setNativeMenuBar(false);
    m_menu->installEventFilter(this);
    m_bar->installEventFilter(this);

    fillMenu(m_menu, *m_previewStyle);

    auto* file = m_bar->addMenu(tr("&File"));
    fillMenu(file, *m_previewStyle);

    auto* view    = m_bar->addMenu(tr("&View"));
    auto* toolbar = view->addAction(tr("Show toolbar"));

    toolbar->setCheckable(true);
    toolbar->setChecked(true);

    view->addSeparator();
    view->addAction(tr("Layout"));

    m_bar->addAction(tr("&Help"))->setEnabled(false);

    static_cast<MenuPreview<QMenu>*>(m_menu)->ensureSelection();
    static_cast<MenuPreview<QMenuBar>*>(m_bar)->ensureSelection();

    auto* sampleLayout = new QVBoxLayout(m_preview);
    sampleLayout->setContentsMargins({});
    sampleLayout->addWidget(m_bar);

    auto* scroll = new QScrollArea(m_preview);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scroll->setWidget(m_menu);

    sampleLayout->addWidget(scroll, 1);

    auto* previewLayout = new QVBoxLayout(previewWidget());
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(m_preview, 1);

    applyPreviewStyle(m_previewStyle.get());

    const MenuAppearance defaults;

    const auto addSurface
        = [this](AppearancePropertyPage* page, const QString& group, const MenuAppearance::Surface& surface) {
              addBrush(page, tr("Background"), group, u"backgroundColour"_s, surface.backgroundColour);
              auto* opacity   = addNumber(page, tr("Background opacity"), group, u"backgroundOpacity"_s,
                                          surface.backgroundOpacity, 0, 100, 0);
              opacity->suffix = tr(" %");
              addNumber(page, tr("Border width"), group, u"borderWidth"_s, surface.borderWidth, 0, 32, 2);
              addColour(page, tr("Border colour"), group, u"borderColour"_s, surface.borderColour);
              auto* borderOpacity
                  = addNumber(page, tr("Border opacity"), group, u"borderOpacity"_s, surface.borderOpacity, 0, 100, 0);
              borderOpacity->suffix = tr(" %");
              addNumber(page, tr("Corner radius"), group, u"radius"_s, surface.radius, 0, 128, 2);
              auto* relief
                  = addNumber(page, tr("Border shading"), group, u"reliefOpacity"_s, surface.reliefOpacity, 0, 100, 0);
              relief->suffix = tr(" %");
          };

    const auto addState
        = [this](AppearancePropertyPage* page, const QString& group, const MenuAppearance::State& state) {
              const auto colourGroup = stateColourGroup(group);
              addBrush(page, tr("Background"), group, u"backgroundColour"_s, state.backgroundColour, colourGroup);
              auto* opacity   = addNumber(page, tr("Background opacity"), group, u"backgroundOpacity"_s,
                                          state.backgroundOpacity, 0, 100, 0);
              opacity->suffix = tr(" %");
              addNumber(page, tr("Border width"), group, u"borderWidth"_s, state.borderWidth, 0, 32, 2);
              addColour(page, tr("Border colour"), group, u"borderColour"_s, state.borderColour, colourGroup);
              addColour(page, tr("Text colour"), group, u"foregroundColour"_s, state.foregroundColour, colourGroup);
              addBool(page, tr("Tint icon"), group, u"tintIcon"_s, state.tintIcon);
              addColour(page, tr("Icon colour"), group, u"iconColour"_s, state.iconColour, colourGroup);
          };

    auto* dimensions = addPartPage({}, {});
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Minimum width"), {}, u"minimumWidth"_s, defaults.minimumWidth, 0, 512, 0);
    addNumber(dimensions, tr("Minimum height"), {}, u"minimumHeight"_s, defaults.minimumHeight, 0, 512, 0);
    addNumber(dimensions, tr("Horizontal margin"), {}, u"horizontalMargin"_s, defaults.horizontalMargin, 0, 128, 0);
    addNumber(dimensions, tr("Vertical margin"), {}, u"verticalMargin"_s, defaults.verticalMargin, 0, 128, 0);
    addNumber(dimensions, tr("Scroller height"), {}, u"auxiliaryExtent"_s, defaults.auxiliaryExtent, 0, 128, 0);
    addSurface(addPartPage(u"panel"_s, tr("Popup panel")), u"panel"_s, defaults.panel);

    auto* items = addPartPage(u"items"_s, tr("Items"));
    addNumber(items, tr("Inset"), {}, u"inset"_s, defaults.inset, 0, 128, 2);
    addNumber(items, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    addNumber(items, tr("Column spacing"), {}, u"columnSpacing"_s, defaults.columnSpacing, 0, 128, 0);
    addNumber(items, tr("Check column width"), {}, u"checkExtent"_s, defaults.checkExtent, 0, 128, 0);
    addNumber(items, tr("Icon column width"), {}, u"iconExtent"_s, defaults.iconExtent, 0, 128, 0);
    addNumber(items, tr("Icon size"), {}, u"iconSize"_s, defaults.iconSize, 1, 128, 0);
    addNumber(items, tr("Shortcut spacing"), {}, u"shortcutSpacing"_s, defaults.shortcutSpacing, 0, 128, 0);

    addPart(
        {.id       = u"surface"_s,
         .title    = tr("Surface"),
         .parentId = u"items"_s,
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
            {.id           = u"hovered"_s,
             .title        = tr("Hovered"),
             .pageId       = u"selected"_s,
             .previewState = PreviewState::Hovered},
            {.id = u"pressed"_s, .title = tr("Pressed"), .pageId = u"pressed"_s, .previewState = PreviewState::Pressed},
            {.id           = u"disabled"_s,
             .title        = tr("Disabled"),
             .pageId       = u"disabled"_s,
             .previewState = PreviewState::Disabled}}});

    static const std::array popupStates{defaults.normal, defaults.selected, defaults.pressed, defaults.disabled};

    const auto& popupDescriptors = parts().back().states;
    for(size_t index{0}; index < popupDescriptors.size(); ++index) {
        const auto& group = popupDescriptors[index].pageId;
        addState(addPage(group), group, popupStates[index]);
    }

    auto* arrows = addPartPage(u"arrows"_s, tr("Submenu arrows"));
    addNumber(arrows, tr("Column width"), {}, u"arrowExtent"_s, defaults.arrowExtent, 0, 128, 0);
    addNumber(arrows, tr("Arrow size"), {}, u"arrowSize"_s, defaults.arrowSize, 0, 128, 2);
    addNumber(arrows, tr("Line width"), {}, u"arrowWidth"_s, defaults.arrowWidth, 0, 32, 2);

    auto* separator = addPartPage(u"separator"_s, tr("Separators and sections"));
    addNumber(separator, tr("Separator height"), u"separator"_s, u"height"_s, defaults.separator.height, 0, 128, 0);
    addNumber(separator, tr("Section vertical padding"), u"separator"_s, u"sectionPadding"_s,
              defaults.separator.sectionPadding, 0, 128, 0);
    addNumber(separator, tr("Minimum width"), u"separator"_s, u"minimumWidth"_s, defaults.separator.minimumWidth, 0,
              512, 0);
    addNumber(separator, tr("Horizontal inset"), u"separator"_s, u"inset"_s, defaults.separator.inset, 0, 128, 0);
    addNumber(separator, tr("Label spacing"), u"separator"_s, u"spacing"_s, defaults.separator.spacing, 0, 128, 0);
    addNumber(separator, tr("Line width"), u"separator"_s, u"lineWidth"_s, defaults.separator.lineWidth, 0, 32, 2);
    addColour(separator, tr("Line colour"), u"separator"_s, u"lineColour"_s, defaults.separator.lineColour);
    addColour(separator, tr("Text colour"), u"separator"_s, u"foregroundColour"_s, defaults.separator.foregroundColour);
    addBool(separator, tr("Bold section text"), u"separator"_s, u"bold"_s, defaults.separator.bold);

    auto* bar = addPartPage(u"bar"_s, tr("Menu bar"));
    addNumber(bar, tr("Horizontal padding"), u"bar"_s, u"horizontalPadding"_s, defaults.bar.horizontalPadding, 0, 128,
              0);
    addNumber(bar, tr("Vertical padding"), u"bar"_s, u"verticalPadding"_s, defaults.bar.verticalPadding, 0, 128, 0);
    addNumber(bar, tr("Horizontal margin"), u"bar"_s, u"horizontalMargin"_s, defaults.bar.horizontalMargin, 0, 128, 0);
    addNumber(bar, tr("Vertical margin"), u"bar"_s, u"verticalMargin"_s, defaults.bar.verticalMargin, 0, 128, 0);
    addNumber(bar, tr("Item spacing"), u"bar"_s, u"spacing"_s, defaults.bar.spacing, 0, 128, 0);
    addSurface(addPartPage(u"bar.panel"_s, tr("Panel"), u"bar"_s), u"bar.panel"_s, defaults.bar.panel);

    auto* barItems = addPartPage(u"bar.items"_s, tr("Items"), u"bar"_s);
    addNumber(barItems, tr("Inset"), u"bar"_s, u"inset"_s, defaults.bar.inset, 0, 128, 2);
    addNumber(barItems, tr("Corner radius"), u"bar"_s, u"radius"_s, defaults.bar.radius, 0, 128, 2);

    addPart(
        {.id       = u"bar.surface"_s,
         .title    = tr("Surface"),
         .parentId = u"bar.items"_s,
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"bar.normal"_s, .previewState = PreviewState::Normal},
            {.id           = u"hovered"_s,
             .title        = tr("Hovered"),
             .pageId       = u"bar.selected"_s,
             .previewState = PreviewState::Hovered},
            {.id           = u"pressed"_s,
             .title        = tr("Pressed"),
             .pageId       = u"bar.pressed"_s,
             .previewState = PreviewState::Pressed},
            {.id           = u"disabled"_s,
             .title        = tr("Disabled"),
             .pageId       = u"bar.disabled"_s,
             .previewState = PreviewState::Disabled}}});

    static const std::array barStates{defaults.bar.normal, defaults.bar.selected, defaults.bar.pressed,
                                      defaults.bar.disabled};

    const auto& barDescriptors = parts().back().states;
    for(size_t index{0}; index < barDescriptors.size(); ++index) {
        const auto& group = barDescriptors[index].pageId;
        addState(addPage(group), group, barStates[index]);
    }

    auto* focus = addPartPage(u"bar.focus"_s, tr("Focus outline"), u"bar"_s, PreviewState::Focused);
    addNumber(focus, tr("Border width"), u"bar.focus"_s, u"borderWidth"_s, defaults.bar.focus.borderWidth, 0, 32, 2);
    addNumber(focus, tr("Inset"), u"bar.focus"_s, u"inset"_s, defaults.bar.focus.inset, 0, 128, 2);
    addNumber(focus, tr("Corner radius"), u"bar.focus"_s, u"radius"_s, defaults.bar.focus.radius, 0, 128, 2);
    addColour(focus, tr("Border colour"), u"bar.focus"_s, u"borderColour"_s, defaults.bar.focus.borderColour);
    addBool(focus, tr("Keyboard focus only"), u"bar.focus"_s, u"keyboardOnly"_s, defaults.bar.focus.keyboardOnly);

    preparePreview(m_menu);
    preparePreview(m_bar);
}

MenuAppearanceEditor::~MenuAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

bool MenuAppearanceEditor::barPreview() const
{
    return m_barPreview;
}

void MenuAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    m_barPreview = part.startsWith("bar"_L1);
    refreshPreview();

    if(updateState) {
        static_cast<MenuPreview<QMenu>*>(m_menu)->ensureSelection();
        static_cast<MenuPreview<QMenuBar>*>(m_bar)->ensureSelection();
        requestPreviewState(page);
    }
}

bool MenuAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress && (watched == m_menu || watched == m_bar)) {
        auto* action = watched == m_menu ? m_menu->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint())
                                         : m_bar->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint());
        if(action) {
            const bool bar = (watched == m_bar);
            if(bar) {
                static_cast<MenuPreview<QMenuBar>*>(m_bar)->selectAction(action);
            }
            else {
                static_cast<MenuPreview<QMenu>*>(m_menu)->selectAction(action);
            }

            const auto state = !static_cast<QWidget*>(watched)->isEnabled() ? PreviewState::Disabled
                             : bar == m_barPreview                          ? previewState()
                                                                            : PreviewState::Normal;
            const auto part  = action->isSeparator()                                            ? u"separator"_s
                             : !action->isEnabled() || state == PreviewState::Disabled          ? u"disabled"_s
                             : state == PreviewState::Pressed                                   ? u"pressed"_s
                             : state == PreviewState::Hovered || state == PreviewState::Focused ? u"selected"_s
                                                                                                : u"normal"_s;
            Q_EMIT partSelected(bar ? u"bar."_s + part : part);
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void MenuAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);

    const auto children = m_preview->findChildren<QMenu*>();
    for(auto* menu : children) {
        menu->setPalette(palette);
        menu->setFont(font);
    }

    refreshPreview();
}

void MenuAppearanceEditor::updatePreview()
{
    const auto resolved = resolveMenuAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Menu appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().menu == *resolved) {
        return;
    }

    auto style = std::make_unique<MenuPreviewStyle>(this, ThemeAppearance{.menu = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void MenuAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_menu->setEnabled(state != PreviewState::Disabled);
    m_bar->setEnabled(state != PreviewState::Disabled);
    refreshPreview();
}

void MenuAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    m_preview->setStyle(style);

    const auto children = m_preview->findChildren<QWidget*>();
    for(auto* child : children) {
        child->setStyle(style);
    }

    refreshPreview();
}

void MenuAppearanceEditor::refreshPreview()
{
    m_menu->setFixedSize(m_menu->sizeHint());
    m_menu->update();
    m_bar->update();
}
} // namespace Fooyin
