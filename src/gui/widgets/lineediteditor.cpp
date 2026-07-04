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

#include <gui/widgets/lineediteditor.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

using namespace Qt::StringLiterals;

namespace Fooyin {
LineEditEditor::LineEditEditor(const QString& name, QWidget* parent)
    : QWidget{parent}
    , m_label{new QLabel(this)}
    , m_lineEdit{new QLineEdit(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    if(!name.isEmpty()) {
        m_label->setText(name + ":"_L1);
        m_label->setBuddy(m_lineEdit);
        layout->addWidget(m_label);
    }
    else {
        m_label->hide();
    }

    layout->addWidget(m_lineEdit);
    setFocusProxy(m_lineEdit);

    QObject::connect(m_lineEdit, &QLineEdit::textChanged, this, &LineEditEditor::textChanged);
    QObject::connect(m_lineEdit, &QLineEdit::textEdited, this, &LineEditEditor::textEdited);
    QObject::connect(m_lineEdit, &QLineEdit::returnPressed, this, &LineEditEditor::returnPressed);
    QObject::connect(m_lineEdit, &QLineEdit::editingFinished, this, &LineEditEditor::editingFinished);
}

LineEditEditor::LineEditEditor(QWidget* parent)
    : LineEditEditor{{}, parent}
{ }

QString LineEditEditor::text() const
{
    return m_lineEdit->text();
}

void LineEditEditor::setText(const QString& text)
{
    m_lineEdit->setText(text);
}

void LineEditEditor::clear()
{
    m_lineEdit->clear();
}

QString LineEditEditor::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

void LineEditEditor::setPlaceholderText(const QString& text)
{
    m_lineEdit->setPlaceholderText(text);
}

int LineEditEditor::labelWidth() const
{
    return m_label->minimumWidth();
}

int LineEditEditor::labelWidthHint() const
{
    return m_label->sizeHint().width();
}

void LineEditEditor::setLabelWidth(int width)
{
    m_label->setMinimumWidth(width);
}

QLabel* LineEditEditor::label() const
{
    return m_label;
}

QLineEdit* LineEditEditor::lineEdit() const
{
    return m_lineEdit;
}
} // namespace Fooyin

#include "gui/widgets/moc_lineediteditor.cpp"
