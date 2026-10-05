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

#include "scripteditortextedit.h"

#include <QAbstractItemView>
#include <QLabel>
#include <QPainter>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTextBlock>

#include <chrono>
#include <ranges>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto FunctionHintInterval = 0ms;
#else
constexpr auto FunctionHintInterval = 0;
#endif

namespace Fooyin {
class ScriptCompleter : public QCompleter
{
public:
    using QCompleter::QCompleter;

    [[nodiscard]] QString pathFromIndex(const QModelIndex& index) const override
    {
        return index.data(InsertTextRole).toString();
    }
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(ScriptEditorTextEdit* editor)
        : QWidget{editor}
        , m_editor{editor}
    { }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {m_editor->lineNumberAreaWidth(), 0};
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        m_editor->paintLineNumbers(event);
    }

private:
    ScriptEditorTextEdit* m_editor;
};

ScriptEditorTextEdit::ScriptEditorTextEdit(QWidget* parent)
    : QPlainTextEdit{parent}
    , m_completer{new ScriptCompleter(this)}
    , m_variableModel{new QStandardItemModel(this)}
    , m_functionModel{new QStandardItemModel(this)}
    , m_lineNumberArea{new LineNumberArea(this)}
    , m_functionHint{new QLabel(viewport())}
    , m_functionHintRevision{-1}
    , m_dismissedFunction{-1}
    , m_completionStart{-1}
    , m_completionEnd{-1}
    , m_autocompleteEnabled{true}
    , m_functionHintsEnabled{true}
    , m_showLineNumbers{true}
    , m_highlightCurrentLine{true}
    , m_highlightMatchingBrackets{true}
{
    populateCompletionModels();

    m_functionHint->setTextFormat(Qt::RichText);
    m_functionHint->setWordWrap(true);
    m_functionHint->setMargin(6);
    m_functionHint->setFrameStyle(Box | Plain);
    m_functionHint->setBackgroundRole(QPalette::ToolTipBase);
    m_functionHint->setForegroundRole(QPalette::ToolTipText);
    m_functionHint->setAutoFillBackground(true);
    m_functionHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_functionHint->hide();

    m_completer->setWidget(this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setFilterMode(Qt::MatchStartsWith);

    QObject::connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this,
                     &ScriptEditorTextEdit::insertCompletion);
    QObject::connect(this, &QPlainTextEdit::blockCountChanged, this, &ScriptEditorTextEdit::updateLineNumberAreaWidth);
    QObject::connect(this, &QPlainTextEdit::updateRequest, this, &ScriptEditorTextEdit::updateLineNumberArea);
    QObject::connect(this, &QPlainTextEdit::cursorPositionChanged, this, &ScriptEditorTextEdit::updateExtraSelections);

    QObject::connect(this, &QPlainTextEdit::cursorPositionChanged, this,
                     &ScriptEditorTextEdit::queueFunctionHintUpdate);
    QObject::connect(this, &QPlainTextEdit::textChanged, this, &ScriptEditorTextEdit::queueFunctionHintUpdate);
    QObject::connect(this, &QPlainTextEdit::updateRequest, this, [this]() {
        if(m_functionHint->isVisible()) {
            positionFunctionHint();
        }
    });

    updateLineNumberAreaWidth();
}

void ScriptEditorTextEdit::setAutocompleteEnabled(bool enabled)
{
    m_autocompleteEnabled = enabled;
    if(!enabled) {
        m_completer->popup()->hide();
    }
}

void ScriptEditorTextEdit::setFunctionHintsEnabled(bool enabled)
{
    if(std::exchange(m_functionHintsEnabled, enabled) == enabled) {
        return;
    }

    m_dismissedFunction = -1;
    updateFunctionHint();
}

void ScriptEditorTextEdit::setLineNumbersVisible(bool visible)
{
    m_showLineNumbers = visible;
    m_lineNumberArea->setVisible(visible);
    updateLineNumberAreaWidth();
}

void ScriptEditorTextEdit::setWhitespaceVisible(bool visible)
{
    QTextOption option = document()->defaultTextOption();
    option.setFlags(
        visible ? option.flags() | QTextOption::ShowTabsAndSpaces | QTextOption::ShowLineAndParagraphSeparators
                : option.flags() & ~QTextOption::ShowTabsAndSpaces & ~QTextOption::ShowLineAndParagraphSeparators);
    document()->setDefaultTextOption(option);
}

void ScriptEditorTextEdit::setCurrentLineHighlighted(bool highlighted)
{
    m_highlightCurrentLine = highlighted;
    updateExtraSelections();
}

void ScriptEditorTextEdit::setMatchingBracketsHighlighted(bool highlighted)
{
    m_highlightMatchingBrackets = highlighted;
    updateExtraSelections();
}

int ScriptEditorTextEdit::lineNumberAreaWidth() const
{
    if(!m_showLineNumbers) {
        return 0;
    }

    int digits{1};
    for(int lines = std::max(1, blockCount()); lines >= 10; lines /= 10) {
        ++digits;
    }
    return 8 + (fontMetrics().horizontalAdvance(u'9') * digits);
}

void ScriptEditorTextEdit::paintLineNumbers(QPaintEvent* event)
{
    QPainter painter{m_lineNumberArea};
    painter.fillRect(event->rect(), palette().alternateBase());
    painter.setPen(palette().placeholderText().color());

    QTextBlock block = firstVisibleBlock();
    int blockNumber  = block.blockNumber();
    int top          = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom       = top + qRound(blockBoundingRect(block).height());

    while(block.isValid() && top <= event->rect().bottom()) {
        if(block.isVisible() && bottom >= event->rect().top()) {
            painter.drawText(0, top, m_lineNumberArea->width() - 4, fontMetrics().height(), Qt::AlignRight,
                             QString::number(blockNumber + 1));
        }

        block  = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void ScriptEditorTextEdit::insertSnippet(const QString& insertText, int cursorOffset, ScriptReferenceKind kind)
{
    QTextCursor cursor{textCursor()};

    if(kind == ScriptReferenceKind::Formatting && cursor.hasSelection() && cursorOffset > 0) {
        const auto splitPosition = insertText.size() - cursorOffset;
        const QString prefix     = insertText.left(splitPosition);
        const QString suffix     = insertText.mid(splitPosition);
        const QString selected   = cursor.selectedText();

        cursor.insertText(prefix + selected + suffix);
        setTextCursor(cursor);
        return;
    }

    cursor.insertText(insertText);
    setTextCursor(cursor);

    if(cursorOffset > 0) {
        cursor = textCursor();
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursorOffset);
        setTextCursor(cursor);
    }
}

void ScriptEditorTextEdit::keyPressEvent(QKeyEvent* event)
{
    if(m_functionHintsEnabled && event->key() == Qt::Key_Space
       && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        m_dismissedFunction = -1;
        updateFunctionHint();
        event->accept();
        return;
    }

    if(event->key() == Qt::Key_Escape && m_functionHint->isVisible()) {
        m_dismissedFunction = functionHintContext().position;
        m_functionHintTimer.stop();
        m_functionHint->hide();
        if(!m_completer->popup()->isVisible()) {
            event->accept();
            return;
        }
    }

    if(m_completer->popup()->isVisible()) {
        switch(event->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                event->ignore();
                return;
            default:
                break;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    if(m_autocompleteEnabled && shouldUpdateCompletion(event)) {
        updateCompletion();
    }
    else if(isCompletionDismissKey(event)) {
        m_completer->popup()->hide();
    }
}

void ScriptEditorTextEdit::focusOutEvent(QFocusEvent* event)
{
    m_functionHintTimer.stop();
    m_functionHint->hide();
    QPlainTextEdit::focusOutEvent(event);
}

void ScriptEditorTextEdit::focusInEvent(QFocusEvent* event)
{
    QPlainTextEdit::focusInEvent(event);
    queueFunctionHintUpdate();
}

void ScriptEditorTextEdit::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_functionHintTimer.timerId()) {
        updateFunctionHint();
        return;
    }
    QPlainTextEdit::timerEvent(event);
}

void ScriptEditorTextEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect contents = contentsRect();
    m_lineNumberArea->setGeometry(QRect{contents.left(), contents.top(), lineNumberAreaWidth(), contents.height()});
}

void ScriptEditorTextEdit::queueFunctionHintUpdate()
{
    if(m_functionHintsEnabled && hasFocus() && !m_functionHintTimer.isActive()) {
        m_functionHintTimer.start(FunctionHintInterval, this);
    }
}

void ScriptEditorTextEdit::updateFunctionHint()
{
    m_functionHintTimer.stop();
    if(!m_functionHintsEnabled || !hasFocus()) {
        m_functionHint->hide();
        return;
    }

    const auto context = functionHintContext();
    if(context.position < 0 || context.position == m_dismissedFunction) {
        m_functionHint->hide();
        return;
    }
    m_dismissedFunction = -1;

    const auto& entries = scriptReferenceEntries();
    const auto entry    = std::ranges::find_if(entries, [&context](const auto& candidate) {
        return candidate.kind == ScriptReferenceKind::Function
            && candidate.insertText.compare(u"$%1()"_s.arg(context.name), Qt::CaseInsensitive) == 0;
    });
    if(entry == entries.end()) {
        m_functionHint->hide();
        return;
    }

    const QString& signature = entry->label;
    const int opening        = static_cast<int>(signature.indexOf(u'('));
    const int closing        = static_cast<int>(signature.lastIndexOf(u')'));

    int start{opening + 1};
    int argument{0};
    while(argument < context.argument) {
        const int comma = static_cast<int>(signature.indexOf(u',', start));
        if(comma < 0 || comma >= closing || signature.mid(start, comma - start).contains(u'…')) {
            break;
        }
        start = comma + 1;
        ++argument;
    }

    int end = static_cast<int>(signature.indexOf(u',', start));
    if(end < 0 || end > closing) {
        end = closing;
    }

    while(start < end && signature.at(start) == u'[') {
        ++start;
    }
    while(end > start && (signature.at(end - 1) == u'[' || signature.at(end - 1) == u']')) {
        --end;
    }

    QString hint = signature.toHtmlEscaped();
    if(opening >= 0 && end > start) {
        hint = signature.left(start).toHtmlEscaped() + u"<b>"_s + signature.mid(start, end - start).toHtmlEscaped()
             + u"</b>"_s + signature.mid(end).toHtmlEscaped();
    }

    m_functionHint->setText(hint + u"<br/>"_s + entry->description.toHtmlEscaped());
    m_functionHint->setMaximumWidth(std::min(520, viewport()->width()));
    m_functionHint->setWordWrap(false);
    m_functionHint->adjustSize();
    m_functionHint->setWordWrap(true);

    m_functionHint->resize(m_functionHint->width(), m_functionHint->heightForWidth(m_functionHint->width()));
    m_functionHint->show();
    m_functionHint->raise();
    positionFunctionHint();
}

void ScriptEditorTextEdit::positionFunctionHint()
{
    const QRect cursor = cursorRect();
    if(!viewport()->rect().intersects(cursor)) {
        m_functionHint->hide();
        return;
    }

    const int x     = std::clamp(cursor.left(), 0, std::max(0, viewport()->width() - m_functionHint->width()));
    const int above = cursor.top() - m_functionHint->height() - 4;
    const int y     = above >= 0 ? above : cursor.bottom() + 4;
    m_functionHint->move(x, std::min(y, std::max(0, viewport()->height() - m_functionHint->height())));
}

bool ScriptEditorTextEdit::shouldUpdateCompletion(const QKeyEvent* event)
{
    if(event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::AltModifier)
       || event->modifiers().testFlag(Qt::MetaModifier)) {
        return false;
    }

    switch(event->key()) {
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return false;
        default:
            break;
    }

    const QString text = event->text();
    if(text.size() != 1) {
        return false;
    }

    return !text.front().isNull();
}

bool ScriptEditorTextEdit::isCompletionDismissKey(const QKeyEvent* event)
{
    switch(event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Escape:
            return true;
        default:
            return false;
    }
}

void ScriptEditorTextEdit::populateCompletionModels()
{
    for(const auto& entry : scriptReferenceEntries()) {
        auto* item = new QStandardItem(entry.label);
        item->setData(entry.insertText, InsertTextRole);
        item->setData(entry.cursorOffset, CursorOffsetRole);
        item->setData(static_cast<int>(entry.kind), KindRole);
        item->setToolTip(entry.description);

        switch(entry.kind) {
            case ScriptReferenceKind::Variable:
                m_variableModel->appendRow(item);
                break;
            case ScriptReferenceKind::Function:
                m_functionModel->appendRow(item);
                break;
            case ScriptReferenceKind::Formatting:
                break;
        }
    }
}

void ScriptEditorTextEdit::updateCompletion()
{
    const CompletionContext context = completionContext();
    if(!context.valid) {
        m_completer->popup()->hide();
        return;
    }

    m_completionStart = context.startPos;
    m_completionEnd   = context.endPos;

    QStandardItemModel* model{nullptr};
    switch(context.kind) {
        case ScriptReferenceKind::Variable:
            model = m_variableModel;
            break;
        case ScriptReferenceKind::Function:
            model = m_functionModel;
            break;
        case ScriptReferenceKind::Formatting:
            break;
    }

    if(model) {
        m_completer->setModel(model);
    }

    m_completer->setCompletionPrefix(context.prefix);

    if(!m_completer->setCurrentRow(0)) {
        m_completer->popup()->hide();
        return;
    }

    QRect rect{cursorRect()};
    rect.setWidth(m_completer->popup()->sizeHintForColumn(0)
                  + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 24);
    m_completer->complete(rect);
}

void ScriptEditorTextEdit::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptEditorTextEdit::updateLineNumberArea(const QRect& rect, int dy)
{
    if(dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    }
    else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if(rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void ScriptEditorTextEdit::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    if(m_highlightCurrentLine) {
        QTextEdit::ExtraSelection selection;
        QColor colour = palette().highlight().color();
        colour.setAlpha(30);
        selection.format.setBackground(colour);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.push_back(selection);
    }

    if(m_highlightMatchingBrackets) {
        appendMatchingBracketSelections(selections);
    }

    setExtraSelections(selections);
}

void ScriptEditorTextEdit::appendMatchingBracketSelections(QList<QTextEdit::ExtraSelection>& selections) const
{
    const QString text = toPlainText();
    int position       = textCursor().position();
    if(position >= text.size() || !QStringView{u"()[]"}.contains(text.at(position))) {
        --position;
    }
    if(position < 0 || position >= text.size()) {
        return;
    }

    const QChar bracket  = text.at(position);
    const QString pairs  = u"()[]"_s;
    const auto pairIndex = pairs.indexOf(bracket);
    if(pairIndex < 0) {
        return;
    }

    const bool opening = pairIndex % 2 == 0;
    const QChar match  = pairs.at(opening ? pairIndex + 1 : pairIndex - 1);
    const int step     = opening ? 1 : -1;

    int depth{0};
    int matchPosition{-1};
    for(int i{position}; i >= 0 && i < text.size(); i += step) {
        if(text.at(i) == bracket) {
            ++depth;
        }
        else if(text.at(i) == match && --depth == 0) {
            matchPosition = i;
            break;
        }
    }
    if(matchPosition < 0) {
        return;
    }

    QColor colour = palette().highlight().color();
    colour.setAlpha(120);
    for(const int bracketPosition : {position, matchPosition}) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(colour);
        selection.cursor = textCursor();
        selection.cursor.setPosition(bracketPosition);
        selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        selections.push_back(selection);
    }
}

void ScriptEditorTextEdit::insertCompletion(const QModelIndex& index)
{
    if(!index.isValid() || m_completionStart < 0 || m_completionEnd < m_completionStart) {
        return;
    }

    QTextCursor cursor{textCursor()};
    cursor.setPosition(m_completionStart);
    cursor.setPosition(m_completionEnd, QTextCursor::KeepAnchor);
    cursor.insertText(index.data(InsertTextRole).toString());
    setTextCursor(cursor);

    const int cursorOffset = index.data(CursorOffsetRole).toInt();
    if(cursorOffset > 0) {
        cursor = textCursor();
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursorOffset);
        setTextCursor(cursor);
    }
}

ScriptEditorTextEdit::CompletionContext ScriptEditorTextEdit::completionContext() const
{
    const QTextCursor cursor = textCursor();
    const QTextBlock block   = cursor.block();
    const QString text       = block.text();
    const int posInBlock     = cursor.position() - block.position();

    if(posInBlock <= 0 || posInBlock > text.size()) {
        return {};
    }

    int start{posInBlock};

    while(start > 0) {
        const QChar ch = text.at(start - 1);

        if(ch.isLetterOrNumber() || ch == u'_') {
            --start;
            continue;
        }

        if(ch == '%'_L1 || ch == '$'_L1) {
            --start;
        }

        break;
    }

    if(start < 0 || start >= posInBlock) {
        return {};
    }

    const QChar opener = text.at(start);
    if(opener != '%'_L1 && opener != '$'_L1) {
        return {};
    }

    // Don't open if at end of variable
    if(opener == '%'_L1 && start == posInBlock - 1 && start > 0) {
        const QChar previous = text.at(start - 1);
        if(previous.isLetterOrNumber() || previous == '_'_L1) {
            return {};
        }
    }

    int end{posInBlock};

    while(end < text.size()) {
        const QChar ch = text.at(end);

        if(ch.isLetterOrNumber() || ch == '_'_L1) {
            ++end;
            continue;
        }

        break;
    }

    if(opener == '%'_L1 && end < text.size() && text.at(end) == '%'_L1) {
        ++end;
    }

    CompletionContext context;
    context.valid    = true;
    context.kind     = opener == '%'_L1 ? ScriptReferenceKind::Variable : ScriptReferenceKind::Function;
    context.prefix   = text.mid(start, posInBlock - start);
    context.startPos = block.position() + start;
    context.endPos   = block.position() + end;
    return context;
}

ScriptEditorTextEdit::FunctionHintContext ScriptEditorTextEdit::functionHintContext()
{
    const int revision = document()->revision();
    if(m_functionHintRevision != revision) {
        m_functionHintScanner.setup(toPlainText());
        m_functionHintRevision = revision;
    }

    int tokenIndex{1};
    const auto peekToken = [this, &tokenIndex](int delta = 0) {
        return m_functionHintScanner.peekNext(tokenIndex + delta);
    };

    std::vector<FunctionHintContext> functions;

    const int cursorPosition = textCursor().position();
    int conditionalDepth{0};
    bool inQuote{false};
    bool inVariable{false};
    bool inAngle{false};
    bool escaped{false};

    auto token = peekToken();
    while(token.type != ScriptScanner::TokEos && token.position < cursorPosition) {
        if(escaped) {
            escaped = false;
        }
        else if(token.type == ScriptScanner::TokEscape) {
            const auto next = peekToken(1).type;
            escaped         = inQuote ? next == ScriptScanner::TokQuote || next == ScriptScanner::TokEscape
                                      : next != ScriptScanner::TokLeftAngle;
        }
        else if(token.type == ScriptScanner::TokQuote && !inVariable && !inAngle) {
            inQuote = !inQuote;
        }
        else if(!inQuote) {
            if(token.type == ScriptScanner::TokComment) {
                if(cursorPosition <= token.position + token.value.size()) {
                    return {};
                }
            }
            else if(token.type == ScriptScanner::TokVar && !inAngle) {
                inVariable = !inVariable;
            }
            else if(!inVariable) {
                if(token.type == ScriptScanner::TokLeftAngle) {
                    // Match the parser's angle literals so their commas don't count as args
                    bool quoted{false};
                    for(int delta{1};; ++delta) {
                        const auto next = peekToken(delta);
                        if(next.type == ScriptScanner::TokQuote) {
                            quoted = !quoted;
                        }
                        else if(next.type == ScriptScanner::TokRightAngle && !quoted) {
                            inAngle = true;
                            break;
                        }
                        else if(next.type == ScriptScanner::TokEos
                                || (!quoted
                                    && (next.type == ScriptScanner::TokRightParen
                                        || next.type == ScriptScanner::TokRightSquare))) {
                            break;
                        }
                    }
                }
                else if(token.type == ScriptScanner::TokRightAngle) {
                    inAngle = false;
                }
                else if(!inAngle) {
                    switch(token.type) {
                        case ScriptScanner::TokLeftParen:
                            if(peekToken(-2).type == ScriptScanner::TokFunc) {
                                functions.push_back({.name             = peekToken(-1).value.toString().trimmed(),
                                                     .position         = token.position,
                                                     .argument         = 0,
                                                     .conditionalDepth = conditionalDepth});
                            }
                            break;
                        case ScriptScanner::TokRightParen:
                            if(!functions.empty()) {
                                functions.pop_back();
                            }
                            break;
                        case ScriptScanner::TokLeftSquare:
                            ++conditionalDepth;
                            break;
                        case ScriptScanner::TokRightSquare:
                            conditionalDepth = std::max(0, conditionalDepth - 1);
                            break;
                        case ScriptScanner::TokComma:
                            if(!functions.empty() && functions.back().conditionalDepth == conditionalDepth) {
                                ++functions.back().argument;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        ++tokenIndex;
        token = peekToken();
    }

    return functions.empty() ? FunctionHintContext{} : functions.back();
}
} // namespace Fooyin
