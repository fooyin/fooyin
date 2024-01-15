/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/customisableinput.h>

#include <QApplication>
#include <QColorDialog>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

namespace Fooyin {
struct CustomisableInput::Private
{
    CustomisableInput* self;

    QLineEdit* input;
    QPushButton* fontButton;
    QPushButton* colourButton;
    QFont font;
    QColor colour;
    State state;

    explicit Private(CustomisableInput* self)
        : self{self}
        , input{new QLineEdit(self)}
        , fontButton{new QPushButton(self)}
        , colourButton{new QPushButton(self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(input);
        layout->addWidget(fontButton);
        layout->addWidget(colourButton);
    }

    void showFontDialog()
    {
        bool ok;
        const QFont chosenFont = QFontDialog::getFont(&ok, font, self, tr("Select Font"));
        if(ok && chosenFont != font) {
            self->setFont(chosenFont);
            state |= FontChanged;
        }
    }

    void showColourDialog()
    {
        const QColor chosenColour
            = QColorDialog::getColor(colour, self, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
        if(chosenColour.isValid() && chosenColour != colour) {
            self->setColour(chosenColour);
            state |= ColourChanged;
        }
    }
};

CustomisableInput::CustomisableInput(QWidget* parent)
    : CustomisableInput{CustomWidget, parent}
{ }

CustomisableInput::CustomisableInput(Attributes attributes, QWidget* parent)
    : ExpandableInput{attributes & CustomWidget, parent}
    , p{std::make_unique<Private>(this)}
{
    setFont(p->font);
    setColour(QApplication::palette().text().color());

    QObject::connect(p->fontButton, &QPushButton::pressed, this, [this]() { p->showFontDialog(); });
    QObject::connect(p->colourButton, &QPushButton::pressed, this, [this]() { p->showColourDialog(); });

    QObject::connect(p->input, &QLineEdit::textEdited, this, &ExpandableInput::textChanged);
}

CustomisableInput::~CustomisableInput() = default;

QString CustomisableInput::text() const
{
    return p->input->text();
}

QFont CustomisableInput::font() const
{
    return p->font;
}

QColor CustomisableInput::colour() const
{
    return p->colour;
}

CustomisableInput::State CustomisableInput::state() const
{
    return p->state;
}

void CustomisableInput::setText(const QString& text)
{
    p->input->setText(text);
}

void CustomisableInput::setFont(const QFont& font)
{
    p->font = font;

    p->fontButton->setText(QString{"%1 (%2)"}.arg(font.family()).arg(font.pointSize()));
}

void CustomisableInput::setColour(const QColor& colour)
{
    p->colour = colour;

    QPixmap px(20, 20);
    px.fill(colour);
    p->colourButton->setIcon(px);
}

void CustomisableInput::setState(State state)
{
    p->state = state;
}

void CustomisableInput::setReadOnly(bool readOnly)
{
    ExpandableInput::setReadOnly(readOnly);

    p->input->setReadOnly(readOnly);
    p->fontButton->setDisabled(readOnly);
    p->colourButton->setDisabled(readOnly);
}

void CustomisableInput::resetState()
{
    p->state &= ~FontChanged;
    p->state &= ~ColourChanged;
}
} // namespace Fooyin

#include "gui/widgets/moc_customisableinput.cpp"
