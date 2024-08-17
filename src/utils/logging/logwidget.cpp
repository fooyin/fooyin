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
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeView>

Q_LOGGING_CATEGORY(LOG_WIDGET, "fy.log")

constexpr auto LogLevel = "LogLevel";

namespace {
QString levelToFilterRule(const QString& level)
{
    if(level == u"debug") {
        return QStringLiteral("fy.*.critical=true\nfy.*.warning=true\nfy.*.info=true\nfy.*.debug=true\n");
    }
    if(level == u"info") {
        return QStringLiteral("fy.*.critical=true\nfy.*.warning=true\nfy.*.info=true\nfy.*.debug=false\n");
    }
    if(level == u"warning") {
        return QStringLiteral("fy.*.critical=true\nfy.*.warning=true\nfy.*.info=false\nfy.*.debug=false\n");
    }
    if(level == u"critical") {
        return QStringLiteral("fy.*.critical=true\nfy.*.warning=false\nfy.*.info=false\nfy.*.debug=false\n");
    }
    return {};
}

void setLevel(const QString& level)
{
    QString filterRules;
    filterRules.append(levelToFilterRule(level));
    QLoggingCategory::setFilterRules(filterRules);
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
    auto* saveButton = new QPushButton(tr("&Save Log"), this);
    QObject::connect(saveButton, &QPushButton::clicked, this, &LogWidget::saveLog);

    auto* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(clearButton, QDialogButtonBox::ResetRole);
    buttonBox->addButton(saveButton, QDialogButtonBox::ApplyRole);

    m_level->addItem(QStringLiteral("Debug"));
    m_level->addItem(QStringLiteral("Info"));
    m_level->addItem(QStringLiteral("Warning"));
    m_level->addItem(QStringLiteral("Critical"));

    m_level->setCurrentText(m_settings->fileValue(LogLevel, QStringLiteral("Info")).toString());
    setLevel(m_level->currentText().toLower());

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_view, 0, 0, 1, 2);
    layout->addWidget(m_level, 1, 0);
    layout->addWidget(buttonBox, 1, 1);
    layout->setColumnStretch(1, 1);

    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_view->header()->setStretchLastSection(true);

    QObject::connect(m_level, &QComboBox::currentTextChanged, this, [this](const QString& level) {
        m_settings->fileSet(LogLevel, level);
        setLevel(level.toLower());
    });
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
    m_model->addEntry({.time = QDateTime::currentDateTime(), .type = type, .category = {}, .message = message});
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
        out << QStringLiteral("%1 %2 %3: %4\n")
                   .arg(entry.time.toString(Qt::ISODateWithMs), LogModel::typeToString(entry.type), entry.category,
                        entry.message);
    }

    logFile.close();
}
} // namespace Fooyin
