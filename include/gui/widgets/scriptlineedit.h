/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>

#include <memory>

namespace Fooyin {
class ScriptTextEditPrivate;
class Track;

class FYGUI_EXPORT ScriptLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ScriptLineEdit(QWidget* parent = nullptr);
    explicit ScriptLineEdit(const QString& script, QWidget* parent = nullptr);
    ScriptLineEdit(const QString& script, const Track& track, QWidget* parent = nullptr);

    //! Sets the script and moves the cursor to the beginning only if the text changes.
    void setScript(const QString& script);
};

class FYGUI_EXPORT ScriptTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ScriptTextEdit(QWidget* parent = nullptr);
    explicit ScriptTextEdit(const QString& script, QWidget* parent = nullptr);
    ScriptTextEdit(const QString& script, const Track& track, QWidget* parent = nullptr);
    ~ScriptTextEdit() override;

    [[nodiscard]] QString text() const;
    void setText(const QString& text);

    //! Includes errors from the consumer's evaluation alongside syntax errors.
    void setError(bool error);
    //! Validates query syntax instead of display script syntax.
    void setQueryMode(bool enabled);

    void addToolAction(QAction* action);
    void addToolAction(QAction* action, Qt::Alignment alignment);
    void addHint(const QString& hint);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<ScriptTextEditPrivate> p;
};

class FYGUI_EXPORT ScriptComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit ScriptComboBox(QWidget* parent = nullptr);
    explicit ScriptComboBox(const QString& script, QWidget* parent = nullptr);
    ScriptComboBox(const QString& script, const Track& track, QWidget* parent = nullptr);
};
} // namespace Fooyin
