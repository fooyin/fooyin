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

#include "itemviewappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "style/itemviewstyle.h"
#include "theme/itemviewappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QListView>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class ItemPreviewDelegate : public QStyledItemDelegate
{
public:
    ItemPreviewDelegate(ItemViewAppearanceEditor* editor, QObject* parent)
        : QStyledItemDelegate{parent}
        , m_editor{editor}
    { }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto item{option};
        initStyleOption(&item, index);
        if(m_editor) {
            m_editor->prepareItemOption(item);
        }
        item.widget->style()->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
    }

private:
    QPointer<ItemViewAppearanceEditor> m_editor;
};

class ItemPreviewStyle : public FooyinStyle
{
public:
    ItemPreviewStyle(const ItemViewAppearance& appearance, ItemViewAppearanceEditor* editor)
        : FooyinStyle{ThemeAppearance{.itemView = appearance}}
        , m_editor{editor}
    { }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if(element == PE_IndicatorBranch && option && painter && m_editor
           && m_editor->previewState() != PreviewState::Normal) {
            auto branch{*option};
            branch.type    = QStyleOption::SO_Default;
            branch.version = QStyleOption::Version;
            branch.state.setFlag(State_MouseOver, m_editor->previewState() == PreviewState::Hovered);

            Style::ItemView::drawBranch(branch, *painter, nullptr, appearance().itemView);
            return;
        }

        FooyinStyle::drawPrimitive(element, option, painter, widget);
    }

private:
    QPointer<ItemViewAppearanceEditor> m_editor;
};
} // namespace

ItemViewAppearanceEditor::ItemViewAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"itemView"_s, tr("Item views")}
    , m_previewStyle{std::make_unique<ItemPreviewStyle>(ItemViewAppearance{}, this)}
    , m_views{new QStackedWidget(previewWidget())}
    , m_itemViews{new QTreeView(m_views), new QListView(m_views), new QTableView(m_views)}
    , m_surfaceState{u"normal"_s}
    , m_selectedPreview{false}
    , m_pickingItem{false}
{
    auto* model = new QStandardItemModel(m_views);
    model->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Time")});

    auto* album = new QStandardItem(tr("Example album"));
    model->appendRow({album, new QStandardItem(tr("Example artist")), new QStandardItem(u"2026"_s)});

    const std::array tracks{tr("Northern lights"), tr("After hours"), tr("Soft focus"), tr("Sundown"),
                            tr("Custom background")};

    for(size_t row{0}; row < tracks.size(); ++row) {
        auto* track = new QStandardItem(tracks[row]);
        track->setIcon(m_previewStyle->standardIcon(QStyle::SP_FileIcon));
        album->appendRow({track, new QStandardItem(tr("Example artist")), new QStandardItem(u"03:24"_s)});
    }

    m_activeIndex = model->index(0, 0, album->index());

    for(auto* view : m_itemViews) {
        view->setModel(model);
        view->setItemDelegate(new ItemPreviewDelegate(this, view));
        view->setAlternatingRowColors(true);
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_views->addWidget(view);
        preparePreview(view);
    }

    auto* tree = static_cast<QTreeView*>(m_itemViews[0]);
    tree->expandAll();
    tree->header()->setSectionResizeMode(QHeaderView::Stretch);

    auto* list = static_cast<QListView*>(m_itemViews[1]);
    list->setRootIndex(album->index());

    auto* table = static_cast<QTableView*>(m_itemViews[2]);
    table->setRootIndex(album->index());
    table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto* type = new QComboBox(previewWidget());
    type->addItems({tr("Tree"), tr("List"), tr("Table")});

    QObject::connect(type, &QComboBox::currentIndexChanged, m_views, &QStackedWidget::setCurrentIndex);
    QObject::connect(m_views, &QStackedWidget::currentChanged, type, &QComboBox::setCurrentIndex);
    QObject::connect(m_views, &QStackedWidget::currentChanged, this, [this] {
        updateActiveIndex();
        refreshPreview();
    });

    auto* selection = new QComboBox(previewWidget());
    selection->addItems({tr("Rows"), tr("Items")});

    QObject::connect(selection, &QComboBox::currentIndexChanged, this, [this](int index) {
        for(auto* view : m_itemViews) {
            view->setSelectionBehavior(index == 0 ? QAbstractItemView::SelectRows : QAbstractItemView::SelectItems);
        }
        refreshPreview();
    });

    auto* controls = new QFormLayout();
    controls->addRow(tr("View type"), type);
    controls->addRow(tr("Selection behaviour"), selection);

    auto* previewLayout = new QVBoxLayout(previewWidget());
    previewLayout->setContentsMargins({});
    previewLayout->addLayout(controls);
    previewLayout->addWidget(m_views, 1);

    applyPreviewStyle(m_previewStyle.get());

    const ItemViewAppearance defaults;

    auto* dimensions = addPartPage({}, {});
    addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);
    addNumber(dimensions, tr("Text padding"), {}, u"textPadding"_s, defaults.textPadding, 0, 128, 0);
    addNumber(dimensions, tr("Tree text padding"), {}, u"treeTextPadding"_s, defaults.treeTextPadding, 0, 128, 0);
    addNumber(dimensions, tr("Horizontal inset"), {}, u"horizontalInset"_s, defaults.horizontalInset, 0, 128, 2);
    addNumber(dimensions, tr("Vertical inset"), {}, u"verticalInset"_s, defaults.verticalInset, 0, 128, 2);
    addNumber(dimensions, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    addNumber(dimensions, tr("Custom background border width"), {}, u"customBorderWidth"_s, defaults.customBorderWidth,
              0, 32, 2);
    dimensions->addHint(new HintLabel(tr("Vertical padding is added above and below the contents. "
                                         "Tree text padding adds space at the outer text edges of tree rows. "
                                         "Widgets with explicit row sizes keep those sizes."),
                                      dimensions));

    addPart(
        {.id       = u"surface"_s,
         .title    = tr("Surface"),
         .parentId = {},
         .states
         = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
            {.id           = u"alternate"_s,
             .title        = tr("Alternate"),
             .pageId       = u"alternate"_s,
             .previewState = PreviewState::Normal},
            {.id = u"hovered"_s, .title = tr("Hovered"), .pageId = u"hovered"_s, .previewState = PreviewState::Hovered},
            {.id           = u"selected"_s,
             .title        = tr("Selected"),
             .pageId       = u"selected"_s,
             .previewState = PreviewState::Normal},
            {.id                = u"selectedHovered"_s,
             .title             = tr("Selected and hovered"),
             .pageId            = u"selectedHovered"_s,
             .previewState      = PreviewState::Hovered,
             .inheritanceGroups = {u"selected"_s, u"normal"_s}}}});

    static const std::array appearances{defaults.normal, defaults.alternate, defaults.hovered, defaults.selected,
                                        defaults.selectedHovered};

    const auto& states = parts().back().states;
    for(size_t index{0}; index < states.size(); ++index) {
        const auto& name  = states[index].pageId;
        const auto& state = appearances[index];

        auto* page = addPage(name);
        addBrush(page, tr("Background"), name, u"backgroundColour"_s, state.backgroundColour);

        auto* opacity = addNumber(page, tr("Background opacity"), name, u"backgroundOpacity"_s, state.backgroundOpacity,
                                  0, 100, 0);
        opacity->suffix = tr(" %");

        auto* lightness   = addNumber(page, tr("Colour lightness"), name, u"backgroundLightness"_s,
                                      state.backgroundLightness, 1, 300, 0);
        lightness->suffix = tr(" %");

        addNumber(page, tr("Border width"), name, u"borderWidth"_s, state.borderWidth, 0, 32, 2);
        addColour(page, tr("Border colour"), name, u"borderColour"_s, state.borderColour);

        auto* borderOpacity
            = addNumber(page, tr("Border opacity"), name, u"borderOpacity"_s, state.borderOpacity, 0, 100, 0);
        borderOpacity->suffix = tr(" %");
    }

    auto* focus = addPartPage(u"focus"_s, tr("Focus"), {}, PreviewState::Focused);
    addNumber(focus, tr("Line width"), u"focus"_s, u"width"_s, defaults.focus.width, 0, 32, 2);
    addNumber(focus, tr("Additional inset"), u"focus"_s, u"inset"_s, defaults.focus.inset, 0, 128, 2);
    addColour(focus, tr("Colour"), u"focus"_s, u"colour"_s, defaults.focus.colour);
    auto* opacity   = addNumber(focus, tr("Opacity"), u"focus"_s, u"opacity"_s, defaults.focus.opacity, 0, 100, 0);
    opacity->suffix = tr(" %");
    auto* selectedOpacity     = addNumber(focus, tr("Selected opacity"), u"focus"_s, u"selectedOpacity"_s,
                                          defaults.focus.selectedOpacity, 0, 100, 0);
    selectedOpacity->suffix   = tr(" %");
    auto* selectedLightness   = addNumber(focus, tr("Selected colour lightness"), u"focus"_s, u"selectedLightness"_s,
                                          defaults.focus.selectedLightness, 1, 300, 0);
    selectedLightness->suffix = tr(" %");
    addBool(focus, tr("Keyboard focus only"), u"focus"_s, u"keyboardOnly"_s, defaults.focus.keyboardOnly);

    auto* branch = addPartPage(u"branch"_s, tr("Tree branches"));
    addNumber(branch, tr("Line width"), u"branch"_s, u"lineWidth"_s, defaults.branch.lineWidth, 0, 32, 2);
    addColour(branch, tr("Line colour"), u"branch"_s, u"lineColour"_s, defaults.branch.lineColour);
    auto* lineOpacity
        = addNumber(branch, tr("Line opacity"), u"branch"_s, u"lineOpacity"_s, defaults.branch.lineOpacity, 0, 100, 0);
    lineOpacity->suffix = tr(" %");
    addColour(branch, tr("Selected line colour"), u"branch"_s, u"selectedLineColour"_s,
              defaults.branch.selectedLineColour);
    auto* selectedLineOpacity   = addNumber(branch, tr("Selected line opacity"), u"branch"_s, u"selectedLineOpacity"_s,
                                            defaults.branch.selectedLineOpacity, 0, 100, 0);
    selectedLineOpacity->suffix = tr(" %");
    auto* filledArrows
        = addBool(branch, tr("Fill arrows"), u"branch"_s, u"filledArrows"_s, defaults.branch.filledArrows);
    filledArrows->toolTip = tr("Draw arrows as solid triangles");
    addNumber(branch, tr("Arrow reserved size"), u"branch"_s, u"arrowExtent"_s, defaults.branch.arrowExtent, 0, 128, 0);
    addNumber(branch, tr("Arrow size"), u"branch"_s, u"arrowSize"_s, defaults.branch.arrowSize, 0, 128, 2);
    addNumber(branch, tr("Arrow line width"), u"branch"_s, u"arrowWidth"_s, defaults.branch.arrowWidth, 0, 32, 2);
    addColour(branch, tr("Arrow colour"), u"branch"_s, u"arrowColour"_s, defaults.branch.arrowColour);
    addColour(branch, tr("Hovered arrow colour"), u"branch"_s, u"hoveredArrowColour"_s,
              defaults.branch.hoveredArrowColour);
    addColour(branch, tr("Selected arrow colour"), u"branch"_s, u"selectedArrowColour"_s,
              defaults.branch.selectedArrowColour);
}

ItemViewAppearanceEditor::~ItemViewAppearanceEditor()
{
    applyPreviewStyle(nullptr);
}

void ItemViewAppearanceEditor::prepareItemOption(QStyleOptionViewItem& option) const
{
    if(previewState() == PreviewState::Normal && !m_selectedPreview) {
        return;
    }

    const auto* view   = qobject_cast<const QAbstractItemView*>(option.widget);
    const bool current = option.index == m_activeIndex;
    const bool active
        = current
       || (view && view->selectionBehavior() == QAbstractItemView::SelectRows
           && option.index.row() == m_activeIndex.row() && option.index.parent() == m_activeIndex.parent());
    option.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus
                      | QStyle::State_KeyboardFocusChange);
    if(!active) {
        return;
    }

    option.state.setFlag(QStyle::State_Selected, m_selectedPreview);

    if((previewState() == PreviewState::Hovered || previewState() == PreviewState::Pressed)
       && (current || !qobject_cast<const QTableView*>(view))) {
        option.state |= QStyle::State_MouseOver;
    }
    if(current && previewState() == PreviewState::Focused) {
        option.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
    }
}

void ItemViewAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }

    if(updateState) {
        if(part == u"branch"_s) {
            m_views->setCurrentIndex(0);
        }
        if(part == u"normal"_s || part == u"alternate"_s || part == u"hovered"_s || part == u"selected"_s
           || part == u"selectedHovered"_s) {
            m_surfaceState    = part;
            m_selectedPreview = part == u"selected"_s || part == u"selectedHovered"_s;

            if(!m_pickingItem) {
                updateActiveIndex();
            }
        }
        requestPreviewState(page);
    }

    showPage(page);
}

bool ItemViewAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        for(auto* view : m_itemViews) {
            if(watched != view->viewport()) {
                continue;
            }

            const auto position = static_cast<QMouseEvent*>(event)->position().toPoint();
            const auto index    = view->indexAt(position);
            if(index.isValid()) {
                const QRect rect  = view->visualRect(index);
                const bool branch = view == m_itemViews[0]
                                 && (view->layoutDirection() == Qt::RightToLeft ? position.x() > rect.right()
                                                                                : position.x() < rect.left());
                if(branch) {
                    Q_EMIT partSelected(u"branch"_s);
                }
                else {
                    m_activeIndex = index;
                    m_pickingItem = true;
                    Q_EMIT partSelected(activeStatePart());
                    m_pickingItem = false;
                    refreshPreview();
                }
            }
            break;
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void ItemViewAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    for(auto* view : m_itemViews) {
        view->setPalette(palette);
        view->setFont(font);
    }

    auto colour = palette.color(QPalette::Highlight);
    colour.setAlphaF(0.25F);

    const auto index = m_itemViews[1]->model()->index(4, 0, m_itemViews[1]->rootIndex());
    for(int column{0}; column < 3; ++column) {
        m_itemViews[1]->model()->setData(index.siblingAtColumn(column), colour, Qt::BackgroundRole);
    }

    refreshPreview();
}

void ItemViewAppearanceEditor::updatePreview()
{
    const auto resolved = resolveItemViewAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Item-view appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().itemView == *resolved) {
        return;
    }

    auto style = std::make_unique<ItemPreviewStyle>(*resolved, this);
    applyPreviewStyle(style.get());
    m_previewStyle = std::move(style);
}

void ItemViewAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* view : m_itemViews) {
        view->setEnabled(state != PreviewState::Disabled);
    }
    refreshPreview();
}

void ItemViewAppearanceEditor::applyPreviewStyle(QStyle* style)
{
    for(auto* view : m_itemViews) {
        view->setStyle(style);

        const auto children = view->findChildren<QWidget*>();
        for(auto* child : children) {
            child->setStyle(style);
        }
    }
    refreshPreview();
}

void ItemViewAppearanceEditor::refreshPreview()
{
    for(auto* view : m_itemViews) {
        view->doItemsLayout();
        view->viewport()->update();
    }
    static_cast<QTableView*>(m_itemViews[2])->resizeRowsToContents();
}

void ItemViewAppearanceEditor::updateActiveIndex()
{
    const auto parent    = m_itemViews[1]->rootIndex();
    const bool alternate = m_surfaceState == u"alternate"_s;
    const bool tree      = m_views->currentIndex() == 0;
    const int row        = m_surfaceState == u"normal"_s || alternate ? (tree == alternate ? 0 : 1) : 0;
    m_activeIndex        = parent.model()->index(row, 0, parent);
}

QString ItemViewAppearanceEditor::activeStatePart() const
{
    if(previewState() == PreviewState::Focused) {
        return u"focus"_s;
    }

    const bool hovered = previewState() == PreviewState::Hovered || previewState() == PreviewState::Pressed;
    const bool alternate
        = (m_activeIndex.row() + (m_views->currentIndex() == 0 && m_activeIndex.parent().isValid() ? 1 : 0)) % 2;
    return m_selectedPreview ? (hovered ? u"selectedHovered"_s : u"selected"_s)
         : hovered           ? u"hovered"_s
         : alternate         ? u"alternate"_s
                             : u"normal"_s;
}
} // namespace Fooyin
