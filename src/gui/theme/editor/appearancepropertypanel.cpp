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

#include "appearancepropertypanel.h"

#include "appearancepropertydelegate.h"
#include "appearancepropertypage.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

namespace Fooyin {
class AppearancePropertyView : public QTreeView
{
public:
    explicit AppearancePropertyView(QWidget* parent)
        : QTreeView{parent}
    {
        setItemDelegate(new AppearancePropertyDelegate(this));
        setUniformRowHeights(true);
        setWordWrap(false);
        setTextElideMode(Qt::ElideRight);
        setFrameShape(NoFrame);
        setSelectionBehavior(SelectItems);
        setEditTriggers(DoubleClicked | EditKeyPressed | AnyKeyPressed);
        header()->setSectionsMovable(false);
        header()->setStretchLastSection(false);

        QObject::connect(this, &QTreeView::clicked, this, [this](const QModelIndex& index) {
            if(index.column() == 1 && (index.flags() & Qt::ItemIsEditable) && !indexWidget(index)) {
                edit(index);
            }
        });
    }

    void finishEditing()
    {
        if(state() == EditingState) {
            if(const QPointer editor{indexWidget(currentIndex())}) {
                commitData(editor);
                if(editor) {
                    // The shared delegate outlives this editor, which Qt deletes later
                    auto* delegate = itemDelegateForIndex(currentIndex());
                    editor->removeEventFilter(delegate);
                    const auto children = editor->findChildren<QWidget*>();
                    for(auto* child : children) {
                        child->removeEventFilter(delegate);
                        QObject::disconnect(child, nullptr, delegate, nullptr);
                    }
                    closeEditor(editor, QAbstractItemDelegate::NoHint);
                }
            }
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if(event->key() == Qt::Key_Delete && currentIndex().isValid()) {
            model()->setData(currentIndex().siblingAtColumn(1), {}, AppearancePropertyModel::ResetRole);
            event->accept();
            return;
        }
        QTreeView::keyPressEvent(event);
    }
};

AppearancePropertyPanel::AppearancePropertyPanel(AppearancePropertyModel* model, QWidget* parent)
    : QWidget{parent}
    , m_view{new AppearancePropertyView(this)}
    , m_layout{new QVBoxLayout(this)}
    , m_page{nullptr}
{
    m_view->setModel(model);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_view->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_view->header()->resizeSection(0, 170);
    m_view->hide();

    m_layout->setContentsMargins({});
    m_layout->addWidget(m_view, 1);
}

void AppearancePropertyPanel::setPage(AppearancePropertyPage* page)
{
    if(m_page == page) {
        return;
    }

    m_view->finishEditing();
    saveViewState();

    if(m_page) {
        m_layout->removeWidget(m_page);
        m_page->hide();
    }
    m_page = page;

    m_view->selectionModel()->clear();
    m_view->setRootIndex(page ? page->rootIndex() : QModelIndex{});
    m_view->setVisible(page != nullptr);

    if(page) {
        m_layout->addWidget(page);
        page->setVisible(page->hasHints());
    }

    m_layout->activate();
    restoreViewState();
}

QTreeView* AppearancePropertyPanel::view() const
{
    return m_view;
}

void AppearancePropertyPanel::saveViewState()
{
    if(!m_page) {
        return;
    }

    auto& state            = m_page->m_viewState;
    state.current          = m_view->currentIndex();
    state.selection        = m_view->selectionModel()->selection();
    state.horizontalScroll = m_view->horizontalScrollBar()->value();
    state.verticalScroll   = m_view->verticalScrollBar()->value();
    state.expanded.clear();

    const auto visit = [&](auto&& self, const QModelIndex& parent) -> void {
        for(int row{0}; row < m_view->model()->rowCount(parent); ++row) {
            const auto index = m_view->model()->index(row, 0, parent);
            if(m_view->isExpanded(index)) {
                state.expanded.emplace_back(index);
            }
            self(self, index);
        }
    };

    visit(visit, m_view->rootIndex());
}

void AppearancePropertyPanel::restoreViewState()
{
    bool expandable{false};
    if(m_page) {
        const auto* model = m_view->model();
        const auto root   = m_view->rootIndex();
        for(int row{0}; row < model->rowCount(root); ++row) {
            if(model->hasChildren(model->index(row, 0, root))) {
                expandable = true;
                break;
            }
        }

        m_view->setRootIsDecorated(expandable);
        m_view->setItemsExpandable(expandable);

        const auto& state = m_page->m_viewState;
        for(const auto& index : state.expanded) {
            if(index.isValid()) {
                m_view->expand(index);
            }
        }

        m_view->selectionModel()->setCurrentIndex(state.current, QItemSelectionModel::NoUpdate);
        m_view->selectionModel()->select(state.selection, QItemSelectionModel::ClearAndSelect);
        m_view->doItemsLayout();
        m_view->horizontalScrollBar()->setValue(state.horizontalScroll);
        m_view->verticalScrollBar()->setValue(state.verticalScroll);
    }
}
} // namespace Fooyin
