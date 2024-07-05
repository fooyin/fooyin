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

#include <gui/widgets/customisableinput.h>

#include <gui/guiconstants.h>
#include <utils/utils.h>

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
    CustomisableInput* m_self;

    QLineEdit* m_input;
    QPushButton* m_optionsButton;
    QPushButton* m_fontButton;
    QPushButton* m_colourButton;
    QFont m_font;
    QColor m_colour;
    State m_state;

    explicit Private(CustomisableInput* self)
        : m_self{self}
        , m_input{new QLineEdit(m_self)}
        , m_optionsButton{new QPushButton(Utils::iconFromTheme(Constants::Icons::Font), {}, m_self)}
        , m_fontButton{new QPushButton(m_self)}
        , m_colourButton{new QPushButton(m_self)}
    {
        auto* layout = new QHBoxLayout(m_self);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(m_input);
        layout->addWidget(m_optionsButton);
        layout->addWidget(m_fontButton);
        layout->addWidget(m_colourButton);

        m_fontButton->hide();
        m_colourButton->hide();
    }

    void toggleOptions() const
    {
        m_fontButton->setHidden(!m_fontButton->isHidden());
        m_colourButton->setHidden(!m_colourButton->isHidden());
    }

    void showFontDialog()
    {
        bool ok;
        const QFont chosenFont = QFontDialog::getFont(&ok, m_font, m_self, tr("Select Font"));
        if(ok && chosenFont != m_font) {
            m_self->setFont(chosenFont);
            m_state |= FontChanged;
        }
    }

    void showColourDialog()
    {
        const QColor chosenColour
            = QColorDialog::getColor(m_colour, m_self, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
        if(chosenColour.isValid() && chosenColour != m_colour) {
            m_self->setColour(chosenColour);
            m_state |= ColourChanged;
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
    setFont(p->m_font);
    setColour(QApplication::palette().text().color());

    QObject::connect(p->m_optionsButton, &QPushButton::pressed, this, [this]() { p->toggleOptions(); });
    QObject::connect(p->m_fontButton, &QPushButton::pressed, this, [this]() { p->showFontDialog(); });
    QObject::connect(p->m_colourButton, &QPushButton::pressed, this, [this]() { p->showColourDialog(); });

    QObject::connect(p->m_input, &QLineEdit::textEdited, this, &ExpandableInput::textChanged);
}

CustomisableInput::~CustomisableInput() = default;

QString CustomisableInput::text() const
{
    return p->m_input->text();
}

QFont CustomisableInput::font() const
{
    return p->m_font;
}

QColor CustomisableInput::colour() const
{
    return p->m_colour;
}

CustomisableInput::State CustomisableInput::state() const
{
    return p->m_state;
}

void CustomisableInput::setText(const QString& text)
{
    p->m_input->setText(text);
}

void CustomisableInput::setFont(const QFont& font)
{
    p->m_font = font;

    p->m_fontButton->setText(QStringLiteral("%1 (%2)").arg(font.family()).arg(font.pointSize()));
}

void CustomisableInput::setColour(const QColor& colour)
{
    p->m_colour = colour;

    QPixmap px(20, 20);
    px.fill(colour);
    p->m_colourButton->setIcon(px);
    p->m_colourButton->setText(colour.name());
}

void CustomisableInput::setState(State state)
{
    p->m_state = state;
}

void CustomisableInput::setReadOnly(bool readOnly)
{
    ExpandableInput::setReadOnly(readOnly);

    p->m_input->setReadOnly(readOnly);
    p->m_fontButton->setDisabled(readOnly);
    p->m_colourButton->setDisabled(readOnly);
}

void CustomisableInput::resetState()
{
    p->m_state &= ~FontChanged;
    p->m_state &= ~ColourChanged;
}
} // namespace Fooyin

#include "gui/widgets/moc_customisableinput.cpp"
