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

#include <gui/widgets/scriptlineedit.h>

#include "gui/scripting/scripteditor.h"

#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QIcon>
#include <QMenu>

namespace Fooyin {
ScriptLineEdit::ScriptLineEdit(QWidget* parent)
    : ScriptLineEdit{{}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, QWidget* parent)
    : QLineEdit{script, parent}
{
    auto* openEditor
        = new QAction(Utils::iconFromTheme(Constants::Icons::ScriptEditor), tr("Open in script editor"), this);
    QObject::connect(openEditor, &QAction::triggered, this, [this]() {
        ScriptEditor::openEditor(text(), [this](const QString& editedScript) {
            if(!isReadOnly()) {
                setText(editedScript);
            }
        });
    });
    addAction(openEditor, TrailingPosition);
}

ScriptTextEdit::ScriptTextEdit(QWidget* parent)
    : ScriptTextEdit{{}, parent}
{ }

ScriptTextEdit::ScriptTextEdit(const QString& script, QWidget* parent)
    : QPlainTextEdit{script, parent}
    , m_openEditor{nullptr}
{ }

void ScriptTextEdit::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(!m_openEditor) {
        m_openEditor
            = new QAction(Utils::iconFromTheme(Constants::Icons::ScriptEditor), tr("Open in script editor"), this);
        QObject::connect(m_openEditor, &QAction::triggered, this, [this]() {
            ScriptEditor::openEditor(toPlainText(), [this](const QString& editedScript) {
                if(!isReadOnly()) {
                    setPlainText(editedScript);
                }
            });
        });
    }

    menu->addSeparator();
    menu->addAction(m_openEditor);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "gui/widgets/moc_scriptlineedit.cpp"
