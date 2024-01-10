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

#include <gui/guiconstants.h>

#include <QApplication>
#include <QColorDialog>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>

namespace Fooyin {
struct CustomisableInput::Private
{
    CustomisableInput* self;

    QFont font;
    QColor colour;
    State state;

    explicit Private(CustomisableInput* self)
        : self{self}
        , font{QApplication::font()}
        , colour{QApplication::palette().text().color()}
    { }

    void showContextMenu(const QPoint& pos)
    {
        QMenu* menu = self->widget()->createStandardContextMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* resetFont = new QAction(tr("Reset Font"), self);
        QObject::connect(resetFont, &QAction::triggered, self, [this]() {
            self->setFont(QApplication::font());
            state &= ~FontChanged;
        });
        menu->addAction(resetFont);

        auto* resetColour = new QAction(tr("Reset Colour"), self);
        QObject::connect(resetColour, &QAction::triggered, self, [this]() {
            self->setColour(QApplication::palette().text().color());
            state &= ~ColourChanged;
        });
        menu->addAction(resetColour);

        menu->popup(self->mapToGlobal(pos));
    }

    void showFontDialog()
    {
        bool ok;
        const QFont chosenFont = QFontDialog::getFont(&ok, font, self, tr("Select Font"));
        if(ok && chosenFont != font) {
            font = chosenFont;
            state |= FontChanged;
        }
    }

    void showColourDialog()
    {
        const QColor chosenColour
            = QColorDialog::getColor(colour, self, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
        if(chosenColour.isValid() && chosenColour != colour) {
            colour = chosenColour;
            state |= ColourChanged;
        }
    }
};

CustomisableInput::CustomisableInput(QWidget* parent)
    : CustomisableInput{None, parent}
{ }

CustomisableInput::CustomisableInput(Attributes attributes, QWidget* parent)
    : ExpandableInput{attributes, parent}
    , p{std::make_unique<Private>(this)}
{
    QLineEdit* lineEdit = widget();

    auto* blockColourButton = new QAction(QIcon::fromTheme(Constants::Icons::TextColour), QStringLiteral(""), this);
    QObject::connect(blockColourButton, &QAction::triggered, this, [this]() { p->showColourDialog(); });
    lineEdit->addAction(blockColourButton, QLineEdit::TrailingPosition);

    auto* blockFontButton = new QAction(QIcon::fromTheme(Constants::Icons::Font), QStringLiteral(""), this);
    QObject::connect(blockFontButton, &QAction::triggered, this, [this]() { p->showFontDialog(); });
    lineEdit->addAction(blockFontButton, QLineEdit::TrailingPosition);

    lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(lineEdit, &QLineEdit::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->showContextMenu(pos); });

    QObject::connect(lineEdit, &QLineEdit::textEdited, this, &ExpandableInput::textChanged);
}

CustomisableInput::~CustomisableInput() = default;

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

void CustomisableInput::setFont(const QFont& font)
{
    p->font = font;
}

void CustomisableInput::setColour(const QColor& colour)
{
    p->colour = colour;
}

void CustomisableInput::setState(State state)
{
    p->state = state;
}

void CustomisableInput::resetState()
{
    p->state &= ~FontChanged;
    p->state &= ~ColourChanged;
}
} // namespace Fooyin

#include "gui/widgets/moc_customisableinput.cpp"
