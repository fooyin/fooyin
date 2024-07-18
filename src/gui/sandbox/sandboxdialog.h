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

#pragma once

#include "scripthighlighter.h"

#include <QDialog>

class QSplitter;
class QTextEdit;
class QTreeView;

namespace Fooyin {
class ExpressionTreeModel;
class SettingsManager;
class TrackSelectionController;

class SandboxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SandboxDialog(LibraryManager* libraryManager, TrackSelectionController* trackSelection,
                           SettingsManager* settings, QWidget* parent = nullptr);
    ~SandboxDialog() override;

private:
    void updateResults();
    void updateResults(const Expression& expression);

    void selectionChanged();
    void textChanged();

    void showErrors() const;

    void saveState() const;
    void restoreState();

    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    QSplitter* m_mainSplitter;
    QSplitter* m_documentSplitter;

    QTextEdit* m_editor;
    QTextEdit* m_results;
    ScriptHighlighter m_highlighter;

    QTreeView* m_expressionTree;
    ExpressionTreeModel* m_model;

    QTimer* m_textChangeTimer;

    ScriptRegistry m_registry;
    ScriptParser m_parser;

    ParsedScript m_currentScript;
};
} // namespace Fooyin
