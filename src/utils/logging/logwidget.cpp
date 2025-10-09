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

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollBar>
#include <QTimerEvent>
#include <QTreeView>

Q_LOGGING_CATEGORY(LOG_WIDGET, "fy.log")

using namespace Qt::StringLiterals;

constexpr auto FlushInterval    = 200;
constexpr auto MaxQueuedEntries = 250;

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
    auto* saveButton = new QPushButton(tr("&Save Log"), this);
    QObject::connect(saveButton, &QPushButton::clicked, this, &LogWidget::saveLog);

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(clearButton, QDialogButtonBox::ResetRole);
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

    QObject::connect(m_level, &QComboBox::currentIndexChanged, this,
                     [this]() { MessageHandler::setLevel(m_level->currentData().value<QtMsgType>()); });
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
