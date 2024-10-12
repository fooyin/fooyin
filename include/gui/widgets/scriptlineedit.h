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

#pragma once

#include "fygui_export.h"

#include <gui/widgets/multilinedelegate.h>

#include <QLineEdit>
#include <QPlainTextEdit>

namespace Fooyin {
class Track;

class FYGUI_EXPORT ScriptLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ScriptLineEdit(QWidget* parent = nullptr);
    explicit ScriptLineEdit(const QString& script, QWidget* parent = nullptr);
    ScriptLineEdit(const QString& script, const Track& track, QWidget* parent = nullptr);
};

class FYGUI_EXPORT ScriptTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ScriptTextEdit(QWidget* parent = nullptr);
    explicit ScriptTextEdit(const QString& script, QWidget* parent = nullptr);
    ScriptTextEdit(const QString& script, const Track& track, QWidget* parent = nullptr);

    [[nodiscard]] QString text() const;
    void setText(const QString& text);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QAction* m_openEditor;
};
} // namespace Fooyin
