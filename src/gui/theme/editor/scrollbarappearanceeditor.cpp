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

#include "scrollbarappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/scrollbarappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyleOptionSlider>
#include <QStylePainter>
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
} // namespace

class ScrollBarPreview : public QScrollBar
{
public:
    ScrollBarPreview(Qt::Orientation orientation, QWidget* parent)
        : QScrollBar{orientation, parent}
    {
        setRange(0, 100);
        setPageStep(25);
        setValue(40);
        setFocusPolicy(Qt::StrongFocus);
    }

    [[nodiscard]] QString partAt(const QPoint& position) const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);

        const auto thumb = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, this);
        return thumb.contains(position) ? u"normal"_s : u"track.normal"_s;
    }

    void setPreviewState(PreviewState state)
    {
        m_state = state;
        setEnabled(state != PreviewState::Disabled);
        update();
    }

    void setTrackPreview(bool track)
    {
        m_trackPreview = track;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionSlider option;
        initStyleOption(&option);

        option.subControls = QStyle::SC_All;

        if(m_state != PreviewState::Normal) {
            option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
            option.activeSubControls = QStyle::SC_None;
        }
        if(m_state == PreviewState::Hovered || m_state == PreviewState::Pressed) {
            option.state |= QStyle::State_MouseOver;
            option.activeSubControls = m_trackPreview ? QStyle::SC_ScrollBarAddPage : QStyle::SC_ScrollBarSlider;
        }
        if(m_state == PreviewState::Pressed) {
            option.state |= QStyle::State_Sunken;
        }
        if(m_state == PreviewState::Focused) {
            option.state |= QStyle::State_HasFocus;
        }

        QStylePainter painter{this};
        painter.drawComplexControl(QStyle::CC_ScrollBar, option);
    }

private:
    PreviewState m_state{PreviewState::Normal};
    bool m_trackPreview{false};
};

ScrollBarAppearanceEditor::ScrollBarAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"scrollBar"_s, tr("Scroll bar")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
    , m_previewPages{new QStackedWidget(previewWidget())}
    , m_autoHidePreview{new QScrollArea(m_previewPages)}
    , m_previewControls{new QWidget(previewWidget())}
    , m_horizontal{new ScrollBarPreview(Qt::Horizontal, m_preview)}
    , m_vertical{new ScrollBarPreview(Qt::Vertical, m_preview)}
{
    setParts(
        {{.id       = u"track"_s,
          .title    = tr("Track"),
          .parentId = {},
          .states   = {{.id           = u"normal"_s,
                        .title        = tr("Normal"),
                        .pageId       = u"track.normal"_s,
                        .previewState = PreviewState::Normal},
                       {.id           = u"hovered"_s,
                        .title        = tr("Hovered"),
                        .pageId       = u"track.hovered"_s,
                        .previewState = PreviewState::Hovered},
                       {.id           = u"pressed"_s,
                        .title        = tr("Pressed"),
                        .pageId       = u"track.pressed"_s,
                        .previewState = PreviewState::Pressed},
                       {.id           = u"disabled"_s,
                        .title        = tr("Disabled"),
                        .pageId       = u"track.disabled"_s,
                        .previewState = PreviewState::Disabled}}},
         {.id       = u"thumb"_s,
          .title    = tr("Thumb"),
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
         {.id = u"focus"_s, .title = tr("Focus"), .parentId = {}, .previewState = PreviewState::Focused},
         {.id = u"autoHide"_s, .title = tr("Auto-hide"), .parentId = {}}});

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());
    m_horizontal->setStyle(m_previewStyle.get());
    m_vertical->setStyle(m_previewStyle.get());

    m_vertical->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_horizontal->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_vertical->setMinimumHeight(m_preview->fontMetrics().height() * 20);
    m_horizontal->installEventFilter(this);
    m_vertical->installEventFilter(this);

    auto* horizontalLabel = new QLabel(tr("Horizontal"), m_preview);
    auto* verticalLabel   = new QLabel(tr("Vertical"), m_preview);
    horizontalLabel->setAlignment(Qt::AlignHCenter);
    verticalLabel->setAlignment(Qt::AlignHCenter);

    auto* previewLayout = new QGridLayout(m_preview);
    previewLayout->addWidget(horizontalLabel, 0, 0);
    previewLayout->addWidget(verticalLabel, 0, 1);
    previewLayout->addWidget(m_horizontal, 1, 0);
    previewLayout->addWidget(m_vertical, 1, 1, Qt::AlignHCenter);
    previewLayout->setColumnStretch(0, 1);
    previewLayout->setRowStretch(1, 1);

    auto* range = new QSpinBox(previewWidget());
    range->setRange(0, 1000000);
    range->setValue(100);

    auto* pageSize = new QSpinBox(previewWidget());
    pageSize->setRange(0, 1000000);
    pageSize->setValue(25);

    auto* controlsLayout = formLayout(m_previewControls);
    controlsLayout->addRow(tr("Range maximum") + u":"_s, range);
    controlsLayout->addRow(tr("Page size") + u":"_s, pageSize);

    range->setToolTip(tr("A range of zero shows a scroll bar without scrollable content"));
    pageSize->setToolTip(tr("Amount of content visible at once, used to calculate thumb length"));

    QObject::connect(range, &QSpinBox::valueChanged, this, [this](int maximum) {
        m_horizontal->setRange(0, maximum);
        m_vertical->setRange(0, maximum);
    });
    QObject::connect(pageSize, &QSpinBox::valueChanged, this, [this](int page) {
        m_horizontal->setPageStep(page);
        m_vertical->setPageStep(page);
    });

    QString sampleText;
    for(int row{1}; row <= 200; ++row) {
        sampleText += tr("%1. Example track — Example artist").arg(row) + u'\n';
    }

    auto* content = new QLabel(m_autoHidePreview);
    content->setText(sampleText);
    content->setAlignment(Qt::AlignLeading | Qt::AlignTop);
    content->setMargin(12);
    content->setFixedSize(1000, (content->fontMetrics().height() * 200) + 24);

    m_autoHidePreview->setWidget(content);

    m_previewPages->addWidget(m_preview);
    m_previewPages->addWidget(m_autoHidePreview);
    m_previewPages->setCurrentWidget(m_preview);

    applyPreviewStyle(m_previewStyle.get());

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addWidget(m_previewControls);
    canvasLayout->addWidget(m_previewPages, 1);

    const ScrollBarAppearance defaults;

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Width"), {}, u"width"_s, defaults.width, 1, 256, 0);
    addNumber(dimensions, tr("Minimum thumb length"), {}, u"minimumThumbLength"_s, defaults.minimumThumbLength, 1, 512,
              0);

    static const std::array trackDefaults{
        std::pair{u"track.normal"_s, defaults.track.normal}, std::pair{u"track.hovered"_s, defaults.track.hovered},
        std::pair{u"track.pressed"_s, defaults.track.pressed}, std::pair{u"track.disabled"_s, defaults.track.disabled}};

    for(const auto& [name, state] : trackDefaults) {
        auto* page = addPage(name);
        addBool(page, tr("Visible track"), name, u"visible"_s, state.visible);
        addColour(page, tr("Background colour"), name, u"backgroundColour"_s, state.backgroundColour,
                  stateColourGroup(name));
    }

    const std::array thumbDefaults{std::pair{u"normal"_s, defaults.normal}, std::pair{u"hovered"_s, defaults.hovered},
                                   std::pair{u"pressed"_s, defaults.pressed},
                                   std::pair{u"disabled"_s, defaults.disabled}};
    for(const auto& [name, state] : thumbDefaults) {
        auto* page = addPage(name);

        addNumber(page, tr("Side inset"), name, u"sideInset"_s, state.sideInset, 0, 128, 2);
        addNumber(page, tr("End inset"), name, u"endInset"_s, state.endInset, 0, 128, 2);

        auto* radius   = addNumber(page, tr("Corner radius"), name, u"cornerRadius"_s, state.cornerRadius, 0, 100, 0);
        radius->suffix = tr(" %");

        auto* opacity   = addNumber(page, tr("Opacity"), name, u"opacity"_s, state.opacity, 0, 100, 0);
        opacity->suffix = tr(" %");

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, state.borderWidth, 0, 32, 2);
        const auto group = stateColourGroup(name);
        addColour(page, tr("Background colour"), name, u"backgroundColour"_s, state.backgroundColour, group);
        addColour(page, tr("Border colour"), name, u"borderColour"_s, state.borderColour, group);
    }

    auto* focus = addPage(u"focus"_s);
    addNumber(focus, tr("Border width"), u"focus"_s, u"borderWidth"_s, defaults.focus.borderWidth, 0, 32, 2);
    addNumber(focus, tr("Corner radius"), u"focus"_s, u"radius"_s, defaults.focus.radius, 0, 128, 2);
    addColour(focus, tr("Border colour"), u"focus"_s, u"borderColour"_s, defaults.focus.borderColour);

    auto* autoHide = addPage(u"autoHide"_s);
    addBool(autoHide, tr("Auto-hide"), u"autoHide"_s, u"enabled"_s, defaults.autoHide.enabled);
    auto* delay   = addNumber(autoHide, tr("Hide delay"), u"autoHide"_s, u"hideDelay"_s, defaults.autoHide.hideDelay, 0,
                              60000, 0);
    delay->suffix = tr(" ms");
    auto* duration   = addNumber(autoHide, tr("Fade duration"), u"autoHide"_s, u"fadeDuration"_s,
                                 defaults.autoHide.fadeDuration, 0, 10000, 0);
    duration->suffix = tr(" ms");
    autoHide->addHint(new HintLabel(tr("Auto-hidden scroll bars overlay the view and appear when scrolling or hovering "
                                       "their area. Bars set to always show remain visible."),
                                    autoHide));

    preparePreview(m_preview);
    preparePreview(m_autoHidePreview);
}

ScrollBarAppearanceEditor::~ScrollBarAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void ScrollBarAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    m_horizontal->setTrackPreview(page.startsWith(u"track."_s));
    m_vertical->setTrackPreview(page.startsWith(u"track."_s));

    const bool autoHide = page == u"autoHide"_s;
    m_previewPages->setCurrentWidget(autoHide ? static_cast<QWidget*>(m_autoHidePreview) : m_preview);
    m_previewControls->setVisible(!autoHide);

    if(updateState) {
        requestPreviewState(page);
    }
}

bool ScrollBarAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress && (watched == m_horizontal || watched == m_vertical)) {
        const auto* slider = watched == m_horizontal ? m_horizontal : m_vertical;

        auto part = slider->partAt(static_cast<QMouseEvent*>(event)->position().toPoint());
        if(part == u"normal"_s || part == u"track.normal"_s) {
            const bool track = part.startsWith(u"track."_s);
            part             = previewState() == PreviewState::Focused && !track
                                 ? u"focus"_s
                                 : previewPage(track ? u"track"_s : u"thumb"_s, previewState());
        }
        Q_EMIT partSelected(part);
    }

    if(event->type() == QEvent::MouseButtonPress
       && (watched == m_autoHidePreview->viewport() || watched == m_autoHidePreview->widget()
           || watched == m_autoHidePreview->horizontalScrollBar()
           || watched == m_autoHidePreview->verticalScrollBar())) {
        Q_EMIT partSelected(u"autoHide"_s);
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void ScrollBarAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
    m_autoHidePreview->setPalette(palette);
    m_autoHidePreview->setFont(font);
    m_autoHidePreview->widget()->setFixedHeight((m_autoHidePreview->widget()->fontMetrics().height() * 200) + 24);
}

void ScrollBarAppearanceEditor::updatePreview()
{
    const auto resolved = resolveScrollBarAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Scroll bar appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().scrollBar == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.scrollBar = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void ScrollBarAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_horizontal->setPreviewState(state);
    m_vertical->setPreviewState(state);
}

void ScrollBarAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    setPreviewStyle(m_preview, style);
    m_autoHidePreview->setStyle(style);

    const auto children = m_autoHidePreview->findChildren<QWidget*>();
    for(auto* child : children) {
        child->setStyle(style);
    }
}
} // namespace Fooyin
