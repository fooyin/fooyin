/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "tageditorview.h"

#include "tageditoritem.h"
#include "tageditormodel.h"

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/stardelegate.h>
#include <utils/stareditor.h>

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QTimer>

#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
TagEditorView::TagEditorView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{parent}
    , m_actionManager{actionManager}
    , m_editTrigger{AllEditTriggers}
    , m_context{new WidgetContext(this, Context{Id{"Context.TagEditor."}.append(reinterpret_cast<uintptr_t>(this))},
                                  this)}
    , m_capitaliseAction{new QAction(tr("Capitalise"), this)}
    , m_copyAction{new QAction(tr("Copy"), this)}
    , m_copyCmd{m_actionManager->registerAction(m_copyAction, Constants::Actions::Copy, m_context->context())}
    , m_pasteAction{new QAction(tr("Paste"), this)}
    , m_pasteCmd{m_actionManager->registerAction(m_pasteAction, Constants::Actions::Paste, m_context->context())}
    , m_pasteFields{new QAction(tr("Paste fields"), this)}
    , m_ratingRow{-1}
    , m_starDelegate{nullptr}
{
    actionManager->addContextObject(m_context);

    setTextElideMode(Qt::ElideRight);
    setMouseTracking(true);
    horizontalHeader()->setSectionsClickable(false);
    verticalHeader()->setVisible(false);
}

void TagEditorView::setTagEditTriggers(EditTriggers triggers)
{
    m_editTrigger = triggers;
    setEditTriggers(triggers & EditKeyPressed);
}

void TagEditorView::setupActions()
{
    QObject::connect(m_capitaliseAction, &QAction::triggered, this, &TagEditorView::capitaliseSelection);

    m_copyCmd->setDefaultShortcut(QKeySequence::Copy);
    m_copyAction->setVisible(selectionModel()->hasSelection());
    QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_copyAction->setVisible(selectionModel()->hasSelection()); });
    QObject::connect(m_copyAction, &QAction::triggered, this, &TagEditorView::copySelection);

    m_pasteCmd->setDefaultShortcut(QKeySequence::Paste);
    m_pasteAction->setVisible(selectionModel()->hasSelection());
    m_pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_pasteAction->setVisible(selectionModel()->hasSelection()); });
    QObject::connect(QApplication::clipboard(), &QClipboard::changed, this,
                     [this]() { m_pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty()); });
    QObject::connect(m_pasteAction, &QAction::triggered, this, [this]() { pasteSelection(false); });

    m_pasteFields->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
    QObject::connect(QApplication::clipboard(), &QClipboard::changed, this,
                     [this]() { m_pasteFields->setEnabled(QApplication::clipboard()->text().contains(" : "_L1)); });
    QObject::connect(m_pasteFields, &QAction::triggered, this, [this]() { pasteSelection(true); });
    m_pasteFields->setEnabled(QApplication::clipboard()->text().contains(" : "_L1));
}

void TagEditorView::setRatingRow(int row)
{
    m_ratingRow = row;
    if(row >= 0) {
        m_starDelegate = qobject_cast<StarDelegate*>(itemDelegateForRow(row));
    }
    else {
        m_starDelegate = nullptr;
    }
}

int TagEditorView::sizeHintForRow(int row) const
{
    if(!model()->hasIndex(row, 0, {})) {
        return 0;
    }

    // Only resize rows based on first column
    return model()->index(row, 0, {}).data(Qt::SizeHintRole).toSize().height();
}

void TagEditorView::setupContextActions(QMenu* menu, const QPoint& /*pos*/)
{
    menu->addAction(m_copyCmd->action());
    menu->addAction(m_pasteCmd->action());
    menu->addAction(m_pasteFields);
    menu->addSeparator();
    m_capitaliseAction->setEnabled(canCapitaliseSelection());
    menu->addAction(m_capitaliseAction);
}

void TagEditorView::mouseMoveEvent(QMouseEvent* event)
{
    if(m_starDelegate) {
        const QModelIndex index = indexAt(event->pos());
        if(index.isValid() && index.row() == m_ratingRow && index.column() == 1) {
            ratingHoverIn(index, event->pos());
        }
        else if(m_starDelegate->hoveredIndex().isValid()) {
            ratingHoverOut();
        }
    }

    ExtendableTableView::mouseMoveEvent(event);
}

void TagEditorView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());

    if(event->button() == Qt::LeftButton) {
        if(!index.isValid() || index.column() != 1) {
            ExtendableTableView::mousePressEvent(event);
            return;
        }

        if(!index.data().canConvert<StarRating>()) {
            ExtendableTableView::mousePressEvent(event);
            return;
        }

        auto starRating   = qvariant_cast<StarRating>(index.data());
        const auto rating = StarEditor::ratingAtPosition(event->pos(), visualRect(index), starRating);
        starRating.setRating(rating);

        model()->setData(index, QVariant::fromValue(starRating));
    }

    ExtendableTableView::mousePressEvent(event);
}

void TagEditorView::mouseReleaseEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());

    ExtendableTableView::mouseReleaseEvent(event);

    if(event->button() == Qt::LeftButton) {
        reopenEditor(index);
    }
}

void TagEditorView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());

    ExtendableTableView::mouseDoubleClickEvent(event);

    if(event->button() == Qt::LeftButton && index.isValid() && index.row() != m_ratingRow) {
        reopenEditor(index);
    }
}

void TagEditorView::keyPressEvent(QKeyEvent* event)
{
    static constexpr QKeyCombination pasteSeq{Qt::CTRL | Qt::SHIFT | Qt::Key_V};

    if(event->keyCombination() == pasteSeq) {
        m_pasteFields->trigger();
        return;
    }
    if(event->key() == Qt::Key_F2 && event->modifiers() == Qt::NoModifier) {
        const QModelIndex editIndex = editableIndexFor(currentIndex());
        if(editIndex.isValid()) {
            setCurrentIndex(editIndex);
            edit(editIndex);
            event->accept();
            return;
        }
    }

    ExtendableTableView::keyPressEvent(event);
}

void TagEditorView::leaveEvent(QEvent* event)
{
    if(m_starDelegate && m_starDelegate->hoveredIndex().isValid()) {
        ratingHoverOut();
    }

    ExtendableTableView::leaveEvent(event);
}

void TagEditorView::reopenEditor(const QModelIndex& index)
{
    if(m_editTrigger == NoEditTriggers || !index.isValid() || index.row() == m_ratingRow
       || (model()->flags(index) & Qt::ItemIsEditable) == 0) {
        return;
    }

    QTimer::singleShot(0, this, [this, index]() {
        if(state() != EditingState && currentIndex() == index) {
            edit(index);
        }
    });
}

QModelIndex TagEditorView::editableIndexFor(const QModelIndex& index) const
{
    if(m_editTrigger == NoEditTriggers || !model() || !index.isValid() || index.row() == m_ratingRow) {
        return {};
    }

    if((model()->flags(index) & Qt::ItemIsEditable) != 0) {
        return index;
    }

    const QModelIndex valueIndex = index.siblingAtColumn(1);
    if(!valueIndex.isValid() || (model()->flags(valueIndex) & Qt::ItemIsEditable) == 0) {
        return {};
    }

    return valueIndex;
}

QModelIndexList TagEditorView::selectedRows() const
{
    if(!model() || !selectionModel()) {
        return {};
    }

    std::set<int> rows;

    const auto selectedIndexes = selectionModel()->selectedIndexes();
    for(const QModelIndex& index : selectedIndexes) {
        if(index.isValid()) {
            rows.emplace(index.row());
        }
    }

    if(rows.empty() && currentIndex().isValid()) {
        rows.emplace(currentIndex().row());
    }

    QModelIndexList indexes;
    for(const int row : rows) {
        const QModelIndex index = model()->index(row, 0);
        if(index.isValid()) {
            indexes.emplace_back(index);
        }
    }
    return indexes;
}

void TagEditorView::copySelection()
{
    const auto selected = selectedRows();
    if(selected.empty()) {
        return;
    }

    QStringList fields;
    for(const auto& index : selected) {
        const QString value = index.siblingAtColumn(1).data(TagEditorItem::Title).toString();
        const QString field = index.siblingAtColumn(0).data(TagEditorItem::Title).toString();
        fields.emplace_back(field + " : "_L1 + value);
    }

    QApplication::clipboard()->setText(fields.join("\n\r"_L1));
}

void TagEditorView::pasteSelection(bool match)
{
    const auto selected = selectedRows();
    if(!match && selected.empty()) {
        return;
    }

    std::map<QString, QString> values;

    const QString text      = QApplication::clipboard()->text();
    const QStringList pairs = text.split(u"\n\r"_s, Qt::SkipEmptyParts);
    const QString delimiter = u" : "_s;

    for(int i{0}; const QString& pair : pairs) {
        if(pair.contains(delimiter)) {
            const auto delIndex = pair.indexOf(delimiter);
            if(delIndex < 0) {
                continue;
            }
            const QString tag   = pair.left(delIndex);
            const QString value = pair.sliced(delIndex + delimiter.length());

            if(!match && std::cmp_less(i, selected.size())) {
                values.emplace(selected.at(i++).data().toString(), value);
            }
            else {
                values.emplace(tag, value);
            }
        }
        else if(!match && std::cmp_less(i, selected.size())) {
            values.emplace(selected.at(i++).data().toString(), pair);
        }
    }

    if(auto* tagModel = qobject_cast<TagEditorModel*>(model())) {
        tagModel->updateValues(values, match);
    }
}

bool TagEditorView::canCapitaliseSelection() const
{
    if(m_editTrigger == NoEditTriggers || !selectionModel()) {
        return false;
    }

    const auto rows = selectedRows();
    return std::ranges::any_of(rows, [this](const QModelIndex& index) {
        if(!index.isValid() || index.row() == m_ratingRow) {
            return false;
        }

        const QModelIndex valueIndex = index.siblingAtColumn(1);
        return valueIndex.isValid() && (model()->flags(valueIndex) & Qt::ItemIsEditable) != 0;
    });
}

void TagEditorView::capitaliseSelection()
{
    if(!canCapitaliseSelection()) {
        return;
    }

    if(auto* tagModel = qobject_cast<TagEditorModel*>(model())) {
        tagModel->capitaliseRows(selectedRows());
    }
}

void TagEditorView::ratingHoverIn(const QModelIndex& index, const QPoint& pos)
{
    const QModelIndexList selected = selectedIndexes();
    const QModelIndex prevIndex    = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex(index, pos, selected);
    setCursor(Qt::PointingHandCursor);

    update(prevIndex);
    update(index);
}

void TagEditorView::ratingHoverOut()
{
    const QModelIndex prevIndex = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex({});
    setCursor({});

    update(prevIndex);
}
} // namespace Fooyin::TagEditor
