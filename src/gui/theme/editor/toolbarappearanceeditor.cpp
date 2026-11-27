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

#include "toolbarappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/toolbarappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QAction>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyleOptionToolBar>
#include <QStylePainter>
#include <QToolBar>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class FramedToolBarPreview : public QToolBar
{
public:
    explicit FramedToolBarPreview(QWidget* parent)
        : QToolBar{parent}
    { }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QToolBar::paintEvent(event);

        QStyleOptionToolBar option;
        initStyleOption(&option);

        QStylePainter painter{this};
        painter.drawPrimitive(QStyle::PE_FrameMenu, option);
    }
};
} // namespace

ToolBarAppearanceEditor::ToolBarAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"toolBar"_s, tr("Toolbars")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
{
    auto* window = new QMainWindow(m_preview);
    window->setWindowFlags(Qt::Widget);
    window->setMinimumSize(240, 200);

    auto* caption = new QLabel(tr("Horizontal and vertical toolbars"), window);
    caption->setAlignment(Qt::AlignCenter);
    caption->setWordWrap(true);
    window->setCentralWidget(caption);

    auto* horizontal = new QToolBar(tr("Horizontal toolbar"), window);
    auto* vertical   = new QToolBar(tr("Vertical toolbar"), window);
    window->addToolBar(Qt::TopToolBarArea, horizontal);
    window->addToolBar(Qt::LeftToolBarArea, vertical);

    auto* framed = new FramedToolBarPreview(m_preview);

    static constexpr std::array icons{QStyle::SP_MediaPlay, QStyle::SP_MediaPause, QStyle::SP_MediaStop,
                                      QStyle::SP_MediaSkipBackward, QStyle::SP_MediaSkipForward};
    const std::array labels{tr("Play"), tr("Pause"), tr("Stop"), tr("Previous"), tr("Next")};
    for(auto* toolbar : {horizontal, vertical, static_cast<QToolBar*>(framed)}) {
        toolbar->setFloatable(false);
        toolbar->setIconSize({16, 16});
        for(size_t index{0}; index < icons.size(); ++index) {
            if(index == 3) {
                toolbar->addSeparator();
            }
            toolbar->addAction(m_previewStyle->standardIcon(icons[index]), labels[index]);
        }
    }

    framed->setFixedWidth(120);

    auto* samples = new QVBoxLayout(m_preview);
    samples->addWidget(window, 1);
    samples->addWidget(new QLabel(tr("Toolbar frame and overflow"), m_preview));
    samples->addWidget(framed, 0, Qt::AlignLeft);

    auto* canvas = new QVBoxLayout(previewWidget());
    canvas->setContentsMargins({});
    canvas->addWidget(m_preview, 1);

    const ToolBarAppearance defaults;

    auto* dimensions = addPartPage({}, {});
    auto* margin     = addNumber(dimensions, tr("Item margin"), {}, u"itemMargin"_s, defaults.itemMargin, 0, 128, 0);
    margin->toolTip  = tr("Extra space around toolbar items. The border width is added to this value.");
    addNumber(dimensions, tr("Item spacing"), {}, u"itemSpacing"_s, defaults.itemSpacing, 0, 128, 0);
    dimensions->addHint(new HintLabel(tr("Toolbar buttons use the Tool button appearance settings."), dimensions));

    auto* background = addPartPage(u"background"_s, tr("Background"));
    addBrush(background, tr("Colour"), {}, u"backgroundColour"_s, defaults.backgroundColour);
    auto* backgroundOpacity
        = addNumber(background, tr("Opacity"), {}, u"backgroundOpacity"_s, defaults.backgroundOpacity, 0, 100, 0);
    backgroundOpacity->suffix = tr(" %");

    auto* border = addPartPage(u"border"_s, tr("Border"));
    addNumber(border, tr("Width"), u"border"_s, u"width"_s, defaults.border.width, 0, 32, 0);
    addColour(border, tr("Colour"), u"border"_s, u"colour"_s, defaults.border.colour);
    addColour(border, tr("Tint colour"), u"border"_s, u"tintColour"_s, defaults.border.tintColour);
    auto* borderTintOpacity
        = addNumber(border, tr("Tint opacity"), u"border"_s, u"tintOpacity"_s, defaults.border.tintOpacity, 0, 100, 0);
    borderTintOpacity->suffix  = tr(" %");
    borderTintOpacity->toolTip = tr("Amount of tint blended into the border colour");
    auto* borderOpacity
        = addNumber(border, tr("Opacity"), u"border"_s, u"opacity"_s, defaults.border.opacity, 0, 100, 0);
    borderOpacity->suffix   = tr(" %");
    const auto frameToolTip = tr("The frame surrounds expanded toolbars showing overflow items and floating toolbars.");
    auto* frameOpacity      = addNumber(border, tr("Frame opacity"), u"border"_s, u"floatingOpacity"_s,
                                        defaults.border.floatingOpacity, 0, 100, 0);
    frameOpacity->suffix    = tr(" %");
    frameOpacity->toolTip   = frameToolTip;
    auto* frameRadius       = addNumber(border, tr("Frame corner radius"), u"border"_s, u"floatingRadius"_s,
                                        defaults.border.floatingRadius, 0, 128, 2);
    frameRadius->toolTip    = frameToolTip;

    auto* handle = addPartPage(u"handle"_s, tr("Drag handle"));
    addNumber(handle, tr("Handle size"), u"handle"_s, u"extent"_s, defaults.handle.extent, 0, 128, 0);
    addNumber(handle, tr("Dot radius"), u"handle"_s, u"dotRadius"_s, defaults.handle.dotRadius, 0, 32, 2);
    addNumber(handle, tr("Column spacing"), u"handle"_s, u"columnSpacing"_s, defaults.handle.columnSpacing, 0, 128, 2);
    addNumber(handle, tr("Row spacing"), u"handle"_s, u"rowSpacing"_s, defaults.handle.rowSpacing, 0, 128, 2);
    addColour(handle, tr("Colour"), u"handle"_s, u"colour"_s, defaults.handle.colour);
    auto* handleOpacity
        = addNumber(handle, tr("Opacity"), u"handle"_s, u"opacity"_s, defaults.handle.opacity, 0, 100, 0);
    handleOpacity->suffix = tr(" %");

    auto* separator = addPartPage(u"separator"_s, tr("Separators"));
    addNumber(separator, tr("Separator spacing"), u"separator"_s, u"extent"_s, defaults.separator.extent, 0, 128, 0);
    addNumber(separator, tr("Line length"), u"separator"_s, u"length"_s, defaults.separator.length, 0, 128, 2);
    addNumber(separator, tr("End margin"), u"separator"_s, u"margin"_s, defaults.separator.margin, 0, 128, 2);
    addNumber(separator, tr("Line width"), u"separator"_s, u"width"_s, defaults.separator.width, 0, 32, 2);
    addColour(separator, tr("Colour"), u"separator"_s, u"colour"_s, defaults.separator.colour);
    addColour(separator, tr("Tint colour"), u"separator"_s, u"tintColour"_s, defaults.separator.tintColour);
    auto* separatorTintOpacity    = addNumber(separator, tr("Tint opacity"), u"separator"_s, u"tintOpacity"_s,
                                              defaults.separator.tintOpacity, 0, 100, 0);
    separatorTintOpacity->suffix  = tr(" %");
    separatorTintOpacity->toolTip = tr("Amount of tint blended into the separator colour");
    auto* separatorOpacity
        = addNumber(separator, tr("Opacity"), u"separator"_s, u"opacity"_s, defaults.separator.opacity, 0, 100, 0);
    separatorOpacity->suffix = tr(" %");

    auto* extension = addPartPage(u"extension"_s, tr("Overflow indicator"));
    addNumber(extension, tr("Button size"), u"extension"_s, u"extent"_s, defaults.extension.extent, 1, 128, 0);
    addNumber(extension, tr("Symbol size"), u"extension"_s, u"glyphSize"_s, defaults.extension.glyphSize, 0, 128, 2);
    addNumber(extension, tr("Symbol thickness"), u"extension"_s, u"glyphWidth"_s, defaults.extension.glyphWidth, 0, 32,
              2);
    addColour(extension, tr("Symbol colour"), u"extension"_s, u"colour"_s, defaults.extension.colour);

    applyPreviewStyle(m_previewStyle.get());
    preparePreview(m_preview);
}

ToolBarAppearanceEditor::~ToolBarAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void ToolBarAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    if(!showPage(part)) {
        return;
    }
    if(updateState) {
        requestPreviewState(part);
    }
}

bool ToolBarAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        if(watched->inherits("QToolBarExtension")) {
            Q_EMIT partSelected(u"extension"_s);
        }
        else if(watched->inherits("QToolBarSeparator")) {
            Q_EMIT partSelected(u"separator"_s);
        }
        else if(const auto* toolbar = qobject_cast<QToolBar*>(watched)) {
            QStyleOptionToolBar option;
            option.initFrom(toolbar);
            option.features.setFlag(QStyleOptionToolBar::Movable, toolbar->isMovable());
            option.state.setFlag(QStyle::State_Horizontal, toolbar->orientation() == Qt::Horizontal);

            const auto position = static_cast<QMouseEvent*>(event)->position().toPoint();
            const auto handle   = toolbar->style()->subElementRect(QStyle::SE_ToolBarHandle, &option, toolbar);
            const int border    = std::max(3, m_previewStyle->appearance().toolBar.border.width);

            Q_EMIT partSelected(handle.contains(position) ? u"handle"_s
                                : toolbar->rect().adjusted(border, border, -border, -border).contains(position)
                                    ? u"background"_s
                                    : u"border"_s);
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void ToolBarAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void ToolBarAppearanceEditor::updatePreview()
{
    const auto resolved = resolveToolBarAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Toolbar appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().toolBar == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.toolBar = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void ToolBarAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_preview->setEnabled(state != PreviewState::Disabled);
    m_preview->update();
}

void ToolBarAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    setPreviewStyle(m_preview, style);
    const auto toolbars = m_preview->findChildren<QToolBar*>();
    for(auto* toolbar : toolbars) {
        toolbar->layout()->invalidate();
        toolbar->updateGeometry();
        toolbar->update();
    }
}
} // namespace Fooyin
