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

#include "headerappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/headerappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStyleOptionHeader>
#include <QTableView>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class HeaderPreview : public QHeaderView
{
public:
    HeaderPreview(Qt::Orientation orientation, HeaderAppearanceEditor* editor, QWidget* parent)
        : QHeaderView{orientation, parent}
        , m_editor{editor}
    {
        setSectionsClickable(true);
        setSectionsMovable(true);
        setHighlightSections(true);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void initStyleOptionForIndex(QStyleOptionHeader* option, int logicalIndex) const override
    {
        QHeaderView::initStyleOptionForIndex(option, logicalIndex);

        if(!m_editor
           || (m_editor->previewState() == PreviewState::Normal && !m_editor->selectedPreview()
               && !m_editor->dragTargetPreview() && !m_editor->sortedPreview())) {
            return;
        }

        const auto state      = m_editor->previewState();
        option->sortIndicator = logicalIndex == m_activeSection && m_editor->sortedPreview() && isSortIndicatorShown()
                                  ? (sortIndicatorOrder() == Qt::AscendingOrder ? QStyleOptionHeader::SortUp
                                                                                : QStyleOptionHeader::SortDown)
                                  : QStyleOptionHeader::None;
        option->state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_On | QStyle::State_HasFocus
                           | QStyle::State_KeyboardFocusChange);

        if(auto* extended = qstyleoption_cast<QStyleOptionHeaderV2*>(option)) {
            extended->isSectionDragTarget = false;
        }

        if(logicalIndex != m_activeSection || !(option->state & QStyle::State_Enabled)) {
            return;
        }

        if(state == PreviewState::Hovered || state == PreviewState::Pressed) {
            option->state |= QStyle::State_MouseOver;
        }
        if(state == PreviewState::Pressed) {
            option->state |= QStyle::State_Sunken;
        }
        if(state == PreviewState::Focused) {
            option->state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
        }
        if(m_editor->selectedPreview()) {
            option->state |= QStyle::State_On;
        }

        if(auto* extended = qstyleoption_cast<QStyleOptionHeaderV2*>(option);
           extended && m_editor->dragTargetPreview()) {
            extended->isSectionDragTarget = true;
        }
    }

public:
    void selectSection(int section)
    {
        if(section >= 0) {
            m_activeSection = section;
            viewport()->update();
        }
    }

private:
    QPointer<HeaderAppearanceEditor> m_editor;
    int m_activeSection{0};
};
} // namespace

HeaderAppearanceEditor::HeaderAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"header"_s, tr("Headers")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_table{new QTableView(previewWidget())}
    , m_selectedPreview{false}
    , m_dragTargetPreview{false}
    , m_sortedPreview{false}
{
    auto* horizontal = new HeaderPreview(Qt::Horizontal, this, m_table);
    auto* vertical   = new HeaderPreview(Qt::Vertical, this, m_table);
    horizontal->viewport()->installEventFilter(this);
    vertical->viewport()->installEventFilter(this);

    m_table->setHorizontalHeader(horizontal);
    m_table->setVerticalHeader(vertical);

    auto* model = new QStandardItemModel(4, 3, m_table);
    model->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Time")});
    model->setVerticalHeaderLabels({tr("One"), tr("Two"), tr("Three"), tr("Four")});
    model->setHeaderData(0, Qt::Horizontal, m_previewStyle->standardIcon(QStyle::SP_FileIcon), Qt::DecorationRole);
    model->setHeaderData(1, Qt::Horizontal, m_previewStyle->standardIcon(QStyle::SP_DirIcon), Qt::DecorationRole);

    const std::array tracks{tr("Northern lights"), tr("After hours"), tr("Soft focus"), tr("Sundown")};
    for(size_t row{0}; row < tracks.size(); ++row) {
        model->setItem(static_cast<int>(row), 0, new QStandardItem(tracks[row]));
        model->setItem(static_cast<int>(row), 1, new QStandardItem(tr("Example artist")));
        model->setItem(static_cast<int>(row), 2, new QStandardItem(u"03:24"_s));
    }

    m_table->setModel(model);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(0, Qt::AscendingOrder);

    horizontal->setStretchLastSection(false);
    horizontal->setDefaultSectionSize(100);
    vertical->setDefaultSectionSize(36);
    vertical->setSortIndicatorShown(true);
    vertical->setSortIndicator(0, Qt::AscendingOrder);

    QObject::connect(vertical, &QHeaderView::sectionClicked, this, [vertical](int section) {
        const auto order
            = vertical->sortIndicatorSection() == section && vertical->sortIndicatorOrder() == Qt::AscendingOrder
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
        vertical->setSortIndicator(section, order);
    });

    auto* order = new QComboBox(previewWidget());
    order->addItems({tr("Ascending"), tr("Descending"), tr("No indicator")});

    QObject::connect(order, &QComboBox::currentIndexChanged, this, [this](int index) {
        for(auto* header : {m_table->horizontalHeader(), m_table->verticalHeader()}) {
            header->setSortIndicatorShown(index != 2);
            if(index != 2) {
                header->setSortIndicator(0, index == 0 ? Qt::AscendingOrder : Qt::DescendingOrder);
            }
        }
        refreshPreview();
    });

    for(const auto* header : {horizontal, vertical}) {
        QObject::connect(header, &QHeaderView::sortIndicatorChanged, this, [this, order](int, Qt::SortOrder sortOrder) {
            const QSignalBlocker blocker{order};
            order->setCurrentIndex(sortOrder == Qt::AscendingOrder ? 0 : 1);
            for(auto* other : {m_table->horizontalHeader(), m_table->verticalHeader()}) {
                other->setSortIndicator(other->sortIndicatorSection(), sortOrder);
            }
            refreshPreview();
        });
    }

    auto* controls       = new QWidget(previewWidget());
    auto* controlsLayout = new QFormLayout(controls);
    controlsLayout->addRow(tr("Sort order"), order);

    auto* previewLayout = new QVBoxLayout(previewWidget());
    previewLayout->setContentsMargins({});
    previewLayout->addWidget(controls);
    previewLayout->addWidget(m_table, 1);

    applyPreviewStyle(m_previewStyle.get());

    const HeaderAppearance defaults;

    auto* dimensions = addPartPage({}, {});
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Minimum thickness"), {}, u"minimumThickness"_s, defaults.minimumThickness, 0, 512, 0);
    addNumber(dimensions, tr("Content spacing"), {}, u"spacing"_s, defaults.spacing, 0, 128, 0);
    addNumber(dimensions, tr("Icon size"), {}, u"iconSize"_s, defaults.iconSize, 1, 128, 0);
    addBrush(dimensions, tr("Empty area background"), {}, u"emptyColour"_s, defaults.emptyColour);

    addPart(
        {.id       = u"surface"_s,
         .title    = tr("Surface"),
         .parentId = {},
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
            {.id = u"hovered"_s, .title = tr("Hovered"), .pageId = u"hovered"_s, .previewState = PreviewState::Hovered},
            {.id = u"pressed"_s, .title = tr("Pressed"), .pageId = u"pressed"_s, .previewState = PreviewState::Pressed},
            {.id = u"sorted"_s, .title = tr("Sorted"), .pageId = u"sorted"_s, .previewState = PreviewState::Normal},
            {.id           = u"selected"_s,
             .title        = tr("Selected"),
             .pageId       = u"selected"_s,
             .previewState = PreviewState::Normal},
            {.id           = u"dragTarget"_s,
             .title        = tr("Drag target"),
             .pageId       = u"dragTarget"_s,
             .previewState = PreviewState::Normal},
            {.id           = u"disabled"_s,
             .title        = tr("Disabled"),
             .pageId       = u"disabled"_s,
             .previewState = PreviewState::Disabled}}});

    static const std::array appearances{defaults.normal,   defaults.hovered,    defaults.pressed, defaults.sorted,
                                        defaults.selected, defaults.dragTarget, defaults.disabled};

    const auto& states = parts().back().states;
    for(size_t index{0}; index < states.size(); ++index) {
        const auto& name  = states[index].pageId;
        const auto& state = appearances[index];

        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);
        addBrush(page, tr("Background"), name, u"backgroundColour"_s, state.backgroundColour, group);

        auto* opacity = addNumber(page, tr("Background opacity"), name, u"backgroundOpacity"_s, state.backgroundOpacity,
                                  0, 100, 0);
        opacity->suffix = tr(" %");

        addColour(page, tr("Background tint"), name, u"tintColour"_s, state.tintColour, group);

        auto* tint   = addNumber(page, tr("Tint opacity"), name, u"tintOpacity"_s, state.tintOpacity, 0, 100, 0);
        tint->suffix = tr(" %");

        addColour(page, tr("Text colour"), name, u"foregroundColour"_s, state.foregroundColour, group);
        addBool(page, tr("Tint icon"), name, u"tintIcon"_s, state.tintIcon);
        addColour(page, tr("Icon colour"), name, u"iconColour"_s, state.iconColour, group);
    }

    auto* border = addPartPage(u"border"_s, tr("Border"));
    addNumber(border, tr("Width"), u"border"_s, u"width"_s, defaults.border.width, 0, 32, 2);
    addColour(border, tr("Colour"), u"border"_s, u"colour"_s, defaults.border.colour);
    auto* borderOpacity
        = addNumber(border, tr("Opacity"), u"border"_s, u"opacity"_s, defaults.border.opacity, 0, 100, 0);
    borderOpacity->suffix = tr(" %");
    auto* shading         = addNumber(border, tr("Border shading"), u"border"_s, u"shadingOpacity"_s,
                                      defaults.border.shadingOpacity, 0, 100, 0);
    shading->suffix       = tr(" %");
    border->addHint(new HintLabel(tr("The border runs along the edge facing the view contents. Zero width hides it."),
                                  border->parentWidget()));

    auto* separator = addPartPage(u"separator"_s, tr("Section separators"));
    addNumber(separator, tr("Width"), u"separator"_s, u"width"_s, defaults.separator.width, 0, 32, 2);
    addNumber(separator, tr("Inset"), u"separator"_s, u"inset"_s, defaults.separator.inset, 0, 128, 2);
    addColour(separator, tr("Colour"), u"separator"_s, u"colour"_s, defaults.separator.colour);
    auto* separatorOpacity
        = addNumber(separator, tr("Opacity"), u"separator"_s, u"opacity"_s, defaults.separator.opacity, 0, 100, 0);
    separatorOpacity->suffix = tr(" %");

    auto* arrow = addPartPage(u"arrow"_s, tr("Sort indicator"));
    addNumber(arrow, tr("Reserved size"), u"arrow"_s, u"extent"_s, defaults.arrow.extent, 0, 128, 0);
    addNumber(arrow, tr("Arrow size"), u"arrow"_s, u"size"_s, defaults.arrow.size, 0, 128, 2);
    addNumber(arrow, tr("Line width"), u"arrow"_s, u"width"_s, defaults.arrow.width, 0, 32, 2);
    addColour(arrow, tr("Colour"), u"arrow"_s, u"colour"_s, defaults.arrow.colour);

    auto* focus = addPartPage(u"focus"_s, tr("Focus and drag outline"), {}, PreviewState::Focused);
    addNumber(focus, tr("Line width"), u"focus"_s, u"width"_s, defaults.focus.width, 0, 32, 2);
    addNumber(focus, tr("Inset"), u"focus"_s, u"inset"_s, defaults.focus.inset, 0, 128, 2);
    addNumber(focus, tr("Corner radius"), u"focus"_s, u"radius"_s, defaults.focus.radius, 0, 128, 2);
    addColour(focus, tr("Colour"), u"focus"_s, u"colour"_s, defaults.focus.colour);
    addBool(focus, tr("Keyboard focus only"), u"focus"_s, u"keyboardOnly"_s, defaults.focus.keyboardOnly);

    preparePreview(m_table);
}

HeaderAppearanceEditor::~HeaderAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

bool HeaderAppearanceEditor::selectedPreview() const
{
    return m_selectedPreview;
}

bool HeaderAppearanceEditor::dragTargetPreview() const
{
    return m_dragTargetPreview;
}

bool HeaderAppearanceEditor::sortedPreview() const
{
    return m_sortedPreview;
}

void HeaderAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    if(updateState) {
        m_selectedPreview   = part == u"selected"_s;
        m_dragTargetPreview = part == u"dragTarget"_s;
        m_sortedPreview     = part == u"sorted"_s || part == u"arrow"_s;

        if(part == u"sorted"_s || part == u"arrow"_s) {
            for(auto* header : {m_table->horizontalHeader(), m_table->verticalHeader()}) {
                header->setSortIndicatorShown(true);
                header->setSortIndicator(header->sortIndicatorSection(), header->sortIndicatorOrder());
            }
        }

        requestPreviewState(page);
    }
}

bool HeaderAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        for(auto* header : {m_table->horizontalHeader(), m_table->verticalHeader()}) {
            if(watched == header->viewport()
               && header->logicalIndexAt(static_cast<QMouseEvent*>(event)->position().toPoint()) >= 0) {
                const auto state = previewState();
                static_cast<HeaderPreview*>(header)->selectSection(
                    header->logicalIndexAt(static_cast<QMouseEvent*>(event)->position().toPoint()));
                Q_EMIT partSelected(state == PreviewState::Disabled  ? u"disabled"_s
                                    : state == PreviewState::Pressed ? u"pressed"_s
                                    : state == PreviewState::Hovered ? u"hovered"_s
                                    : m_dragTargetPreview            ? u"dragTarget"_s
                                    : state == PreviewState::Focused ? u"focus"_s
                                    : m_selectedPreview              ? u"selected"_s
                                    : m_sortedPreview                ? u"sorted"_s
                                                                     : u"normal"_s);
                break;
            }
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void HeaderAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_table->setPalette(palette);
    m_table->setFont(font);
    refreshPreview();
}

void HeaderAppearanceEditor::updatePreview()
{
    const auto resolved = resolveHeaderAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Header appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().header == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.header = *resolved});
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void HeaderAppearanceEditor::updatePreviewState(PreviewState state)
{
    m_table->setEnabled(state != PreviewState::Disabled);
    refreshPreview();
}

void HeaderAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    m_table->setStyle(style);

    const auto children = m_table->findChildren<QWidget*>();
    for(auto* child : children) {
        child->setStyle(style);
    }

    refreshPreview();
}

void HeaderAppearanceEditor::refreshPreview()
{
    for(auto* header : {m_table->horizontalHeader(), m_table->verticalHeader()}) {
        for(int section{0}; section < header->count(); ++section) {
            const int size = header->sectionSizeHint(section);
            if(header->sectionSize(section) < size) {
                header->resizeSection(section, size);
            }
        }
    }

    m_table->horizontalHeader()->updateGeometry();
    m_table->verticalHeader()->updateGeometry();
    m_table->horizontalHeader()->viewport()->update();
    m_table->verticalHeader()->viewport()->update();
    m_table->updateGeometry();
}
} // namespace Fooyin
