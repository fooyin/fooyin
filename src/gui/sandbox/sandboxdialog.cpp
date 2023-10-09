/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "sandboxdialog.h"

#include <QGridLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>

namespace Fy::Sandbox {
SandboxDialog::SandboxDialog(QWidget* parent)
    : QDialog{parent}
    , m_mainLayout{new QGridLayout(this)}
    , m_mainSplitter{new QSplitter(Qt::Horizontal, this)}
    , m_documentSplitter{new QSplitter(Qt::Vertical, this)}
    , m_editor{new QTextEdit(this)}
    , m_results{new QTextEdit(this)}
    , m_expressiontree{new QTreeView(this)}
    , m_highlighter{m_editor->document()}
    , m_errorTimer{new QTimer(this)}
    , m_parser{&m_registry}
{
    setWindowTitle(tr("Script Sandbox"));
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_documentSplitter->addWidget(m_editor);
    m_documentSplitter->addWidget(m_results);

    m_mainSplitter->addWidget(m_documentSplitter);
    m_mainSplitter->addWidget(m_expressiontree);
    m_mainLayout->addWidget(m_mainSplitter);

    m_errorTimer->setInterval(1000);

    QObject::connect(m_editor, &QTextEdit::textChanged, this, &SandboxDialog::resetTimer);
    QObject::connect(m_errorTimer, &QTimer::timeout, this, &SandboxDialog::textChanged);
}

void SandboxDialog::resetTimer()
{
    m_errorTimer->start();
    m_results->clear();
}

void SandboxDialog::textChanged()
{
    m_results->clear();

    auto parsedText   = m_parser.parse(m_editor->toPlainText());
    const auto errors = parsedText.errors;
    for(const Core::Scripting::Error& error : errors) {
        m_results->append(error.message);
    }
}
} // namespace Fy::Sandbox
