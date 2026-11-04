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

#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/utils.h>

#include <QAction>
#include <QBasicTimer>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimerEvent>
#include <QToolButton>
#include <QToolTip>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ScriptTextEditPrivate
{
public:
    explicit ScriptTextEditPrivate(ScriptTextEdit* self)
        : m_self{self}
        , m_openEditor{new QAction(ScriptTextEdit::tr("Open in script editor"), self)}
        , m_toolArea{new QWidget(self)}
        , m_toolAreaLayout{new QHBoxLayout(m_toolArea)}
        , m_error{new QAction(ScriptTextEdit::tr("Script error"), self)}
    { }

    void validate()
    {
        const auto parsed = m_queryMode ? m_parser.parseQuery(m_self->text()) : m_parser.parse(m_self->text());
        m_syntaxError     = !parsed.errors.empty();
        m_error->setVisible(m_syntaxError || m_evaluationError);
    }

    ScriptTextEdit* m_self;
    QAction* m_openEditor;
    QWidget* m_toolArea;
    QHBoxLayout* m_toolAreaLayout;
    QAction* m_error;
    QToolButton* m_errorButton{nullptr};
    ScriptParser m_parser;
    QBasicTimer m_timer;
    bool m_queryMode{false};
    bool m_syntaxError{false};
    bool m_evaluationError{false};
};

ScriptLineEdit::ScriptLineEdit(QWidget* parent)
    : ScriptLineEdit{{}, {}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, QWidget* parent)
    : ScriptLineEdit{script, {}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, const Track& track, QWidget* parent)
    : QLineEdit{parent}
{
    setScript(script);

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

void ScriptLineEdit::setScript(const QString& script)
{
    if(text() == script) {
        return;
    }
    setText(script);
    setCursorPosition(0);
}

ScriptTextEdit::ScriptTextEdit(QWidget* parent)
    : ScriptTextEdit{{}, {}, parent}
{ }

ScriptTextEdit::ScriptTextEdit(const QString& script, QWidget* parent)
    : ScriptTextEdit{script, {}, parent}
{ }

ScriptTextEdit::ScriptTextEdit(const QString& script, const Track& track, QWidget* parent)
    : QPlainTextEdit{script, parent}
    , p{std::make_unique<ScriptTextEditPrivate>(this)}
{
    Gui::setThemeIcon(p->m_openEditor, Constants::Icons::ScriptEditor);
    QObject::connect(p->m_openEditor, &QAction::triggered, this, [this, track]() {
        ScriptEditor::openEditor(
            text(),
            [this, track](const QString& editedScript) {
                if(!isReadOnly()) {
                    setText(editedScript);
                }
            },
            track, this);
    });

    p->m_toolAreaLayout->setContentsMargins(4, 0, 4, 0);
    p->m_toolAreaLayout->addStretch();

    p->m_toolArea->setAutoFillBackground(true);
    p->m_toolArea->setBackgroundRole(QPalette::AlternateBase);
    addToolAction(p->m_openEditor);

    Gui::setThemeIcon(p->m_error, Constants::Icons::Warning);
    p->m_error->setToolTip(tr("Open in the Script Editor for details"));
    addToolAction(p->m_error, Qt::AlignRight);
    QObject::connect(p->m_error, &QAction::triggered, p->m_openEditor, &QAction::trigger);
    QObject::connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        p->m_evaluationError = false;
        p->m_timer.start(150, this);
    });

    p->validate();
}

ScriptTextEdit::~ScriptTextEdit() = default;

QString ScriptTextEdit::text() const
{
    return toPlainText();
}

void ScriptTextEdit::setText(const QString& text)
{
    setPlainText(text);
}

void ScriptTextEdit::setError(bool error)
{
    p->m_evaluationError = error;
    p->m_error->setVisible(p->m_syntaxError || p->m_evaluationError);
}

void ScriptTextEdit::setQueryMode(bool enabled)
{
    if(p->m_queryMode != enabled) {
        p->m_queryMode = enabled;
        p->validate();
    }
}

void ScriptTextEdit::addToolAction(QAction* action)
{
    addToolAction(action, Qt::AlignLeft);
}

void ScriptTextEdit::addToolAction(QAction* action, Qt::Alignment alignment)
{
    auto* button = new QToolButton(p->m_toolArea);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setDefaultAction(action);
    button->setAutoRaise(true);

    if(alignment & Qt::AlignRight) {
        // Keep other actions before the error indicator
        const int index = p->m_errorButton ? p->m_toolAreaLayout->indexOf(p->m_errorButton) : -1;
        p->m_toolAreaLayout->insertWidget(index, button);
    }
    else {
        int index{0};
        while(index < p->m_toolAreaLayout->count() && !p->m_toolAreaLayout->itemAt(index)->spacerItem()) {
            ++index;
        }
        p->m_toolAreaLayout->insertWidget(index, button);
    }

    if(action == p->m_error) {
        p->m_errorButton = button;
    }

    p->m_toolArea->setFixedHeight(p->m_toolArea->sizeHint().height());
    setViewportMargins(0, 0, 0, p->m_toolArea->height());

    button->setVisible(action->isVisible());
    QObject::connect(action, &QAction::visibleChanged, button,
                     [action, button]() { button->setVisible(action->isVisible()); });
}

void ScriptTextEdit::addHint(const QString& hint)
{
    auto* button = new QToolButton(p->m_toolArea);
    button->setText(tr("Script help"));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setToolTip(hint);
    Gui::setThemeIcon(button, Constants::Icons::Information);
    QObject::connect(button, &QToolButton::clicked, button, [button]() {
        QToolTip::showText(button->mapToGlobal(QPoint{0, button->height()}), button->toolTip(), button);
    });

    p->m_toolAreaLayout->addWidget(button);
    p->m_toolArea->setFixedHeight(p->m_toolArea->sizeHint().height());
    setViewportMargins(0, 0, 0, p->m_toolArea->height());
}

QSize ScriptTextEdit::sizeHint() const
{
    QSize hint = QPlainTextEdit::sizeHint();
    hint.setHeight((3 * fontMetrics().lineSpacing()) + (2 * frameWidth()) + viewportMargins().bottom()
                   + p->m_toolArea->height());
    return hint;
}

QSize ScriptTextEdit::minimumSizeHint() const
{
    QSize hint = QPlainTextEdit::minimumSizeHint();
    hint.setHeight((3 * fontMetrics().lineSpacing()) + (2 * frameWidth()) + viewportMargins().bottom()
                   + p->m_toolArea->height());
    return hint;
}

void ScriptTextEdit::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);

    if(event->type() == QEvent::ReadOnlyChange) {
        p->m_openEditor->setDisabled(isReadOnly());
    }
}

void ScriptTextEdit::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());
    menu->setAttribute(Qt::WA_DeleteOnClose);

    p->m_openEditor->setDisabled(isReadOnly());

    menu->addSeparator();
    menu->addAction(p->m_openEditor);

    menu->popup(event->globalPos());
}

void ScriptTextEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect rect          = contentsRect();
    const int scrollbarHeight = horizontalScrollBar()->isVisible() ? horizontalScrollBar()->height() : 0;
    p->m_toolArea->setGeometry(rect.x(), rect.bottom() + 1 - p->m_toolArea->height() - scrollbarHeight, rect.width(),
                               p->m_toolArea->height());
}

void ScriptTextEdit::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_timer.timerId()) {
        p->m_timer.stop();
        p->validate();
        return;
    }
    QPlainTextEdit::timerEvent(event);
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
