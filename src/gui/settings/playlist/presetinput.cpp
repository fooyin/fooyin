/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "presetinput.h"

#include "gui/guiconstants.h"

#include <QColorDialog>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

namespace Fy::Gui::Settings {
PresetInput::PresetInput(QWidget* parent)
    : QWidget{parent}
    , m_editBlock{new QLineEdit(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_editBlock);

    auto* blockFontButton   = new QAction(QIcon::fromTheme(Constants::Icons::Font), "", this);
    auto* blockColourButton = new QAction(QIcon::fromTheme(Constants::Icons::TextColour), "", this);

    QObject::connect(blockFontButton, &QAction::triggered, this, &PresetInput::showFontDialog);
    QObject::connect(blockColourButton, &QAction::triggered, this, &PresetInput::showColourDialog);

    m_editBlock->addAction(blockColourButton, QLineEdit::TrailingPosition);
    m_editBlock->addAction(blockFontButton, QLineEdit::TrailingPosition);
}

QString PresetInput::text() const
{
    return m_editBlock->text();
}

QFont PresetInput::font() const
{
    return m_font;
}

QColor PresetInput::colour() const
{
    return m_colour;
}

PresetInput::State PresetInput::state() const
{
    return m_state;
}

void PresetInput::setReadOnly(bool readOnly)
{
    m_editBlock->setReadOnly(readOnly);
}

void PresetInput::setText(const QString& text)
{
    m_editBlock->setText(text);
}

void PresetInput::setFont(const QFont& font)
{
    m_font = font;
}

void PresetInput::setColour(const QColor& colour)
{
    m_colour = colour;
}

void PresetInput::resetState()
{
    m_state &= FontChanged;
    m_state &= ColourChanged;
}

void PresetInput::showFontDialog()
{
    bool ok;
    const QFont chosenFont = QFontDialog::getFont(&ok, m_font, this, "Select Font");
    if(ok) {
        m_font = chosenFont;
        m_state |= FontChanged;
    }
}

void PresetInput::showColourDialog()
{
    const QColor chosenColour = QColorDialog::getColor(m_colour, this, "Select Colour");
    if(chosenColour.isValid()) {
        m_colour = chosenColour;
        m_state |= ColourChanged;
    }
}
} // namespace Fy::Gui::Settings
