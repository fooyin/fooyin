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

#include "frameappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/frameappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
FrameAppearanceEditor::FrameAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"frame"_s, tr("Frames & separators")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
{
    auto* samples = new QVBoxLayout(m_preview);
    samples->addWidget(new QLabel(tr("List view frame"), m_preview));
    auto* list = new QListWidget(m_preview);
    list->addItems({tr("First item"), tr("Second item"), tr("Third item")});
    samples->addWidget(list, 1);

    samples->addWidget(new QLabel(tr("Panel frame and horizontal separator"), m_preview));
    auto* panel = new QFrame(m_preview);
    panel->setFrameShape(QFrame::StyledPanel);
    auto* panelLayout = new QVBoxLayout(panel);
    auto* first       = new QLabel(tr("First section"), panel);
    first->setAlignment(Qt::AlignCenter);
    panelLayout->addWidget(first, 1);
    auto* horizontal = new QFrame(panel);
    horizontal->setFrameShape(QFrame::HLine);
    panelLayout->addWidget(horizontal);
    auto* second = new QLabel(tr("Second section"), panel);
    second->setAlignment(Qt::AlignCenter);
    panelLayout->addWidget(second, 1);
    samples->addWidget(panel, 1);

    samples->addWidget(new QLabel(tr("Vertical separator"), m_preview));
    auto* sections = new QHBoxLayout();
    sections->addWidget(new QLabel(tr("First section"), m_preview), 1, Qt::AlignCenter);
    auto* vertical = new QFrame(m_preview);
    vertical->setFrameShape(QFrame::VLine);
    sections->addWidget(vertical);
    sections->addWidget(new QLabel(tr("Second section"), m_preview), 1, Qt::AlignCenter);
    samples->addLayout(sections, 1);

    auto* canvas = new QVBoxLayout(previewWidget());
    canvas->setContentsMargins({});
    canvas->addWidget(m_preview, 1);

    static constexpr FrameAppearance defaults;

    auto* frame = addPartPage({}, {});
    addNumber(frame, tr("Border width"), {}, u"borderWidth"_s, defaults.borderWidth, 0, 32, 2);
    addNumber(frame, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    addColour(frame, tr("Border colour"), {}, u"borderColour"_s, defaults.borderColour);
    auto* borderOpacity
        = addNumber(frame, tr("Border opacity"), {}, u"borderOpacity"_s, defaults.borderOpacity, 0, 100, 0);
    borderOpacity->suffix = tr(" %");
    frame->addHint(
        new HintLabel(tr("Applies to panel outlines around lists, tables and other framed widgets."), frame));

    auto* separator = addPartPage(u"separator"_s, tr("Separators"));
    addNumber(separator, tr("Line thickness"), u"separator"_s, u"width"_s, defaults.separator.width, 0, 32, 2);
    auto* inset
        = addNumber(separator, tr("End inset"), u"separator"_s, u"inset"_s, defaults.separator.inset, 0, 128, 2);
    inset->toolTip = tr("Space between each end of the line and the edge of the separator");
    addColour(separator, tr("Colour"), u"separator"_s, u"colour"_s, defaults.separator.colour);
    auto* opacity
        = addNumber(separator, tr("Opacity"), u"separator"_s, u"opacity"_s, defaults.separator.opacity, 0, 100, 0);
    opacity->suffix = tr(" %");
    separator->addHint(new HintLabel(tr("Applies to simple horizontal and vertical dividing lines."), separator));

    applyPreviewStyle(m_previewStyle.get());
    preparePreview(m_preview);
}

FrameAppearanceEditor::~FrameAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void FrameAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    if(!showPage(part)) {
        return;
    }
    if(updateState) {
        requestPreviewState(part);
    }
}

bool FrameAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        auto* target = qobject_cast<QWidget*>(watched);
        while(target && target != m_preview) {
            if(const auto* frame = qobject_cast<QFrame*>(target)) {
                if(frame->frameShape() == QFrame::HLine || frame->frameShape() == QFrame::VLine) {
                    Q_EMIT partSelected(u"separator"_s);
                    break;
                }
                if(frame->frameShape() == QFrame::StyledPanel) {
                    Q_EMIT partSelected(QString{});
                    break;
                }
            }
            target = target->parentWidget();
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void FrameAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void FrameAppearanceEditor::updatePreview()
{
    const auto resolved = resolveFrameAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Frame appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().frame == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.frame = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void FrameAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_preview->setEnabled(state != PreviewState::Disabled);
    m_preview->update();
}

void FrameAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    setPreviewStyle(m_preview, style);
    const auto frames = m_preview->findChildren<QFrame*>();
    for(auto* frame : frames) {
        frame->updateGeometry();
        frame->update();
    }
}
} // namespace Fooyin
