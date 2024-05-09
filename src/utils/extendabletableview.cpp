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

#include <utils/extendabletableview.h>

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextEdit>
#include <QToolButton>

constexpr auto ButtonAreaHeight = 40;

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

    QSize minimumSizeHint() const override
    {
        return {0, ButtonAreaHeight};
    }

    QSize sizeHint() const override
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

struct ExtendableTableView::Private
{
    ExtendableTableView* self;

    ActionManager* actionManager;

    ExtendableTableModel* model{nullptr};

    Tools tools;
    WidgetContext* context;

    ExtendableToolArea* toolArea;

    QAction* add;
    QAction* remove;
    QPointer<QAction> moveUp;
    QPointer<QAction> moveDown;

    Command* addCommand;
    Command* removeCommand;

    QToolButton* addButton;
    QToolButton* removeButton;
    QPointer<QToolButton> moveUpButton;
    QPointer<QToolButton> moveDownButton;

    int column{0};

    Private(ExtendableTableView* self_, ActionManager* actionManager_, const Tools& tools_)
        : self{self_}
        , actionManager{actionManager_}
        , tools{tools_}
        , context{new WidgetContext(
              self, Context{Id{"Context.ExtendableTableView."}.append(Utils::generateRandomHash())}, self)}
        , toolArea{new ExtendableToolArea(self)}
        , add{new QAction(tr("Add"), self)}
        , remove{new QAction(tr("Remove"), self)}
        , addCommand{actionManager->registerAction(add, Constants::Actions::New, context->context())}
        , removeCommand{actionManager->registerAction(remove, Constants::Actions::Remove, context->context())}
        , addButton{new QToolButton(self)}
        , removeButton{new QToolButton(self)}
    {
        actionManager->addContextObject(context);

        addCommand->setDefaultShortcut(QKeySequence::New);
        removeCommand->setDefaultShortcut(QKeySequence::Delete);

        QObject::connect(add, &QAction::triggered, self, [this]() { handleNewRow(); });
        QObject::connect(remove, &QAction::triggered, self, [this]() { handleRemoveRow(); });

        addButton->setDefaultAction(addCommand->action());
        toolArea->addTool(addButton);
        removeButton->setDefaultAction(removeCommand->action());
        toolArea->addTool(removeButton);

        updateTools();
    }

    void updateToolArea() const
    {
        const int vHeaderWidth  = self->verticalHeader()->isVisible() ? self->verticalHeader()->width() : 0;
        const int hHeaderHeight = self->horizontalHeader()->isVisible() ? self->horizontalHeader()->height() : 0;

        self->setViewportMargins(vHeaderWidth, hHeaderHeight, 0, ButtonAreaHeight);
    }

    void updateTools()
    {
        if(tools & Move) {
            if(!moveUp) {
                moveUp = new QAction(Utils::iconFromTheme(Constants::Icons::Up), tr("Move Up"), self);
                QObject::connect(moveUp, &QAction::triggered, self, [this]() { handleMoveUp(); });

                moveUpButton = new QToolButton(self);
                moveUpButton->setDefaultAction(moveUp);
                toolArea->addTool(moveUpButton);
            }
            if(!moveDown) {
                moveDown = new QAction(Utils::iconFromTheme(Constants::Icons::Down), tr("Move Down"), self);
                QObject::connect(moveDown, &QAction::triggered, self, [this]() { handleMoveDown(); });

                moveDownButton = new QToolButton(self);
                moveDownButton->setDefaultAction(moveDown);
                toolArea->addTool(moveDownButton);
            }
        }
        else {
            if(moveUp) {
                moveUp->deleteLater();
            }
            if(moveUpButton) {
                moveUpButton->deleteLater();
            }
            if(moveDown) {
                moveDown->deleteLater();
            }
            if(moveDownButton) {
                moveDownButton->deleteLater();
            }
        }
    }

    void handleNewRow()
    {
        if(model) {
            QObject::connect(
                model, &QAbstractItemModel::rowsInserted, self,
                [this](const QModelIndex& parent, int first) {
                    const QModelIndex index = model->index(first, column, parent);
                    if(index.isValid()) {
                        self->scrollTo(index);
                        self->edit(index);
                    }
                },
                Qt::SingleShotConnection);

            model->addPendingRow();
        }
    }

    void handleRemoveRow() const
    {
        if(model) {
            const QModelIndexList selected = self->selectionModel()->selectedIndexes();

            std::set<int> rows;
            std::ranges::transform(selected, std::inserter(rows, rows.begin()),
                                   [](const QModelIndex& index) { return index.row(); });
            std::ranges::for_each(rows, [this](const int row) { model->removeRow(row); });
        }
    }

    void handleMoveUp() const
    {
        // TODO: Implement
    }

    void handleMoveDown() const
    {
        // TODO: Implement
    }
};

ExtendableTableView::ExtendableTableView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, {None}, parent}
{ }

ExtendableTableView::ExtendableTableView(ActionManager* actionManager, const Tools& tools, QWidget* parent)
    : QTableView{parent}
    , p{std::make_unique<Private>(this, actionManager, tools)}
{ }

void ExtendableTableView::setTools(const Tools& tools)
{
    p->tools = tools;
    p->updateTools();
}

void ExtendableTableView::setToolButtonStyle(Qt::ToolButtonStyle style)
{
    p->addButton->setToolButtonStyle(style);
    p->removeButton->setToolButtonStyle(style);

    if(p->moveUpButton) {
        p->moveUpButton->setToolButtonStyle(style);
    }
    if(p->moveDownButton) {
        p->moveDownButton->setToolButtonStyle(style);
    }
}

void ExtendableTableView::addCustomTool(QWidget* widget)
{
    if(widget) {
        p->toolArea->addCustomTool(widget);
    }
}

ExtendableTableView::~ExtendableTableView() = default;

void ExtendableTableView::setExtendableModel(ExtendableTableModel* model)
{
    p->model = model;
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
    p->column = column;
}

QAction* ExtendableTableView::removeAction() const
{
    return p->remove;
}

QAction* ExtendableTableView::moveUpAction() const
{
    return p->moveUp;
}

QAction* ExtendableTableView::moveDownAction() const
{
    return p->moveDown;
}

Context ExtendableTableView::context() const
{
    return p->context->context();
}

void ExtendableTableView::addContextActions(QMenu* /*menu*/) { }

void ExtendableTableView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if(!index.isValid() || index.row() < model()->rowCount() - 1) {
        QTableView::scrollTo(index, hint);
        return;
    }

    QRect lastItemRect = visualRect(index);
    QTableView::scrollTo(index, hint);
    QRect visibleRect = viewport()->rect();

    // Fixes issues with scrolling to last index with a bottom margin
    if(!visibleRect.contains(lastItemRect.bottomRight())) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}

void ExtendableTableView::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addContextActions(menu);
    menu->addSeparator();

    menu->addAction(p->addCommand->action());
    menu->addAction(p->removeCommand->action());

    menu->popup(event->globalPos());
}

void ExtendableTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);

    p->updateToolArea();

    const QRect rect           = contentsRect();
    const int hScrollbarHeight = horizontalScrollBar()->isVisible() ? horizontalScrollBar()->height() : 0;

    p->toolArea->setGeometry(rect.x(), rect.y() + rect.height() - ButtonAreaHeight - hScrollbarHeight, rect.width(),
                             ButtonAreaHeight);
}
} // namespace Fooyin

#include "extendabletableview.moc"
#include "utils/moc_extendabletableview.cpp"
