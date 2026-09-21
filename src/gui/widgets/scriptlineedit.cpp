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

#include <gui/widgets/scriptlineedit.h>

#include "gui/scripting/scripteditor.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/utils.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollBar>
#include <QToolButton>

namespace Fooyin {
ScriptLineEdit::ScriptLineEdit(QWidget* parent)
    : ScriptLineEdit{{}, {}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, QWidget* parent)
    : ScriptLineEdit{script, {}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, const Track& track, QWidget* parent)
    : QLineEdit{script, parent}
{
    auto* openEditor = new QAction(tr("Open in script editor"), this);
    Gui::setThemeIcon(openEditor, Constants::Icons::ScriptEditor);
    QObject::connect(openEditor, &QAction::triggered, this, [this, track]() {
        ScriptEditor::openEditor(
            text(),
            [this](const QString& editedScript) {
                if(!isReadOnly()) {
                    setText(editedScript);
                }
            },
            track, this);
    });
    addAction(openEditor, TrailingPosition);
}

ScriptTextEdit::ScriptTextEdit(QWidget* parent)
    : ScriptTextEdit{{}, {}, parent}
{ }

ScriptTextEdit::ScriptTextEdit(const QString& script, QWidget* parent)
    : ScriptTextEdit{script, {}, parent}
{ }

ScriptTextEdit::ScriptTextEdit(const QString& script, const Track& track, QWidget* parent)
    : QPlainTextEdit{script, parent}
    , m_openEditor{new QAction(tr("Open in script editor"), this)}
    , m_toolArea{new QWidget(this)}
{
    Gui::setThemeIcon(m_openEditor, Constants::Icons::ScriptEditor);
    QObject::connect(m_openEditor, &QAction::triggered, this, [this, track]() {
        ScriptEditor::openEditor(
            text(),
            [this, track](const QString& editedScript) {
                if(!isReadOnly()) {
                    setText(editedScript);
                }
            },
            track, this);
    });

    auto* editorButton = new QToolButton(m_toolArea);
    editorButton->setText(tr("Script Editor"));
    editorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    editorButton->setDefaultAction(m_openEditor);
    editorButton->setAutoRaise(true);

    auto* layout = new QHBoxLayout(m_toolArea);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->addWidget(editorButton);
    layout->addStretch();

    m_toolArea->setAutoFillBackground(true);
    m_toolArea->setBackgroundRole(QPalette::AlternateBase);
    m_toolArea->setFixedHeight(m_toolArea->sizeHint().height());
    setViewportMargins(0, 0, 0, m_toolArea->height());
}

QString ScriptTextEdit::text() const
{
    return toPlainText();
}

void ScriptTextEdit::setText(const QString& text)
{
    setPlainText(text);
}

QSize ScriptTextEdit::sizeHint() const
{
    QSize hint = QPlainTextEdit::sizeHint();
    hint.rheight() += m_toolArea->height();
    return hint;
}

QSize ScriptTextEdit::minimumSizeHint() const
{
    QSize hint = QPlainTextEdit::minimumSizeHint();
    hint.rheight() += m_toolArea->height();
    return hint;
}

void ScriptTextEdit::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);

    if(event->type() == QEvent::ReadOnlyChange) {
        m_openEditor->setDisabled(isReadOnly());
    }
}

void ScriptTextEdit::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());
    menu->setAttribute(Qt::WA_DeleteOnClose);

    m_openEditor->setDisabled(isReadOnly());

    menu->addSeparator();
    menu->addAction(m_openEditor);

    menu->popup(event->globalPos());
}

void ScriptTextEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect rect          = contentsRect();
    const int scrollbarHeight = horizontalScrollBar()->isVisible() ? horizontalScrollBar()->height() : 0;
    m_toolArea->setGeometry(rect.x(), rect.bottom() - m_toolArea->height() - scrollbarHeight, rect.width(),
                            m_toolArea->height());
}

ScriptComboBox::ScriptComboBox(QWidget* parent)
    : ScriptComboBox{{}, {}, parent}
{ }

ScriptComboBox::ScriptComboBox(const QString& script, QWidget* parent)
    : ScriptComboBox{script, {}, parent}
{ }

ScriptComboBox::ScriptComboBox(const QString& script, const Track& track, QWidget* parent)
    : QComboBox{parent}
{
    setLineEdit(new ScriptLineEdit(script, track, this));
    setEditable(true);
}
} // namespace Fooyin

#include "gui/widgets/moc_scriptlineedit.cpp"
