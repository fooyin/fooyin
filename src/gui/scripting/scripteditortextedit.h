/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "scripting/scriptreferenceentries.h"

#include <core/scripting/scriptscanner.h>

#include <QBasicTimer>
#include <QCompleter>
#include <QPlainTextEdit>
#include <QWidget>

class QLabel;
class QStandardItemModel;

namespace Fooyin {
class LineNumberArea;
class ScriptCompleter;
class ScriptEditorTextEdit;

enum ScriptReferenceRole : int
{
    InsertTextRole = Qt::UserRole + 1,
    CursorOffsetRole,
    KindRole,
};

class ScriptEditorTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ScriptEditorTextEdit(QWidget* parent = nullptr);

    void setAutocompleteEnabled(bool enabled);
    void setFunctionHintsEnabled(bool enabled);
    void setLineNumbersVisible(bool visible);
    void setWhitespaceVisible(bool visible);
    void setCurrentLineHighlighted(bool highlighted);
    void setMatchingBracketsHighlighted(bool highlighted);

    [[nodiscard]] int lineNumberAreaWidth() const;

    void paintLineNumbers(QPaintEvent* event);
    void insertSnippet(const QString& insertText, int cursorOffset = 0,
                       ScriptReferenceKind kind = ScriptReferenceKind::Variable);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct CompletionContext
    {
        bool valid{false};
        ScriptReferenceKind kind{ScriptReferenceKind::Variable};
        QString prefix;
        int startPos{-1};
        int endPos{-1};
    };

    struct FunctionHintContext
    {
        QString name;
        int position{-1};
        int argument{0};
        int conditionalDepth{0};
    };

    [[nodiscard]] FunctionHintContext functionHintContext();
    void queueFunctionHintUpdate();
    void updateFunctionHint();
    void positionFunctionHint();

    [[nodiscard]] static bool shouldUpdateCompletion(const QKeyEvent* event);
    [[nodiscard]] static bool isCompletionDismissKey(const QKeyEvent* event);

    void populateCompletionModels();
    void updateCompletion();
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);

    void updateExtraSelections();
    void appendMatchingBracketSelections(QList<QTextEdit::ExtraSelection>& selections) const;

    [[nodiscard]] CompletionContext completionContext() const;

    void insertCompletion(const QModelIndex& index);

    ScriptCompleter* m_completer;
    QStandardItemModel* m_variableModel;
    QStandardItemModel* m_functionModel;
    LineNumberArea* m_lineNumberArea;
    QLabel* m_functionHint;
    QBasicTimer m_functionHintTimer;
    ScriptScanner m_functionHintScanner;
    int m_functionHintRevision;
    int m_dismissedFunction;
    int m_completionStart;
    int m_completionEnd;
    bool m_autocompleteEnabled;
    bool m_functionHintsEnabled;
    bool m_showLineNumbers;
    bool m_highlightCurrentLine;
    bool m_highlightMatchingBrackets;
};
} // namespace Fooyin
