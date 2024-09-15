/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/extendabletableview.h>

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/modelutils.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolButton>

constexpr auto ButtonAreaHeight = 45;

namespace Fooyin {
class ExtendableToolArea : public QWidget
{
    Q_OBJECT

public:
    explicit ExtendableToolArea(QWidget* parent = nullptr)
        : QWidget{parent}
        , m_toolLayout{new QHBoxLayout()}
        , m_customToolLayout{new QHBoxLayout()}
    {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 5, 10, 5);

        layout->addLayout(m_toolLayout);
        layout->addLayout(m_customToolLayout);
        layout->addStretch();
    }

    void addTool(QWidget* widget) const
    {
        m_toolLayout->addWidget(widget);
    }

    void addCustomTool(QWidget* widget) const
    {
        m_customToolLayout->addWidget(widget);
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {0, ButtonAreaHeight};
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {0, ButtonAreaHeight};
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter{this};
        painter.fillRect(event->rect(), palette().alternateBase());
    }

private:
    QHBoxLayout* m_toolLayout;
    QHBoxLayout* m_customToolLayout;
};

void ExtendableTableModel::moveRowsUp(const QModelIndexList& /*indexes*/) { }

void ExtendableTableModel::moveRowsDown(const QModelIndexList& /*indexes*/) { }

class ExtendableTableViewPrivate
{
public:
    ExtendableTableViewPrivate(ExtendableTableView* self, ActionManager* actionManager,
                               const ExtendableTableView::Tools& tools);

    void updateToolArea() const;
    void updateTools();

    void handleNewRow();
    void handleRemoveRow() const;
    void handleMoveUp() const;
    void handleMoveDown() const;

    ExtendableTableView* m_self;
    ActionManager* m_actionManager;

    ExtendableTableModel* m_model{nullptr};
    int m_column{0};
    ExtendableTableView::Tools m_tools;
    WidgetContext* m_context;
    ExtendableToolArea* m_toolArea;

    QAction* m_add;
    QAction* m_remove;
    QPointer<QAction> m_moveUp;
    QPointer<QAction> m_moveDown;

    Command* m_addCommand;
    Command* m_removeCommand;

    QToolButton* m_addButton;
    QToolButton* m_removeButton;
    QPointer<QToolButton> m_moveUpButton;
    QPointer<QToolButton> m_moveDownButton;
};

ExtendableTableViewPrivate::ExtendableTableViewPrivate(ExtendableTableView* self, ActionManager* actionManager,
                                                       const ExtendableTableView::Tools& tools)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_tools{tools}
    , m_context{new WidgetContext(
          m_self, Context{Id{"Context.ExtendableTableView."}.append(Utils::generateUniqueHash())}, m_self)}
    , m_toolArea{new ExtendableToolArea(m_self)}
    , m_add{new QAction(Utils::iconFromTheme(Constants::Icons::Add), ExtendableTableView::tr("Add"), m_self)}
    , m_remove{new QAction(Utils::iconFromTheme(Constants::Icons::Remove), ExtendableTableView::tr("Remove"), m_self)}
    , m_addCommand{m_actionManager->registerAction(m_add, Constants::Actions::New, m_context->context())}
    , m_removeCommand{m_actionManager->registerAction(m_remove, Constants::Actions::Remove, m_context->context())}
    , m_addButton{new QToolButton(m_self)}
    , m_removeButton{new QToolButton(m_self)}
{
    m_actionManager->addContextObject(m_context);

    m_addCommand->setDefaultShortcut(QKeySequence::New);
    m_removeCommand->setDefaultShortcut(QKeySequence::Delete);

    QObject::connect(m_add, &QAction::triggered, m_self, [this]() { handleNewRow(); });
    QObject::connect(m_remove, &QAction::triggered, m_self, [this]() { handleRemoveRow(); });

    m_addButton->setDefaultAction(m_addCommand->action());
    m_toolArea->addTool(m_addButton);
    m_removeButton->setDefaultAction(m_removeCommand->action());
    m_toolArea->addTool(m_removeButton);

    updateTools();
}

void ExtendableTableViewPrivate::updateToolArea() const
{
    const QRect rect        = m_self->contentsRect();
    const int vHeaderWidth  = m_self->verticalHeader()->isVisible() ? m_self->verticalHeader()->width() : 0;
    const int hHeaderHeight = m_self->horizontalHeader()->isVisible() ? m_self->horizontalHeader()->height() : 0;
    const int hScrollbarHeight
        = m_self->horizontalScrollBar()->isVisible() ? m_self->horizontalScrollBar()->height() : 0;

    m_toolArea->setGeometry(rect.x(), rect.bottom() - ButtonAreaHeight - hScrollbarHeight, rect.width(),
                            ButtonAreaHeight);
    m_self->setViewportMargins(vHeaderWidth, hHeaderHeight, 0, ButtonAreaHeight);
}

void ExtendableTableViewPrivate::updateTools()
{
    if(m_tools & ExtendableTableView::Move) {
        if(!m_moveUp) {
            m_moveUp
                = new QAction(Utils::iconFromTheme(Constants::Icons::Up), ExtendableTableView::tr("Move Up"), m_self);
            QObject::connect(m_moveUp, &QAction::triggered, m_self, [this]() { handleMoveUp(); });

            m_moveUpButton = new QToolButton(m_self);
            m_moveUpButton->setDefaultAction(m_moveUp);
            m_toolArea->addTool(m_moveUpButton);
        }
        if(!m_moveDown) {
            m_moveDown = new QAction(Utils::iconFromTheme(Constants::Icons::Down), ExtendableTableView::tr("Move Down"),
                                     m_self);
            QObject::connect(m_moveDown, &QAction::triggered, m_self, [this]() { handleMoveDown(); });

            m_moveDownButton = new QToolButton(m_self);
            m_moveDownButton->setDefaultAction(m_moveDown);
            m_toolArea->addTool(m_moveDownButton);
        }
    }
    else {
        if(m_moveUp) {
            m_moveUp->deleteLater();
        }
        if(m_moveUpButton) {
            m_moveUpButton->deleteLater();
        }
        if(m_moveDown) {
            m_moveDown->deleteLater();
        }
        if(m_moveDownButton) {
            m_moveDownButton->deleteLater();
        }
    }
}

void ExtendableTableViewPrivate::handleNewRow()
{
    if(!m_model) {
        return;
    }

    QObject::connect(
        m_model, &QAbstractItemModel::rowsInserted, m_self,
        [this](const QModelIndex& parent, int first) {
            const QModelIndex index = m_model->index(first, m_column, parent);
            if(index.isValid()) {
                m_self->scrollTo(index, QAbstractItemView::EnsureVisible);
                m_self->edit(index);
            }
        },
        Qt::SingleShotConnection);

    m_model->addPendingRow();
}

void ExtendableTableViewPrivate::handleRemoveRow() const
{
    if(!m_model) {
        return;
    }

    const QModelIndexList selected = m_self->selectionModel()->selectedIndexes();

    std::set<int> rows;
    std::ranges::transform(selected, std::inserter(rows, rows.begin()),
                           [](const QModelIndex& index) { return index.row(); });
    std::ranges::for_each(rows, [this](const int row) { m_model->removeRow(row); });
}

void ExtendableTableViewPrivate::handleMoveUp() const
{
    if(!m_model) {
        return;
    }

    std::set<QModelIndex> uniqueRows;
    const QModelIndexList selected = m_self->selectionModel()->selectedIndexes();
    for(const auto& index : selected) {
        uniqueRows.emplace(index.siblingAtColumn(0));
    }
    QModelIndexList rows;
    for(const auto& index : uniqueRows) {
        rows.emplace_back(index);
    }
    std::sort(rows.begin(), rows.end());

    const auto ranges = Utils::getIndexRanges(rows);

    for(const auto& range : ranges) {
        m_model->moveRowsUp(range);
    }
}

void ExtendableTableViewPrivate::handleMoveDown() const
{
    if(!m_model) {
        return;
    }

    std::set<QModelIndex> uniqueRows;
    const QModelIndexList selected = m_self->selectionModel()->selectedIndexes();
    for(const auto& index : selected) {
        uniqueRows.emplace(index.siblingAtColumn(0));
    }
    QModelIndexList rows;
    for(const auto& index : uniqueRows) {
        rows.emplace_back(index);
    }
    std::sort(rows.begin(), rows.end());

    const auto ranges = Utils::getIndexRanges(rows);

    for(const auto& range : ranges) {
        m_model->moveRowsDown(range);
    }
}

ExtendableTableView::ExtendableTableView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, {None}, parent}
{ }

ExtendableTableView::ExtendableTableView(ActionManager* actionManager, const Tools& tools, QWidget* parent)
    : QTableView{parent}
    , p{std::make_unique<ExtendableTableViewPrivate>(this, actionManager, tools)}
{ }

ExtendableTableView::~ExtendableTableView() = default;

void ExtendableTableView::setTools(const Tools& tools)
{
    p->m_tools = tools;
    p->updateTools();
}

void ExtendableTableView::setToolButtonStyle(Qt::ToolButtonStyle style)
{
    p->m_addButton->setToolButtonStyle(style);
    p->m_removeButton->setToolButtonStyle(style);

    if(p->m_moveUpButton) {
        p->m_moveUpButton->setToolButtonStyle(style);
    }
    if(p->m_moveDownButton) {
        p->m_moveDownButton->setToolButtonStyle(style);
    }
}

void ExtendableTableView::addCustomTool(QWidget* widget)
{
    if(widget) {
        p->m_toolArea->addCustomTool(widget);
    }
}

void ExtendableTableView::setExtendableModel(ExtendableTableModel* model)
{
    if(!model || std::exchange(p->m_model, model) == model) {
        return;
    }

    QObject::disconnect(model, &ExtendableTableModel::pendingRowCancelled, this, nullptr);

    setModel(model);

    QObject::connect(
        model, &ExtendableTableModel::pendingRowCancelled, this,
        [model]() {
            // We can't remove the row in setData, so we use a queued signal to remove it once setData returns
            model->removePendingRow();
        },
        Qt::QueuedConnection);
}

void ExtendableTableView::setExtendableColumn(int column)
{
    p->m_column = column;
}

QAction* ExtendableTableView::addRowAction() const
{
    return p->m_add;
}

QAction* ExtendableTableView::removeRowAction() const
{
    return p->m_remove;
}

QAction* ExtendableTableView::moveUpAction() const
{
    return p->m_moveUp;
}

QAction* ExtendableTableView::moveDownAction() const
{
    return p->m_moveDown;
}

Context ExtendableTableView::context() const
{
    return p->m_context->context();
}

QSize ExtendableTableView::sizeHint() const
{
    QSize hint = QTableView::sizeHint();
    hint.rheight() += ButtonAreaHeight;
    return hint;
}

QSize ExtendableTableView::minimumSizeHint() const
{
    QSize hint = QTableView::minimumSizeHint();
    hint.rheight() += ButtonAreaHeight;
    return hint;
}

void ExtendableTableView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if(!index.isValid() || index.row() < model()->rowCount() - 1) {
        QTableView::scrollTo(index, hint);
        return;
    }

    const QRect lastItemRect = visualRect(index);
    QTableView::scrollTo(index, hint);
    const QRect visibleRect = viewport()->rect();

    // Fixes issues with scrolling to last index with a bottom margin
    if(!visibleRect.contains(lastItemRect.bottomRight())) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}

void ExtendableTableView::setupContextActions(QMenu* /*menu*/, const QPoint& /*pos*/) { }

void ExtendableTableView::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    setupContextActions(menu, event->pos());
    menu->addSeparator();

    menu->addAction(p->m_addCommand->action());
    if(indexAt(event->pos()).isValid()) {
        menu->addAction(p->m_removeCommand->action());
    }

    menu->popup(event->globalPos());
}

void ExtendableTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);

    p->updateToolArea();
}
} // namespace Fooyin

#include "extendabletableview.moc"
#include "gui/widgets/moc_extendabletableview.cpp"
