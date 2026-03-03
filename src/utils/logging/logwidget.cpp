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

#include "logwidget.h"

#include <utils/logging/messagehandler.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLoggingCategory>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTimerEvent>
#include <QTreeView>

#include <algorithm>

Q_LOGGING_CATEGORY(LOG_WIDGET, "fy.log")

using namespace Qt::StringLiterals;

constexpr auto FlushInterval    = 200;
constexpr auto MaxQueuedEntries = 250;

namespace {
QModelIndexList allRowIndexes(const QAbstractItemModel* model)
{
    QModelIndexList indexes;
    if(!model) {
        return indexes;
    }

    const QModelIndex root;

    const int rows = model->rowCount(root);

    indexes.reserve(rows);

    for(int row{0}; row < rows; ++row) {
        indexes.push_back(model->index(row, 0, root));
    }

    return indexes;
}
} // namespace

namespace Fooyin {
LogWidget::LogWidget(SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_view{new QTreeView(this)}
    , m_model{new LogModel(this)}
    , m_level{new QComboBox(this)}
    , m_scrollIsAtBottom{false}
{
    setWindowTitle(tr("Log"));

    auto* clearButton = new QPushButton(tr("&Clear"), this);
    QObject::connect(clearButton, &QPushButton::clicked, m_model, &LogModel::clear);
    auto* copyButton = new QPushButton(tr("Co&py Log"), this);
    QObject::connect(copyButton, &QPushButton::clicked, this, [this]() { copyRows(allRowIndexes(m_model)); });
    auto* saveButton = new QPushButton(tr("&Save Log"), this);
    QObject::connect(saveButton, &QPushButton::clicked, this, &LogWidget::saveLog);

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(clearButton, QDialogButtonBox::ResetRole);
    buttonBox->addButton(copyButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(saveButton, QDialogButtonBox::ApplyRole);

    m_level->addItem(tr("Debug"), QtMsgType::QtDebugMsg);
    m_level->addItem(tr("Info"), QtMsgType::QtInfoMsg);
    m_level->addItem(tr("Warning"), QtMsgType::QtWarningMsg);
    m_level->addItem(tr("Critical"), QtMsgType::QtCriticalMsg);

    m_level->setCurrentIndex(m_level->findData(MessageHandler::level()));

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_view, 0, 0, 1, 2);
    layout->addWidget(m_level, 1, 0);
    layout->addWidget(buttonBox, 1, 1);
    layout->setColumnStretch(1, 1);

    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_view->header()->setStretchLastSection(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* copyAction = new QAction(tr("&Copy"), m_view);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copyAction, &QAction::triggered, this, &LogWidget::copySelectedRows);
    m_view->addAction(copyAction);

    QObject::connect(m_level, &QComboBox::currentIndexChanged, this,
                     [this]() { MessageHandler::setLevel(m_level->currentData().value<QtMsgType>()); });
    QObject::connect(m_view, &QWidget::customContextMenuRequested, this, &LogWidget::showContextMenu);
    QObject::connect(m_model, &QAbstractItemModel::rowsAboutToBeInserted, this, [this]() {
        if(const auto* bar = m_view->verticalScrollBar()) {
            m_scrollIsAtBottom = (bar->value() == bar->maximum());
        }
    });
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() {
        if(m_scrollIsAtBottom) {
            m_view->scrollToBottom();
        }
    });

    MessageHandler::install(this);
}

void LogWidget::addEntry(const QString& message, QtMsgType type)
{
    m_queue.push({.time = QDateTime::currentDateTime(), .type = type, .category = {}, .message = message});

    m_flushTimer.start(m_queue.size() >= MaxQueuedEntries ? 0 : FlushInterval, this);
}

QSize LogWidget::sizeHint() const
{
    return Utils::proportionateSize(this, 0.3, 0.4);
}

void LogWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_flushTimer.timerId()) {
        QWidget::timerEvent(event);
        return;
    }

    m_flushTimer.stop();

    if(!m_queue.empty()) {
        std::vector<ConsoleEntry> entries;
        entries.reserve(m_queue.size());

        while(!m_queue.empty()) {
            entries.push_back(std::move(m_queue.front()));
            m_queue.pop();
        }

        m_model->addEntries(std::move(entries));
    }

    QWidget::timerEvent(event);
}

void LogWidget::copySelectedRows() const
{
    const auto* selection = m_view->selectionModel();
    if(!selection) {
        return;
    }

    copyRows(selection->selectedRows());
}

void LogWidget::copyRows(const QModelIndexList& rows) const
{
    if(rows.isEmpty()) {
        return;
    }

    QModelIndexList sortedRows{rows};

    std::ranges::sort(sortedRows, [](const QModelIndex& lhs, const QModelIndex& rhs) {
        if(lhs.row() == rhs.row()) {
            return lhs.column() < rhs.column();
        }
        return lhs.row() < rhs.row();
    });

    QStringList lines;
    lines.reserve(sortedRows.size());

    for(const QModelIndex& rowIndex : std::as_const(sortedRows)) {
        QStringList columns;
        columns.reserve(m_model->columnCount({}));

        for(int col{0}; col < m_model->columnCount({}); ++col) {
            const QModelIndex index = m_model->index(rowIndex.row(), col);
            columns.push_back(m_model->data(index, Qt::DisplayRole).toString());
        }

        lines.push_back(columns.join(u"\t"_s));
    }

    if(!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join(u"\n"_s));
    }
}

void LogWidget::showContextMenu(const QPoint& pos)
{
    if(auto* selection = m_view->selectionModel()) {
        const QModelIndex clicked = m_view->indexAt(pos);
        if(clicked.isValid() && !selection->isRowSelected(clicked.row(), clicked.parent())) {
            selection->clearSelection();
            selection->select(clicked, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            selection->setCurrentIndex(clicked, QItemSelectionModel::Current);
        }
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* copy = new QAction(tr("&Copy"), menu);
    copy->setEnabled(m_view->selectionModel() && m_view->selectionModel()->hasSelection());
    QObject::connect(copy, &QAction::triggered, this, &LogWidget::copySelectedRows);
    menu->addAction(copy);

    menu->popup(m_view->viewport()->mapToGlobal(pos));
}

void LogWidget::saveLog()
{
    const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save Log"), QDir::homePath(), {}, nullptr,
                                                          QFileDialog::DontResolveSymlinks);
    if(saveFile.isEmpty()) {
        return;
    }

    QFile logFile{saveFile};
    if(!logFile.open(QIODevice::WriteOnly)) {
        qCWarning(LOG_WIDGET) << "Failed to open file for writing";
        return;
    }

    QTextStream out{&logFile};

    const auto entries = m_model->entries();
    for(const auto& entry : entries) {
        out << u"%1 %2 %3: %4\n"_s.arg(entry.time.toString(Qt::ISODateWithMs), LogModel::typeToString(entry.type),
                                       entry.category, entry.message);
    }

    logFile.close();
}
} // namespace Fooyin
