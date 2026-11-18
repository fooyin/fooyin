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

#include "tooltipappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "style/tooltipstyle.h"
#include "theme/tooltipappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QLabel>
#include <QMouseEvent>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class ToolTipPreview : public QLabel
{
public:
    ToolTipPreview(const QString& text, QWidget* parent)
        : QLabel{text, parent}
    {
        setIndent(1);
        setTextInteractionFlags(Qt::NoTextInteraction);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QStyleOptionFrame option;
        option.initFrom(this);
        option.palette = parentWidget()->palette();

        QStylePainter painter{this};
        painter.drawPrimitive(QStyle::PE_PanelTipLabel, option);
        painter.end();
        QLabel::paintEvent(event);
    }
};
} // namespace

ToolTipAppearanceEditor::ToolTipAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"toolTip"_s, tr("Tooltips")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
{
    setParts({{.id = u"background"_s, .title = tr("Background"), .parentId = {}},
              {.id = u"border"_s, .title = tr("Border"), .parentId = {}},
              {.id = u"text"_s, .title = tr("Text"), .parentId = {}}});

    auto* samplesLayout = new QVBoxLayout(m_preview);
    samplesLayout->addStretch();

    const std::array captions{tr("Plain text"), tr("Rich text")};
    const std::array texts{tr("Example tooltip"), tr("<b>Northern lights</b><br/>Example artist — Example album")};

    for(size_t index{0}; index < texts.size(); ++index) {
        samplesLayout->addWidget(new QLabel(captions[index], m_preview));

        auto* sample = new ToolTipPreview(texts[index], m_preview);
        m_samples.push_back(sample);
        samplesLayout->addWidget(sample, 0, Qt::AlignLeft);
        samplesLayout->addSpacing(16);
    }

    samplesLayout->addStretch();

    auto* previewLayout = new QVBoxLayout(previewWidget());
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(m_preview, 1);

    applyPreviewStyle(m_previewStyle.get());
    preparePreview(m_preview);

    const ToolTipAppearance defaults;

    auto* dimensions = addPage({});
    auto* padding    = addNumber(dimensions, tr("Content padding"), {}, u"padding"_s, defaults.padding, 1, 128, 0);
    padding->toolTip = tr("Space between the tooltip edge and its contents. The border may increase this value.");
    addNumber(dimensions, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);

    auto* background = addPage(u"background"_s);
    addColour(background, tr("Colour"), {}, u"backgroundColour"_s, defaults.backgroundColour, QPalette::Inactive);
    auto* backgroundOpacity
        = addNumber(background, tr("Opacity"), {}, u"backgroundOpacity"_s, defaults.backgroundOpacity, 0, 100, 0);
    backgroundOpacity->suffix = tr(" %");

    auto* border = addPage(u"border"_s);
    addNumber(border, tr("Width"), {}, u"borderWidth"_s, defaults.borderWidth, 0, 32, 2);
    addColour(border, tr("Colour"), {}, u"borderColour"_s, defaults.borderColour, QPalette::Inactive);
    auto* borderOpacity   = addNumber(border, tr("Opacity"), {}, u"borderOpacity"_s, defaults.borderOpacity, 0, 100, 0);
    borderOpacity->suffix = tr(" %");

    auto* text = addPage(u"text"_s);
    addColour(text, tr("Colour"), {}, u"foregroundColour"_s, defaults.foregroundColour, QPalette::Inactive);
}

ToolTipAppearanceEditor::~ToolTipAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void ToolTipAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    if(!showPage(part)) {
        return;
    }
    if(updateState) {
        requestPreviewState(part);
    }
}

bool ToolTipAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() != QEvent::MouseButtonPress) {
        return ComponentAppearanceEditor::eventFilter(watched, event);
    }

    for(const auto* sample : m_samples) {
        if(watched != sample) {
            continue;
        }

        const auto position = static_cast<QMouseEvent*>(event)->position().toPoint();

        const double border = std::max(3.0, m_previewStyle->appearance().toolTip.borderWidth);
        const QRectF inner  = QRectF{sample->rect()}.adjusted(border, border, -border, -border);
        const int margin    = sample->margin();
        const QRect text    = sample->contentsRect().adjusted(margin, margin, -margin, -margin);
        Q_EMIT partSelected(!inner.contains(position) ? u"border"_s
                            : text.contains(position) ? u"text"_s
                                                      : u"background"_s);
        break;
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void ToolTipAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
    applyPreviewStyle(m_previewStyle.get());
}

void ToolTipAppearanceEditor::updatePreview()
{
    const auto resolved = resolveToolTipAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Tooltip appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().toolTip == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.toolTip = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void ToolTipAppearanceEditor::updatePreviewState(PreviewState /*state*/) { }

void ToolTipAppearanceEditor::applyPreviewStyle(FooyinStyle* style)
{
    for(auto* sample : m_samples) {
        sample->setStyle(style);
        sample->setMargin(1 + sample->style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, sample));
        sample->setForegroundRole(QPalette::ToolTipText);
        sample->setPalette(style ? Style::ToolTip::textPalette(m_preview->palette(), style->appearance().toolTip)
                                 : m_preview->palette());
        sample->updateGeometry();
        sample->update();
    }
}
} // namespace Fooyin
