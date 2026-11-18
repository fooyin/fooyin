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

#include "fooyinstyle.h"

#include "buttonstyle.h"
#include "checkstyle.h"
#include "dockstyle.h"
#include "framestyle.h"
#include "headerstyle.h"
#include "inputstyle.h"
#include "itemviewstyle.h"
#include "menustyle.h"
#include "progressstyle.h"
#include "scrollbarstyle.h"
#include "sliderstyle.h"
#include "splitters/fysplitter.h"
#include "splitterstyle.h"
#include "styleutils.h"
#include "tabstyle.h"
#include "toolbarstyle.h"
#include "tooltipstyle.h"

#include <gui/iconloader.h>
#include <gui/widgets/expandedtreeview.h>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QLineEdit>
#include <QMargins>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollBar>
#include <QSlider>
#include <QSplitterHandle>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStyleOptionDockWidget>
#include <QStyleOptionFrame>
#include <QStyleOptionGroupBox>
#include <QStyleOptionHeader>
#include <QStyleOptionMenuItem>
#include <QStyleOptionProgressBar>
#include <QStyleOptionRubberBand>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QStyleOptionTab>
#include <QStyleOptionToolBar>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVariant>

using namespace Qt::StringLiterals;

constexpr auto HoverAttributesProperty = "_fooyin_style_hover_attributes";

namespace {
QMargins itemTextMargins(const QStyleOptionViewItem& option, const QWidget* widget,
                         const Fooyin::ItemViewAppearance& appearance)
{
    if(!widget) {
        widget = option.widget;
    }
    const bool tree = qobject_cast<const QTreeView*>(widget) || qobject_cast<const Fooyin::ExpandedTreeView*>(widget);
    if(!tree) {
        return {appearance.textPadding, 0, appearance.textPadding, 0};
    }

    const bool leading  = option.viewItemPosition == QStyleOptionViewItem::Beginning
                       || option.viewItemPosition == QStyleOptionViewItem::OnlyOne
                       || option.viewItemPosition == QStyleOptionViewItem::Invalid;
    const bool trailing = option.viewItemPosition == QStyleOptionViewItem::End
                       || option.viewItemPosition == QStyleOptionViewItem::OnlyOne
                       || option.viewItemPosition == QStyleOptionViewItem::Invalid;

    const int leadingPadding  = appearance.textPadding + (leading ? appearance.treeTextPadding : 0);
    const int trailingPadding = appearance.textPadding + (trailing ? appearance.treeTextPadding : 0);
    if(option.direction == Qt::RightToLeft) {
        return {trailingPadding, 0, leadingPadding, 0};
    }
    return {leadingPadding, 0, trailingPadding, 0};
}

bool usesHover(const QWidget* widget)
{
    return qobject_cast<const QAbstractButton*>(widget) || qobject_cast<const QComboBox*>(widget)
        || qobject_cast<const QAbstractSpinBox*>(widget) || qobject_cast<const QAbstractSlider*>(widget)
        || qobject_cast<const QProgressBar*>(widget) || qobject_cast<const QSplitterHandle*>(widget)
        || qobject_cast<const Fooyin::FySplitterHandle*>(widget) || qobject_cast<const QTabBar*>(widget)
        || qobject_cast<const QLineEdit*>(widget) || qobject_cast<const QTextEdit*>(widget)
        || qobject_cast<const QPlainTextEdit*>(widget) || qobject_cast<const QGroupBox*>(widget);
}
} // namespace

namespace Fooyin {
FooyinStyle::FooyinStyle()
    : FooyinStyle{ThemeAppearance{}}
{ }

FooyinStyle::FooyinStyle(ThemeAppearance appearance)
    : m_appearance{std::move(appearance)}
    , m_stateAnimation{this, m_appearance.transitionDuration}
    , m_progressAnimation{this}
    , m_scrollBarAnimation{this, m_appearance.scrollBar}
{ }

const SliderAppearance& FooyinStyle::sliderAppearance() const
{
    return m_appearance.slider;
}

const ThemeAppearance& FooyinStyle::appearance() const
{
    return m_appearance;
}

void FooyinStyle::setTransitionDuration(int duration)
{
    m_appearance.transitionDuration = std::clamp(duration, 0, 400);
    m_stateAnimation.setDuration(m_appearance.transitionDuration);
}

QBrush FooyinStyle::animatedBrush(const QStyleOption& option, const QWidget* widget, Style::TransitionChannel channel,
                                  const QBrush& target) const
{
    return m_stateAnimation.brush(option, widget, channel, target);
}

void FooyinStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                                const QWidget* widget) const
{
    switch(element) {
        case PE_IndicatorArrowUp:
        case PE_IndicatorArrowDown:
        case PE_IndicatorArrowLeft:
        case PE_IndicatorArrowRight: {
            const auto arrow = element == PE_IndicatorArrowUp   ? Qt::UpArrow
                             : element == PE_IndicatorArrowDown ? Qt::DownArrow
                             : element == PE_IndicatorArrowLeft ? Qt::LeftArrow
                                                                : Qt::RightArrow;
            Style::drawArrow(*option, *painter, arrow);
            return;
        }
        case PE_PanelTipLabel:
            Style::ToolTip::drawPanel(*option, *painter, m_appearance.toolTip, widget);
            return;
        case PE_PanelToolBar:
            if(const auto* toolbar = qstyleoption_cast<const QStyleOptionToolBar*>(option)) {
                Style::Toolbar::drawPanel(*toolbar, *painter);
                return;
            }
            break;
        case PE_IndicatorToolBarHandle:
            Style::Toolbar::drawHandle(*option, *painter);
            return;
        case PE_IndicatorToolBarSeparator:
            Style::Toolbar::drawSeparator(*option, *painter);
            return;
        case PE_FrameDockWidget:
            Style::Dock::drawFrame(*option, *painter);
            return;
        case PE_PanelButtonTool:
            if(const auto* button = qstyleoption_cast<const QStyleOptionToolButton*>(option);
               button && widget && qobject_cast<const QDockWidget*>(widget->parentWidget())) {
                auto panel{*button};
                panel.subControls = panel.activeSubControls = SC_ToolButton;
                Style::Button::drawToolButton(*this, panel, *painter, widget, m_appearance.toolButton);
                return;
            }
            break;
        case PE_Frame:
            if(widget && widget->inherits("QComboBoxPrivateContainer")) {
                break;
            }
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                const auto* richText  = qobject_cast<const QTextEdit*>(widget);
                const auto* plainText = qobject_cast<const QPlainTextEdit*>(widget);
                if(richText || plainText) {
                    auto input{*frame};
                    input.state.setFlag(State_ReadOnly, richText ? richText->isReadOnly() : plainText->isReadOnly());
                    Style::Input::drawLineEdit(input, *painter, false, m_appearance.lineEdit, widget);
                    return;
                }
                Style::Frame::draw(*frame, *painter);
                return;
            }
            break;
        case PE_FrameGroupBox:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                Style::Frame::drawGroupFrame(*frame, *painter, m_appearance.groupBox);
                return;
            }
            break;
        case PE_IndicatorBranch:
            Style::ItemView::drawBranch(*option, *painter, widget, m_appearance.itemView);
            return;
        case PE_FrameFocusRect:
            // Tree views can omit the widget argument when drawing their current row focus frame
            if((option->state & State_Item) || qobject_cast<const QAbstractItemView*>(widget)
               || qobject_cast<const QAbstractItemView*>(option->styleObject)) {
                Style::ItemView::drawFocus(*option, *painter, m_appearance.itemView);
                return;
            }
            Style::Frame::drawFocus(*option, *painter);
            return;
        case PE_PanelItemViewRow:
        case PE_PanelItemViewItem:
            if(const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                Style::ItemView::drawPanel(*item, *painter, widget, element == PE_PanelItemViewRow,
                                           m_appearance.itemView);
                return;
            }
            break;
        case PE_IndicatorHeaderArrow:
            if(const auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                Style::Header::drawSortIndicator(*header, *painter, m_appearance.header);
                return;
            }
            break;
        case PE_PanelMenu:
            Style::Menu::drawPanel(*option, *painter, widget, m_appearance.menu);
            return;
        case PE_FrameMenu:
            if(qobject_cast<const QToolBar*>(widget)) {
                Style::Toolbar::drawFrame(*option, *painter);
            }
            return;
        case PE_PanelMenuBar:
            Style::Menu::drawMenuBarBackground(*option, *painter, widget, m_appearance.menu);
            return;
        case PE_IndicatorCheckBox:
        case PE_IndicatorRadioButton:
            Style::Check::drawIndicator(
                *option, *painter, element == PE_IndicatorRadioButton,
                element == PE_IndicatorRadioButton ? m_appearance.radioButton : m_appearance.checkBox, widget);
            return;
        case PE_IndicatorItemViewItemCheck: {
            auto indicator{*option};
            // Item views provide their own focus outline around the cell
            indicator.state &= ~State_HasFocus;
            Style::Check::drawIndicator(indicator, *painter, false, m_appearance.checkBox, widget);
            return;
        }
        case PE_PanelLineEdit:
        case PE_FrameLineEdit:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                Style::Input::drawLineEdit(*frame, *painter, element == PE_PanelLineEdit, m_appearance.lineEdit,
                                           widget);
                return;
            }
            break;
        case PE_PanelScrollAreaCorner:
            Style::ScrollBar::drawCorner(m_appearance.scrollBar, *option, *painter);
            return;
        case PE_FrameTabWidget:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option)) {
                Style::Tab::drawPane(*frame, *painter, m_appearance.tab, widget);
                return;
            }
            break;
        case PE_FrameTabBarBase:
            // Flat tabs use spacing to separate the strip from the page
            return;
        case PE_IndicatorTabClose:
            Style::Tab::drawCloseIndicator(*option, *painter, m_appearance.tab);
            return;
        default:
            break;
    }

    QCommonStyle::drawPrimitive(element, option, painter, widget);
}

void FooyinStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                              const QWidget* widget) const
{
    switch(element) {
        case CE_RubberBand:
            if(const auto* band = qstyleoption_cast<const QStyleOptionRubberBand*>(option)) {
                Style::Frame::drawRubberBand(*band, *painter);
                return;
            }
            break;
        case CE_ComboBoxLabel:
            if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                Style::Input::drawComboBoxLabel(*this, *combo, *painter, widget);
                return;
            }
            break;
        case CE_Splitter:
            Style::Splitter::draw(*option, *painter, widget, m_appearance.splitter);
            return;
        case CE_ToolBar:
            if(const auto* toolbar = qstyleoption_cast<const QStyleOptionToolBar*>(option)) {
                Style::Toolbar::drawPanel(*toolbar, *painter);
                return;
            }
            break;
        case CE_DockWidgetTitle:
            if(const auto* dock = qstyleoption_cast<const QStyleOptionDockWidget*>(option)) {
                Style::Dock::drawTitle(*this, *dock, *painter, widget);
                return;
            }
            break;
        case CE_ShapedFrame:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
               frame && (frame->frameShape == QFrame::HLine || frame->frameShape == QFrame::VLine)) {
                Style::Frame::drawSeparator(*frame, *painter);
                return;
            }
            break;
        case CE_MenuEmptyArea:
            // The panel already fills the space around the action rows
            return;
        case CE_MenuBarEmptyArea:
            Style::Menu::drawMenuBarBackground(*option, *painter, widget, m_appearance.menu);
            return;
        case CE_HeaderEmptyArea:
            Style::Header::drawEmptyArea(*option, *painter, widget, m_appearance.header);
            return;
        case CE_MenuScroller:
        case CE_MenuTearoff:
            Style::Menu::drawAuxiliary(element, *option, *painter, widget, m_appearance.menu);
            return;
        case CE_Header:
        case CE_CheckBoxLabel:
        case CE_RadioButtonLabel:
            QCommonStyle::drawControl(element, option, painter, widget);
            return;
        case CE_TabBarTab:
            if(const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                auto padded{*tab};
                padded.rect = Style::Tab::paddedTabRect(*tab, m_appearance.tab);
                if(!padded.rect.isEmpty()) {
                    QCommonStyle::drawControl(element, &padded, painter, widget);
                }
                return;
            }
            break;
        case CE_HeaderSection:
            if(const auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                Style::Header::drawSection(*header, *painter, widget, m_appearance.header);
                return;
            }
            break;
        case CE_HeaderLabel:
            if(const auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                Style::Header::drawLabel(*this, *header, *painter, widget, m_appearance.header);
                return;
            }
            break;
        case CE_MenuItem:
            if(const auto* menu = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                Style::Menu::drawItem(*this, *menu, *painter, widget, m_appearance.menu);
                return;
            }
            break;
        case CE_MenuBarItem:
            if(const auto* menu = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                Style::Menu::drawMenuBarItem(*this, *menu, *painter, widget, m_appearance.menu);
                return;
            }
            break;
        case CE_ProgressBar:
        case CE_ProgressBarGroove:
        case CE_ProgressBarContents:
        case CE_ProgressBarLabel:
            if(const auto* progress = qstyleoption_cast<const QStyleOptionProgressBar*>(option)) {
                Style::Progress::draw(element, *progress, *painter,
                                      m_progressAnimation.phase(*progress, widget, m_appearance.progressBar),
                                      m_appearance.progressBar);
                return;
            }
            break;
        case CE_CheckBox:
        case CE_RadioButton:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                Style::Check::draw(*this, *button, *painter, widget, element == CE_RadioButton);
                return;
            }
            break;
        case CE_PushButton:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                Style::Button::drawPushButton(*this, *button, *painter, widget, m_appearance.pushButton);
                return;
            }
            break;
        case CE_PushButtonBevel:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                Style::Button::drawPushButtonPanel(*this, *button, *painter, widget, m_appearance.pushButton);
                return;
            }
            break;
        case CE_PushButtonLabel:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                Style::Button::drawPushButtonLabel(*this, *button, *painter, widget, m_appearance.pushButton);
                return;
            }
            break;
        case CE_ToolButtonLabel:
            if(const auto* button = qstyleoption_cast<const QStyleOptionToolButton*>(option)) {
                if(widget && widget->inherits("QToolBarExtension")) {
                    Style::Toolbar::drawExtension(*button, *painter, widget);
                }
                else if(widget && qobject_cast<const QDockWidget*>(widget->parentWidget()) && !button->icon.isNull()) {
                    auto label{*button};
                    label.icon
                        = Style::tintedIcon(button->icon, button->iconSize, painter->device()->devicePixelRatioF(),
                                            button->state & State_Enabled ? QIcon::Normal : QIcon::Disabled, QIcon::Off,
                                            button->palette.color(Style::colourGroup(*button), QPalette::WindowText));
                    Style::Button::drawToolButtonLabel(*this, label, *painter, widget, m_appearance.toolButton);
                }
                else {
                    Style::Button::drawToolButtonLabel(*this, *button, *painter, widget, m_appearance.toolButton);
                }
                return;
            }
            break;
        case CE_TabBarTabLabel:
            if(const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                Style::Tab::drawLabel(*this, *tab, *painter, widget, m_appearance.tab);
                return;
            }
            break;
        case CE_TabBarTabShape:
            if(const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                Style::Tab::drawShape(*tab, *painter, widget, m_appearance.tab);
                return;
            }
            break;
        default:
            break;
    }

    QCommonStyle::drawControl(element, option, painter, widget);
}

void FooyinStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                                     const QWidget* widget) const
{
    switch(control) {
        case CC_GroupBox:
            if(const auto* group = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
                Style::Frame::drawGroupBox(*this, *group, *painter, widget, m_appearance.groupBox);
                return;
            }
            break;
        case CC_ComboBox:
            if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                Style::Input::drawComboBox(*this, *combo, *painter, widget, m_appearance.comboBox);
                return;
            }
            break;
        case CC_SpinBox:
            if(const auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
                Style::Input::drawSpinBox(*this, *spin, *painter, widget, m_appearance.spinBox);
                return;
            }
            break;
        case CC_ToolButton:
            if(const auto* button = qstyleoption_cast<const QStyleOptionToolButton*>(option)) {
                Style::Button::drawToolButton(*this, *button, *painter, widget, m_appearance.toolButton);
                return;
            }
            break;
        case CC_ScrollBar:
            if(const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
                const double opacity = m_scrollBarAnimation.opacity(*slider, widget);
                if(opacity > 0.0) {
                    painter->save();
                    painter->setOpacity(painter->opacity() * opacity);
                    Style::ScrollBar::draw(*this, m_appearance.scrollBar, *slider, *painter, widget);
                    painter->restore();
                }
                return;
            }
            break;
        case CC_Slider:
            if(const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
                // Use the shared Qt tick-mark renderer with this style's slider metrics
                if(slider->subControls & SC_SliderTickmarks) {
                    auto ticks{*slider};
                    ticks.subControls = SC_SliderTickmarks;
                    QCommonStyle::drawComplexControl(control, &ticks, painter, widget);
                }
                Style::Slider::draw(*this, m_appearance.slider, *slider, *painter, widget);
                return;
            }
            break;
        default:
            break;
    }

    QCommonStyle::drawComplexControl(control, option, painter, widget);
}

int FooyinStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
    switch(metric) {
        case PM_ScrollView_ScrollBarOverlap:
            return Style::ScrollBar::isTransient(m_appearance.scrollBar, widget) ? m_appearance.scrollBar.width : 0;
        case PM_ToolTipLabelFrameWidth:
            return Style::ToolTip::frameWidth(m_appearance.toolTip);
        case PM_SplitterWidth:
            return m_appearance.splitter.width;
        case PM_LayoutHorizontalSpacing:
            if(m_appearance.layout.horizontalSpacing >= 0) {
                return m_appearance.layout.horizontalSpacing;
            }
            break;
        case PM_LayoutVerticalSpacing:
            if(m_appearance.layout.verticalSpacing >= 0) {
                return m_appearance.layout.verticalSpacing;
            }
            break;
        case PM_LayoutLeftMargin:
            if(m_appearance.layout.leftMargin >= 0) {
                return m_appearance.layout.leftMargin;
            }
            break;
        case PM_LayoutTopMargin:
            if(m_appearance.layout.topMargin >= 0) {
                return m_appearance.layout.topMargin;
            }
            break;
        case PM_LayoutRightMargin:
            if(m_appearance.layout.rightMargin >= 0) {
                return m_appearance.layout.rightMargin;
            }
            break;
        case PM_LayoutBottomMargin:
            if(m_appearance.layout.bottomMargin >= 0) {
                return m_appearance.layout.bottomMargin;
            }
            break;
        default:
            break;
    }
    if(const auto value = Style::Dock::pixelMetric(metric)) {
        return *value;
    }
    if(const auto value = Style::Toolbar::pixelMetric(metric)) {
        return *value;
    }
    if(const auto value = Style::Header::pixelMetric(metric, option, m_appearance.header)) {
        return *value;
    }
    if(const auto value = Style::Menu::pixelMetric(metric, m_appearance.menu)) {
        return *value;
    }
    if(const auto value = Style::Check::pixelMetric(metric, m_appearance.checkBox, m_appearance.radioButton)) {
        return *value;
    }
    if(const auto value = Style::Input::pixelMetric(metric, widget, m_appearance.lineEdit, m_appearance.comboBox,
                                                    m_appearance.spinBox)) {
        return *value;
    }
    if(const auto value = Style::Button::pixelMetric(metric, option, m_appearance.toolButton)) {
        return *value;
    }
    if(const auto value = Style::ScrollBar::pixelMetric(metric, m_appearance.scrollBar)) {
        return *value;
    }
    if(const auto value = Style::Slider::pixelMetric(metric, m_appearance.slider)) {
        return *value;
    }
    if(const auto value = Style::Tab::pixelMetric(metric, m_appearance.tab)) {
        return *value;
    }

    return QCommonStyle::pixelMetric(metric, option, widget);
}

int FooyinStyle::styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                           QStyleHintReturn* returnData) const
{
    switch(hint) {
        case SH_RubberBand_Mask:
            return 0;
        case SH_ComboBox_ListMouseTracking:
        case SH_ItemView_ArrowKeysNavigateIntoChildren:
        case SH_ItemView_ChangeHighlightOnFocus:
            return 1;
        case SH_ComboBox_Popup:
            if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                return !combo->editable;
            }
            return 0;
        case SH_ItemView_ShowDecorationSelected:
            // Keep tree branches on their normal background when the item is selected
            return !qobject_cast<const QTreeView*>(widget);
        default:
            break;
    }

    if(const auto value = Style::Menu::styleHint(hint)) {
        return *value;
    }
    if(const auto value = Style::ScrollBar::styleHint(hint, m_appearance.scrollBar, widget)) {
        return *value;
    }
    if(const auto value = Style::Tab::styleHint(hint, m_appearance.tab)) {
        return *value;
    }

    return QCommonStyle::styleHint(hint, option, widget, returnData);
}

QSize FooyinStyle::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contentsSize,
                                    const QWidget* widget) const
{
    switch(type) {
        case CT_TabWidget:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option)) {
                return Style::Tab::paneSize(*frame, contentsSize, m_appearance.tab);
            }
            break;
        case CT_TabBarTab:
            if(const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                return Style::Tab::sizeFromContents(*tab, contentsSize, m_appearance.tab);
            }
            break;
        case CT_ItemViewItem:
            if(const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                auto content{*item};
                const auto margins = itemTextMargins(*item, widget, m_appearance.itemView);
                const int padding  = margins.left() + margins.right();
                content.rect.setWidth(std::max(0, content.rect.width() - padding));

                auto size = QCommonStyle::sizeFromContents(type, &content, contentsSize, widget);
                size.rwidth() += padding;
                size.rheight() += 2 * m_appearance.itemView.verticalPadding;
                return size;
            }
            break;
        case CT_GroupBox:
            if(const auto* group = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
                return Style::Frame::groupBoxSize(*this, *group, contentsSize, widget, m_appearance.groupBox);
            }
            break;
        case CT_HeaderSection:
            if(const auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                return Style::Header::sizeFromContents(*header, contentsSize, m_appearance.header);
            }
            break;
        case CT_MenuItem:
            if(const auto* menu = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                return Style::Menu::itemSize(*this, *menu, contentsSize, widget, m_appearance.menu);
            }
            break;
        case CT_MenuBarItem:
            if(const auto* menu = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
                return Style::Menu::menuBarItemSize(*menu, contentsSize, m_appearance.menu);
            }
            break;
        case CT_ProgressBar:
            if(const auto* progress = qstyleoption_cast<const QStyleOptionProgressBar*>(option)) {
                return Style::Progress::sizeFromContents(*progress, contentsSize, m_appearance.progressBar);
            }
            break;
        case CT_LineEdit:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                return Style::Input::lineEditSize(*frame, contentsSize, m_appearance.lineEdit, widget);
            }
            break;
        case CT_ComboBox:
            if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                return Style::Input::comboBoxSize(*combo, contentsSize, m_appearance.comboBox);
            }
            break;
        case CT_SpinBox:
            if(const auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
                return Style::Input::spinBoxSize(*spin, contentsSize, m_appearance.spinBox);
            }
            break;
        case CT_PushButton:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                return Style::Button::pushButtonSize(*button, contentsSize, m_appearance.pushButton);
            }
            break;
        case CT_ToolButton:
            if(qstyleoption_cast<const QStyleOptionToolButton*>(option)) {
                return Style::Button::toolButtonSize(contentsSize, m_appearance.toolButton);
            }
            break;
        default:
            break;
    }

    return QCommonStyle::sizeFromContents(type, option, contentsSize, widget);
}

QRect FooyinStyle::subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const
{
    switch(element) {
        case SE_TabBarTabLeftButton:
        case SE_TabBarTabRightButton:
            if(const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                auto padded{*tab};
                padded.rect = Style::Tab::paddedTabRect(*tab, m_appearance.tab);
                if(padded.rect.isEmpty()) {
                    return {};
                }
                return QCommonStyle::subElementRect(element, &padded, widget);
            }
            break;
        case SE_ProgressBarGroove:
        case SE_ProgressBarContents:
        case SE_ProgressBarLabel:
            if(const auto* progress = qstyleoption_cast<const QStyleOptionProgressBar*>(option)) {
                return Style::Progress::subElementRect(element, *progress, m_appearance.progressBar);
            }
            break;
        case SE_TabWidgetTabContents:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionTabWidgetFrame*>(option)) {
                return Style::Tab::paneContentsRect(*this, *frame, widget, m_appearance.tab);
            }
            break;
        case SE_ItemViewItemFocusRect:
            if(const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                return item->rect;
            }
            break;
        case SE_ItemViewItemText:
            if(const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                auto rect          = QCommonStyle::subElementRect(element, option, widget);
                const auto margins = itemTextMargins(*item, widget, m_appearance.itemView);
                const int left     = std::min(margins.left(), std::max(0, rect.width()));
                const int right    = std::min(margins.right(), std::max(0, rect.width() - left));
                rect.adjust(left, 0, -right, 0);
                return rect;
            }
            break;
        case SE_HeaderLabel:
        case SE_HeaderArrow:
            if(const auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                return Style::Header::subElementRect(*header, element, m_appearance.header);
            }
            break;
        case SE_LineEditContents:
            if(const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
                return Style::Input::lineEditContentsRect(*frame, m_appearance.lineEdit, widget);
            }
            break;
        case SE_PushButtonContents:
            if(const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
                return Style::Button::pushButtonContentsRect(*button, m_appearance.pushButton);
            }
            break;
        default:
            break;
    }

    return QCommonStyle::subElementRect(element, option, widget);
}

QRect FooyinStyle::subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl,
                                  const QWidget* widget) const
{
    switch(control) {
        case CC_GroupBox:
            if(const auto* group = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
                return Style::Frame::groupBoxSubControlRect(*this, *group, subControl, widget, m_appearance.groupBox);
            }
            break;
        case CC_ComboBox:
            if(const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
                return Style::Input::comboBoxSubControlRect(*combo, subControl, m_appearance.comboBox);
            }
            break;
        case CC_SpinBox:
            if(const auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
                return Style::Input::spinBoxSubControlRect(*spin, subControl, m_appearance.spinBox);
            }
            break;
        case CC_ToolButton:
            if(const auto* button = qstyleoption_cast<const QStyleOptionToolButton*>(option)) {
                return Style::Button::toolButtonSubControlRect(*this, *button, subControl, widget);
            }
            break;
        case CC_ScrollBar:
            if(const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
                return Style::ScrollBar::subControlRect(*this, *slider, subControl, widget);
            }
            break;
        case CC_Slider:
            if(const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option);
               slider && (subControl == SC_SliderHandle || subControl == SC_SliderGroove)) {
                return Style::Slider::subControlRect(*this, m_appearance.slider, *slider, subControl, widget);
            }
            break;
        default:
            break;
    }

    return QCommonStyle::subControlRect(control, option, subControl, widget);
}

void FooyinStyle::polish(QWidget* widget)
{
    QCommonStyle::polish(widget);
    Style::ItemView::polish(widget);
    Style::ToolTip::polish(widget, m_appearance.toolTip);
    Style::Menu::polish(widget);
    Style::Tab::polish(widget);
    m_scrollBarAnimation.polish(widget);
    m_stateAnimation.polish(widget);

    if((m_appearance.tab.design == TabAppearance::Design::Connected && qobject_cast<QTabBar*>(widget))
       || (m_appearance.tab.strip.backgroundOpacity > 0
           && (qobject_cast<QTabBar*>(widget) || qobject_cast<QTabWidget*>(widget)))) {
        widget->installEventFilter(this);
    }
    if(qobject_cast<QSlider*>(widget)
       && (m_appearance.slider.groove.normal != m_appearance.slider.groove.hovered
           || m_appearance.slider.groove.normal != m_appearance.slider.groove.pressed)) {
        widget->installEventFilter(this);
    }
    if(qobject_cast<QScrollBar*>(widget)
       && (m_appearance.scrollBar.track.normal != m_appearance.scrollBar.track.hovered
           || m_appearance.scrollBar.track.normal != m_appearance.scrollBar.track.pressed)) {
        widget->installEventFilter(this);
    }

    if(usesHover(widget) && !widget->property(HoverAttributesProperty).isValid()) {
        const int attributes
            = (widget->testAttribute(Qt::WA_Hover) ? 1 : 0) | (widget->testAttribute(Qt::WA_OpaquePaintEvent) ? 2 : 0);
        widget->setProperty(HoverAttributesProperty, attributes);
        widget->setAttribute(Qt::WA_Hover, true);
        widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
}

void FooyinStyle::unpolish(QWidget* widget)
{
    widget->removeEventFilter(this);

    Style::Tab::unpolish(widget);
    Style::Header::unpolish(widget);
    Style::Menu::unpolish(widget);
    Style::ToolTip::unpolish(widget);
    Style::ItemView::unpolish(widget);

    m_stateAnimation.remove(widget);
    m_progressAnimation.remove(widget);
    m_scrollBarAnimation.remove(widget);

    if(const auto saved = widget->property(HoverAttributesProperty); saved.isValid()) {
        const int attributes = saved.toInt();
        widget->setAttribute(Qt::WA_Hover, attributes & 1);
        widget->setAttribute(Qt::WA_OpaquePaintEvent, attributes & 2);
        widget->setProperty(HoverAttributesProperty, {});
    }

    QCommonStyle::unpolish(widget);
}

bool FooyinStyle::eventFilter(QObject* watched, QEvent* event)
{
    switch(event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
        case QEvent::HoverLeave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            if(auto* slider = qobject_cast<QAbstractSlider*>(watched)) {
                slider->update();
            }
            break;
        case QEvent::Paint:
            if(auto* widget = qobject_cast<QWidget*>(watched)) {
                Style::Tab::drawStripBackground(widget, m_appearance.tab);
            }
            if(auto* bar = qobject_cast<QTabBar*>(watched)) {
                Style::Tab::updatePane(bar);
            }
            break;
        default:
            break;
    }
    return QCommonStyle::eventFilter(watched, event);
}
} // namespace Fooyin
