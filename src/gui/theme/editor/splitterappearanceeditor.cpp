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

#include "splitterappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/splitterappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QEvent>
#include <QLabel>
#include <QSplitter>
#include <QStyleOption>
#include <QStylePainter>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class SplitterPreviewHandle : public QSplitterHandle
{
public:
    SplitterPreviewHandle(Qt::Orientation orientation, QSplitter* parent, ComponentAppearanceEditor* editor)
        : QSplitterHandle{orientation, parent}
        , m_editor{editor}
    { }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if(!m_editor || m_editor->previewState() == PreviewState::Normal) {
            QSplitterHandle::paintEvent(event);
            return;
        }

        QStyleOption option;
        option.initFrom(this);
        option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
        option.state.setFlag(QStyle::State_Horizontal, orientation() == Qt::Horizontal);

        if(m_editor && isEnabled()) {
            option.state.setFlag(QStyle::State_MouseOver, m_editor->previewState() == PreviewState::Hovered
                                                              || m_editor->previewState() == PreviewState::Pressed);
            option.state.setFlag(QStyle::State_Sunken, m_editor->previewState() == PreviewState::Pressed);
        }

        QStylePainter painter{this};
        painter.drawControl(QStyle::CE_Splitter, option);
    }

private:
    QPointer<ComponentAppearanceEditor> m_editor;
};

class SplitterPreview : public QSplitter
{
public:
    SplitterPreview(Qt::Orientation orientation, ComponentAppearanceEditor* editor, QWidget* parent)
        : QSplitter{orientation, parent}
        , m_editor{editor}
    {
        setChildrenCollapsible(false);
    }

protected:
    QSplitterHandle* createHandle() override
    {
        return new SplitterPreviewHandle(orientation(), this, m_editor);
    }

private:
    QPointer<ComponentAppearanceEditor> m_editor;
};
} // namespace

SplitterAppearanceEditor::SplitterAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"splitter"_s, tr("Splitters")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
    , m_splitters{new SplitterPreview(Qt::Horizontal, this, m_preview),
                  new SplitterPreview(Qt::Vertical, this, m_preview)}
{
    auto* samplesLayout = new QVBoxLayout(m_preview);

    const std::array captions{tr("Side-by-side panes"), tr("Stacked panes")};

    for(size_t index{0}; index < m_splitters.size(); ++index) {
        auto* splitter = m_splitters[index];
        samplesLayout->addWidget(new QLabel(captions[index], m_preview));

        for(const auto& text : {tr("First pane"), tr("Second pane")}) {
            auto* pane = new QLabel(text, splitter);
            pane->setAlignment(Qt::AlignCenter);
            pane->setMinimumSize(80, 60);
            pane->setAutoFillBackground(true);
            pane->setBackgroundRole(QPalette::Base);
            pane->setForegroundRole(QPalette::Text);
            splitter->addWidget(pane);
        }

        samplesLayout->addWidget(splitter, 1);
    }

    auto* previewLayout = new QVBoxLayout(previewWidget());
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(m_preview, 1);

    applyPreviewStyle(m_previewStyle.get());
    preparePreview(m_preview);

    const SplitterAppearance defaults;

    auto* dimensions = addPartPage({}, {});
    addNumber(dimensions, tr("Handle width"), {}, u"width"_s, defaults.width, 1, 128, 0);
    addNumber(dimensions, tr("Background corner radius"), {}, u"backgroundRadius"_s, defaults.backgroundRadius, 0, 128,
              2);
    dimensions->addHint(new HintLabel(tr("Handle width controls spacing and the area used to drag the splitter. "
                                         "Explicit handle sizes and visibility settings in the layout take priority."),
                                      dimensions));

    auto* grip = addPartPage(u"grip"_s, tr("Grip"));
    addNumber(grip, tr("Length"), u"grip"_s, u"length"_s, defaults.grip.length, 0, 128, 2);
    addNumber(grip, tr("Thickness"), u"grip"_s, u"thickness"_s, defaults.grip.thickness, 0, 128, 2);
    addNumber(grip, tr("End margin"), u"grip"_s, u"margin"_s, defaults.grip.margin, 0, 128, 2);
    auto* radius
        = addNumber(grip, tr("Corner roundness"), u"grip"_s, u"cornerRadius"_s, defaults.grip.cornerRadius, 0, 100, 0);
    radius->suffix = tr(" %");
    grip->addHint(new HintLabel(tr("Zero length or thickness hides the grip."), grip));

    addPart(
        {.id       = u"surface"_s,
         .title    = tr("Surface"),
         .parentId = {},
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
            {.id = u"hovered"_s, .title = tr("Hovered"), .pageId = u"hovered"_s, .previewState = PreviewState::Hovered},
            {.id = u"pressed"_s, .title = tr("Pressed"), .pageId = u"pressed"_s, .previewState = PreviewState::Pressed},
            {.id           = u"disabled"_s,
             .title        = tr("Disabled"),
             .pageId       = u"disabled"_s,
             .previewState = PreviewState::Disabled}}});

    const std::array appearances{defaults.normal, defaults.hovered, defaults.pressed, defaults.disabled};

    const auto& states = parts().back().states;
    for(size_t index{0}; index < states.size(); ++index) {
        const auto& name  = states[index].pageId;
        const auto& state = appearances[index];
        auto* page        = addPage(name);
        const auto group  = stateColourGroup(name);

        addColour(page, tr("Background colour"), name, u"backgroundColour"_s, state.backgroundColour, group);

        auto* backgroundOpacity   = addNumber(page, tr("Background opacity"), name, u"backgroundOpacity"_s,
                                              state.backgroundOpacity, 0, 100, 0);
        backgroundOpacity->suffix = tr(" %");

        addColour(page, tr("Grip colour"), name, u"gripColour"_s, state.gripColour, group);

        auto* gripOpacity   = addNumber(page, tr("Grip opacity"), name, u"gripOpacity"_s, state.gripOpacity, 0, 100, 0);
        gripOpacity->suffix = tr(" %");
    }
}

SplitterAppearanceEditor::~SplitterAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void SplitterAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool SplitterAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        const auto* handle = qobject_cast<QSplitterHandle*>(watched);
        if(handle && (handle->splitter() == m_splitters[0] || handle->splitter() == m_splitters[1])) {
            Q_EMIT partSelected(previewPage(u"surface"_s, previewState()));
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void SplitterAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
    refreshPreview();
}

void SplitterAppearanceEditor::updatePreview()
{
    const auto resolved = resolveSplitterAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Splitter appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().splitter == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.splitter = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void SplitterAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_preview->setEnabled(state != PreviewState::Disabled);
    refreshPreview();
}

void SplitterAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    for(auto* splitter : m_splitters) {
        splitter->setStyle(style);

        const auto children = splitter->findChildren<QWidget*>();
        for(auto* child : children) {
            child->setStyle(style);
        }
    }
    refreshPreview();
}

void SplitterAppearanceEditor::refreshPreview()
{
    for(auto* splitter : m_splitters) {
        for(int index{1}; index < splitter->count(); ++index) {
            splitter->handle(index)->updateGeometry();
            splitter->handle(index)->update();
        }
        splitter->updateGeometry();
    }
}
} // namespace Fooyin
