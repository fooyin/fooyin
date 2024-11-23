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
#include <gui/widgets/multilinedelegate.h>
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

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
TagEditorView::TagEditorView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, parent}
    , m_actionManager{actionManager}
    , m_editTrigger{EditTrigger::AllEditTriggers}
    , m_context{new WidgetContext(this, Context{"Context.TagEditor"}, this)}
    , m_copyAction{new QAction(tr("Copy"), this)}
    , m_pasteAction{new QAction(tr("Paste"), this)}
    , m_pasteFields{new QAction(tr("Paste fields"), this)}
    , m_ratingRow{-1}
    , m_starDelegate{nullptr}
{
    actionManager->addContextObject(m_context);

    setTextElideMode(Qt::ElideRight);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setMouseTracking(true);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsClickable(false);
    verticalHeader()->setVisible(false);
}

void TagEditorView::setTagEditTriggers(EditTrigger triggers)
{
    m_editTrigger = triggers;
    setEditTriggers(triggers);
}

void TagEditorView::setupActions()
{
    m_copyAction->setShortcut(QKeySequence::Copy);
    if(auto* command = m_actionManager->command(Constants::Actions::Copy)) {
        m_copyAction->setShortcuts(command->defaultShortcuts());
    }
    QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_copyAction->setVisible(selectionModel()->hasSelection()); });
    QObject::connect(m_copyAction, &QAction::triggered, this, &TagEditorView::copySelection);
    m_copyAction->setVisible(selectionModel()->hasSelection());

    m_pasteAction->setShortcut(QKeySequence::Paste);
    if(auto* command = m_actionManager->command(Constants::Actions::Paste)) {
        m_pasteAction->setShortcuts(command->defaultShortcuts());
    }
    QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_pasteAction->setVisible(selectionModel()->hasSelection()); });
    QObject::connect(QApplication::clipboard(), &QClipboard::changed, this,
                     [this]() { m_pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty()); });
    QObject::connect(m_pasteAction, &QAction::triggered, this, [this]() { pasteSelection(false); });
    m_pasteAction->setVisible(selectionModel()->hasSelection());
    m_pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());

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
    menu->addAction(m_copyAction);
    menu->addAction(m_pasteAction);
    menu->addSeparator();
    menu->addAction(m_pasteFields);
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

    if(event->button() == Qt::RightButton || (index.isValid() && index.row() == m_ratingRow)) {
        // Don't start editing on right-click
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    else {
        setEditTriggers(m_editTrigger);
    }

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

void TagEditorView::keyPressEvent(QKeyEvent* event)
{
    const QKeyCombination pasteSeq{Qt::CTRL | Qt::SHIFT | Qt::Key_V};

    if((event == QKeySequence::Copy)) {
        m_copyAction->trigger();
        return;
    }
    if((event == QKeySequence::Paste)) {
        m_pasteAction->trigger();
        return;
    }
    if(event->keyCombination() == pasteSeq) {
        m_pasteFields->trigger();
        return;
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

void TagEditorView::copySelection()
{
    const auto selected = selectionModel()->selectedRows();
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
    const auto selected = selectionModel()->selectedRows();
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
