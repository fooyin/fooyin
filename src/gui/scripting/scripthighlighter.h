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

#include <core/scripting/scriptscanner.h>

#include <QRegularExpression>
#include <QSyntaxHighlighter>

namespace Fooyin {
class ScriptHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit ScriptHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void expression();
    void quote();
    void variable();
    void function();
    void functionArgs();
    void conditional();

    void setTokenFormat(const QTextCharFormat& format);

    void advance();
    [[nodiscard]] bool currentToken(ScriptScanner::TokenType type) const;
    bool match(ScriptScanner::TokenType type);

    QTextCharFormat m_varFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_conditionalFormat;
    QTextCharFormat m_operatorFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_errorFormat;

    ScriptScanner m_scanner;
    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;
};
} // namespace Fooyin
