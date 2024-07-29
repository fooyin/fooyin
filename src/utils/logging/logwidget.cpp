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

#include "logmodel.h"

#include <utils/logging/messagehandler.h>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeView>

Q_LOGGING_CATEGORY(LOG_WIDGET, "LogWidget")

namespace Fooyin {
LogWidget::LogWidget(QWidget* parent)
    : QWidget{parent}
    , m_view{new QTreeView(this)}
    , m_model{new LogModel(this)}
    , m_scrollIsAtBottom{false}
{
    setWindowTitle(tr("Log"));

    auto* clearButton = new QPushButton(tr("&Clear"), this);
    QObject::connect(clearButton, &QPushButton::clicked, m_model, &LogModel::clear);
    auto* saveButton = new QPushButton(tr("&Save Log"), this);
    QObject::connect(saveButton, &QPushButton::clicked, this, &LogWidget::saveLog);

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(clearButton, QDialogButtonBox::ResetRole);
    buttonBox->addButton(saveButton, QDialogButtonBox::ApplyRole);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_view, 0, 0);
    layout->addWidget(buttonBox, 1, 0);

    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_view->header()->setStretchLastSection(true);

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

    resize(720, 480);
}

void LogWidget::addEntry(const QString& message, QtMsgType type)
{
    m_model->addEntry({QDateTime::currentDateTime(), type, message});
}

void LogWidget::saveLog()
{
    const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save Log"), QDir::homePath());
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
        out << entry.time.toString(Qt::ISODateWithMs) << " " << static_cast<int>(entry.type) << " " << entry.message
            << "\n";
    }

    logFile.close();
}
} // namespace Fooyin
