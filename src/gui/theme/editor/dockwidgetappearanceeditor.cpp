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

#include "dockwidgetappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/dockwidgetappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QDockWidget>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyleOptionDockWidget>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class DockWidgetPreview : public QDockWidget
{
public:
    DockWidgetPreview(const QString& title, bool frame, QWidget* parent)
        : QDockWidget{title, parent}
        , m_frame{frame}
    {
        setWindowFlags(Qt::Widget);
        setFeatures(DockWidgetClosable | DockWidgetFloatable);
        updateFrameMargins();
    }

    [[nodiscard]] QStyleOptionDockWidget styleOption() const
    {
        QStyleOptionDockWidget option;
        initStyleOption(&option);
        return option;
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QDockWidget::changeEvent(event);
        if(event->type() == QEvent::StyleChange) {
            updateFrameMargins();
        }
    }

    void paintEvent(QPaintEvent* event) override
    {
        QDockWidget::paintEvent(event);

        if(m_frame) {
            QStyleOptionFrame option;
            option.initFrom(this);

            QStylePainter painter{this};
            painter.drawPrimitive(QStyle::PE_FrameDockWidget, option);
        }
    }

private:
    void updateFrameMargins()
    {
        const int width = m_frame ? style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, nullptr, this) : 0;
        setContentsMargins(width, width, width, width);
    }

    bool m_frame;
};
} // namespace

DockWidgetAppearanceEditor::DockWidgetAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"dockWidget"_s, tr("Dock widgets")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
    , m_docks{}
{
    auto* window = new QMainWindow(m_preview);
    window->setWindowFlags(Qt::Widget);
    window->setMinimumSize(240, 200);

    auto* caption = new QLabel(tr("Docked panels"), window);
    caption->setAlignment(Qt::AlignCenter);
    caption->setWordWrap(true);
    window->setCentralWidget(caption);

    m_docks[0] = new DockWidgetPreview(tr("Horizontal title"), false, window);
    m_docks[1] = new DockWidgetPreview(tr("Vertical title"), false, window);
    m_docks[1]->setFeatures(m_docks[1]->features() | QDockWidget::DockWidgetVerticalTitleBar);
    window->addDockWidget(Qt::BottomDockWidgetArea, m_docks[0]);
    window->addDockWidget(Qt::LeftDockWidgetArea, m_docks[1]);

    m_docks[2] = new DockWidgetPreview(tr("Dock widget frame"), true, m_preview);
    for(auto* dock : m_docks) {
        auto* content = new QLabel(tr("Panel contents"), dock);
        content->setAlignment(Qt::AlignCenter);
        content->setMinimumSize(60, 60);
        content->setAutoFillBackground(true);
        content->setBackgroundRole(QPalette::Base);
        content->setForegroundRole(QPalette::Text);
        dock->setWidget(content);
    }

    auto* samples = new QVBoxLayout(m_preview);
    samples->addWidget(window, 1);
    samples->addWidget(new QLabel(tr("Frame preview"), m_preview));
    samples->addWidget(m_docks[2]);

    auto* canvas = new QVBoxLayout(previewWidget());
    canvas->setContentsMargins({});
    canvas->addWidget(m_preview, 1);

    const DockWidgetAppearance defaults;

    auto* dimensions = addPartPage({}, {});
    auto* titleMargin
        = addNumber(dimensions, tr("Title margin"), {}, u"titleMargin"_s, defaults.titleMargin, 0, 128, 0);
    titleMargin->toolTip = tr("Space around the title text. This also affects the title bar height.");
    auto* gap
        = addNumber(dimensions, tr("Resize gap width"), {}, u"separatorExtent"_s, defaults.separatorExtent, 0, 128, 0);
    gap->toolTip = tr("Space between dock widgets used to resize them");

    auto* title = addPartPage(u"title"_s, tr("Title bar"));
    addBrush(title, tr("Background"), u"title"_s, u"backgroundColour"_s, defaults.title.backgroundColour);
    auto* opacity   = addNumber(title, tr("Background opacity"), u"title"_s, u"backgroundOpacity"_s,
                                defaults.title.backgroundOpacity, 0, 100, 0);
    opacity->suffix = tr(" %");
    addColour(title, tr("Text colour"), u"title"_s, u"foregroundColour"_s, defaults.title.foregroundColour);
    addColour(title, tr("Button icon colour"), u"title"_s, u"iconColour"_s, defaults.title.iconColour);

    for(const auto& [group, border] :
        std::array{std::pair{u"title.border"_s, defaults.title.border}, std::pair{u"frame"_s, defaults.frame}}) {
        const bool frame = group == "frame"_L1;
        auto* page       = addPartPage(group, frame ? tr("Frame") : tr("Border"), frame ? QString{} : u"title"_s);
        addNumber(page, tr("Width"), group, u"width"_s, border.width, 0, 32, 0);
        addColour(page, tr("Colour"), group, u"colour"_s, border.colour);
        addColour(page, tr("Tint colour"), group, u"tintColour"_s, border.tintColour);
        auto* tint    = addNumber(page, tr("Tint opacity"), group, u"tintOpacity"_s, border.tintOpacity, 0, 100, 0);
        tint->suffix  = tr(" %");
        tint->toolTip = tr("Amount of tint blended into the border colour");
        auto* borderOpacity   = addNumber(page, tr("Opacity"), group, u"opacity"_s, border.opacity, 0, 100, 0);
        borderOpacity->suffix = tr(" %");
        if(frame) {
            page->addHint(new HintLabel(
                tr("The frame applies to floating dock widgets when fooyin draws their window decorations."), page));
        }
    }

    auto* buttons = addPartPage(u"buttons"_s, tr("Title buttons"), u"title"_s);
    addNumber(buttons, tr("Button margin"), {}, u"buttonMargin"_s, defaults.buttonMargin, 0, 128, 0);
    buttons->addHint(
        new HintLabel(tr("Close and float button surfaces use the Tool button appearance settings."), buttons));

    applyPreviewStyle(m_previewStyle.get());
    preparePreview(m_preview);
}

DockWidgetAppearanceEditor::~DockWidgetAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void DockWidgetAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    if(!showPage(part)) {
        return;
    }
    if(updateState) {
        requestPreviewState(part);
    }
}

bool DockWidgetAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonDblClick && qobject_cast<QDockWidget*>(watched)) {
        return true;
    }
    if(event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick) {
        return ComponentAppearanceEditor::eventFilter(watched, event);
    }
    if(watched->inherits("QDockWidgetTitleButton")) {
        Q_EMIT partSelected(u"buttons"_s);
        return true;
    }

    const auto* target = qobject_cast<QWidget*>(watched);
    for(auto* dock : m_docks) {
        if(!target || (target != dock && !dock->isAncestorOf(target))) {
            continue;
        }

        const auto position = target->mapTo(dock, static_cast<QMouseEvent*>(event)->position().toPoint());
        const auto option   = static_cast<DockWidgetPreview*>(dock)->styleOption();
        if(option.rect.contains(position)) {
            const int width   = std::max(3, m_previewStyle->appearance().dockWidget.title.border.width);
            const bool border = option.verticalTitleBar ? position.x() >= option.rect.right() - width + 1
                                                        : position.y() >= option.rect.bottom() - width + 1;
            Q_EMIT partSelected(border ? u"title.border"_s : u"title"_s);
        }
        else if(dock == m_docks[2]) {
            const int width = std::max(3, m_previewStyle->appearance().dockWidget.frame.width);
            if(!dock->rect().adjusted(width, width, -width, -width).contains(position)) {
                Q_EMIT partSelected(u"frame"_s);
            }
        }
        break;
    }

    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void DockWidgetAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void DockWidgetAppearanceEditor::updatePreview()
{
    const auto resolved = resolveDockWidgetAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Dock widget appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().dockWidget == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.dockWidget = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void DockWidgetAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_preview->setEnabled(state != PreviewState::Disabled);
    m_preview->update();
}

void DockWidgetAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    setPreviewStyle(m_preview, style);
    for(auto* dock : m_docks) {
        dock->layout()->invalidate();
        dock->updateGeometry();
        dock->update();
    }
}
} // namespace Fooyin
