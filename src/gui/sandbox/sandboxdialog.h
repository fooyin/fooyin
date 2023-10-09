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

#pragma once

#include "scripthighlighter.h"

#include <QDialog>

class QTreeView;
class QGridLayout;
class QSplitter;
class QTextEdit;
class QTimer;

namespace Fy::Sandbox {

class SandboxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SandboxDialog(QWidget* parent = nullptr);

private:
    void resetTimer();
    void textChanged();

    QGridLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    QSplitter* m_documentSplitter;

    QTextEdit* m_editor;
    QTextEdit* m_results;
    QTreeView* m_expressiontree;
    ScriptHighlighter m_highlighter;
    QTimer* m_errorTimer;
    Core::Scripting::Registry m_registry;
    Core::Scripting::Parser m_parser;
};
} // namespace Fy::Sandbox
