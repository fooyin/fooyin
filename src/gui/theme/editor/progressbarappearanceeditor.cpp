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

#include "progressbarappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/progressbarappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QStyleOptionProgressBar>
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

class ProgressPreview : public QProgressBar
{
public:
    ProgressPreview(ComponentAppearanceEditor* editor, QWidget* parent)
        : QProgressBar{parent}
        , m_editor{editor}
    { }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if(!m_editor) {
            QProgressBar::paintEvent(event);
            return;
        }

        QStyleOptionProgressBar option;
        initStyleOption(&option);

        option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);

        QStylePainter painter{this};
        painter.drawControl(QStyle::CE_ProgressBar, option);
    }

private:
    QPointer<ComponentAppearanceEditor> m_editor;
};
} // namespace

ProgressBarAppearanceEditor::ProgressBarAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"progressBar"_s, tr("Progress bar")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QGroupBox(tr("Preview"), previewWidget())}
{
    setParts(
        {{.id = u"track"_s, .title = tr("Track"), .parentId = {}},
         {.id = u"fill"_s, .title = tr("Fill"), .parentId = {}},
         {.id       = u"colours"_s,
          .title    = tr("Colours"),
          .parentId = {},
          .states
          = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
             {.id           = u"disabled"_s,
              .title        = tr("Disabled"),
              .pageId       = u"disabled"_s,
              .previewState = PreviewState::Disabled}}},
         {.id = u"text"_s, .title = tr("Text"), .parentId = {}},
         {.id = u"busy"_s, .title = tr("Indeterminate"), .parentId = {}}});

    m_preview->setAutoFillBackground(true);
    m_preview->setStyle(m_previewStyle.get());

    const auto addBar = [this](Qt::Orientation orientation, int value, bool text, bool busy) {
        auto* bar = new ProgressPreview(this, m_preview);

        bar->setOrientation(orientation);
        bar->setRange(0, busy ? 0 : 100);
        if(!busy) {
            bar->setValue(value);
        }
        bar->setTextVisible(text);
        bar->setAlignment(Qt::AlignCenter);
        bar->setStyle(m_previewStyle.get());
        bar->installEventFilter(this);

        m_bars.push_back(bar);
        return bar;
    };

    auto* previewLayout = new QGridLayout(m_preview);

    const std::array labels{tr("Empty"), tr("Progress"), tr("Complete"), tr("Compact"), tr("Indeterminate")};
    static constexpr std::array values{0, 45, 100, 45, 0};

    for(size_t index{0}; index < labels.size(); ++index) {
        auto* bar = addBar(Qt::Horizontal, values[index], index != 3, index == 4);
        previewLayout->addWidget(new QLabel(labels[index], m_preview), static_cast<int>(index), 0);
        previewLayout->addWidget(bar, static_cast<int>(index), 1);
    }

    auto* vertical = addBar(Qt::Vertical, 45, true, false);
    vertical->setMinimumHeight(m_preview->fontMetrics().height() * 8);
    previewLayout->addWidget(vertical, 0, 2, 5, 1);

    auto* verticalBusy = addBar(Qt::Vertical, 0, false, true);
    previewLayout->addWidget(verticalBusy, 0, 3, 5, 1);
    previewLayout->setColumnStretch(1, 1);

    auto* value = new QSpinBox(previewWidget());
    value->setRange(0, 100);
    value->setSuffix(tr(" %"));
    value->setValue(45);

    auto* inverted    = new QCheckBox(previewWidget());
    auto* bottomToTop = new QCheckBox(previewWidget());

    QObject::connect(value, &QSpinBox::valueChanged, this, [this](int progress) {
        for(auto* bar : {m_bars[1], m_bars[3], m_bars[5]}) {
            bar->setValue(progress);
        }
    });
    QObject::connect(inverted, &QCheckBox::toggled, this, [this](bool reverse) {
        for(auto* bar : m_bars) {
            bar->setInvertedAppearance(reverse);
        }
    });
    QObject::connect(bottomToTop, &QCheckBox::toggled, this, [this](bool reverse) {
        for(auto* bar : m_bars) {
            bar->setTextDirection(reverse ? QProgressBar::BottomToTop : QProgressBar::TopToBottom);
        }
    });

    auto* controls       = new QWidget(previewWidget());
    auto* controlsLayout = formLayout(controls);
    controlsLayout->addRow(tr("Progress") + u":"_s, value);
    controlsLayout->addRow(tr("Invert fill direction") + u":"_s, inverted);
    controlsLayout->addRow(tr("Vertical text bottom to top") + u":"_s, bottomToTop);

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addWidget(controls);
    canvasLayout->addStretch();
    canvasLayout->addWidget(m_preview);
    canvasLayout->addStretch();

    const ProgressBarAppearance defaults;

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Minimum thickness"), {}, u"minimumThickness"_s, defaults.minimumThickness, 0, 512, 0);

    auto* track = addPage(u"track"_s);
    addNumber(track, tr("Inset"), u"track"_s, u"inset"_s, defaults.track.inset, 0, 128, 2);
    addNumber(track, tr("Corner radius"), u"track"_s, u"radius"_s, defaults.track.radius, 0, 128, 2);
    addNumber(track, tr("Border width"), u"track"_s, u"borderWidth"_s, defaults.track.borderWidth, 0, 32, 2);
    addNumber(track, tr("Compact thickness"), u"track"_s, u"compactThickness"_s, defaults.track.compactThickness, 1,
              256, 0);
    addNumber(track, tr("Compact corner radius"), u"track"_s, u"compactRadius"_s, defaults.track.compactRadius, 0, 128,
              2);
    addNumber(track, tr("Compact border width"), u"track"_s, u"compactBorderWidth"_s, defaults.track.compactBorderWidth,
              0, 32, 2);
    track->addHint(new HintLabel(tr("Compact settings apply when text is hidden."), track));

    auto* fill = addPage(u"fill"_s);
    addNumber(fill, tr("Inset"), u"fill"_s, u"inset"_s, defaults.fill.inset, 0, 128, 2);
    addNumber(fill, tr("Corner radius"), u"fill"_s, u"radius"_s, defaults.fill.radius, 0, 128, 2);
    addNumber(fill, tr("Compact corner radius"), u"fill"_s, u"compactRadius"_s, defaults.fill.compactRadius, 0, 128, 2);
    auto* segmented    = addBool(fill, tr("Segmented fill"), u"fill"_s, u"segmented"_s, defaults.fill.segmented);
    segmented->toolTip = tr("Divide the fill into segments, with a partially filled segment at the current value.");
    addNumber(fill, tr("Segment length"), u"fill"_s, u"segmentLength"_s, defaults.fill.segmentLength, 1, 512, 0);
    addNumber(fill, tr("Segment spacing"), u"fill"_s, u"segmentSpacing"_s, defaults.fill.segmentSpacing, 0, 128, 0);

    static const std::array states{std::pair{u"normal"_s, defaults.normal},
                                   std::pair{u"disabled"_s, defaults.disabled}};

    for(const auto& [name, colours] : states) {
        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);

        addBrush(page, tr("Track background"), name, u"trackColour"_s, colours.trackColour, group);
        addBrush(page, tr("Compact track background"), name, u"compactTrackColour"_s, colours.compactTrackColour,
                 group);
        addColour(page, tr("Border colour"), name, u"borderColour"_s, colours.borderColour, group);
        addBrush(page, tr("Fill"), name, u"fillColour"_s, colours.fillColour, group);
        addColour(page, tr("Text colour"), name, u"textColour"_s, colours.textColour, group);
        addColour(page, tr("Filled text colour"), name, u"filledTextColour"_s, colours.filledTextColour, group);
    }

    auto* text = addPage(u"text"_s);
    addBool(text, tr("Show text"), u"text"_s, u"visible"_s, defaults.text.visible);

    const auto positionName = defaults.text.position == ProgressBarAppearance::Text::Position::Left  ? u"left"_s
                            : defaults.text.position == ProgressBarAppearance::Text::Position::Right ? u"right"_s
                                                                                                     : u"inside"_s;
    auto* position
        = addChoice(text, tr("Position"), u"text"_s, u"position"_s, positionName,
                    {{tr("Inside"), u"inside"_s}, {tr("Outside left"), u"left"_s}, {tr("Outside right"), u"right"_s}});
    position->toolTip = tr("Outside positions stay on the left or right, including on vertical bars.");

    const auto alignmentName = defaults.text.alignment == ProgressBarAppearance::Text::Alignment::Leading ? u"leading"_s
                             : defaults.text.alignment == ProgressBarAppearance::Text::Alignment::Centre  ? u"center"_s
                             : defaults.text.alignment == ProgressBarAppearance::Text::Alignment::Trailing
                                 ? u"trailing"_s
                                 : u"widget"_s;
    auto* alignment          = addChoice(text, tr("Alignment"), u"text"_s, u"alignment"_s, alignmentName,
                                         {{tr("Widget default"), u"widget"_s},
                                          {tr("Leading"), u"leading"_s},
                                          {tr("Centre"), u"center"_s},
                                          {tr("Trailing"), u"trailing"_s}});
    alignment->toolTip       = tr("Alignment follows the text direction, including on vertical bars.");

    addNumber(text, tr("Padding"), u"text"_s, u"padding"_s, defaults.text.padding, 0, 128, 0);
    auto* spacing = addNumber(text, tr("Outside spacing"), u"text"_s, u"spacing"_s, defaults.text.spacing, 0, 128, 0);
    spacing->toolTip = tr("Space between outside text and the bar");
    text->addHint(new HintLabel(tr("Outside text uses the compact track settings."), text));

    auto* busy = addPage(u"busy"_s);
    addBool(busy, tr("Animate"), u"busy"_s, u"animated"_s, defaults.busy.animated);
    auto* duration
        = addNumber(busy, tr("Cycle duration"), u"busy"_s, u"duration"_s, defaults.busy.duration, 100, 60000, 0);
    duration->suffix = tr(" ms");
    auto* length
        = addNumber(busy, tr("Fill length"), u"busy"_s, u"segmentSize"_s, defaults.busy.segmentSize, 1, 100, 0);
    length->suffix = tr(" %");

    preparePreview(m_preview);
}

ProgressBarAppearanceEditor::~ProgressBarAppearanceEditor()
{
    for(auto* bar : m_bars) {
        bar->setStyle(nullptr);
    }
    m_preview->setStyle(nullptr);
}

void ProgressBarAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool ProgressBarAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        const auto bar = std::ranges::find_if(m_bars, [watched](auto* item) { return watched == item; });
        if(bar != m_bars.end()) {
            const auto part = previewState() == PreviewState::Disabled ? u"disabled"_s
                            : (*bar)->maximum() == 0                   ? u"busy"_s
                                                                       : u"normal"_s;
            Q_EMIT partSelected(part);
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void ProgressBarAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void ProgressBarAppearanceEditor::updatePreview()
{
    const auto resolved = resolveProgressBarAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Progress bar appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().progressBar == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.progressBar = *resolved});
    for(auto* bar : m_bars) {
        bar->setStyle(style.get());
    }
    m_preview->setStyle(style.get());
    m_previewStyle = std::move(style);
}

void ProgressBarAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* bar : m_bars) {
        bar->setEnabled(state != PreviewState::Disabled);
        bar->update();
    }
}
} // namespace Fooyin
