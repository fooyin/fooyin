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

#include <gui/widgets/fontbutton.h>

#include <QCheckBox>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
class FontButtonPrivate
{
public:
    FontButtonPrivate(FontButton* self, const QIcon& icon, const QString& text)
        : m_self{self}
        , m_checkBox{new QCheckBox(text, m_self)}
        , m_button{new QPushButton(icon, {}, m_self)}
    { }

    void pickFont();
    void updateText();
    void updateButtonState();

    FontButton* m_self;
    QFont m_font;
    bool m_changed{false};
    QCheckBox* m_checkBox;
    QPushButton* m_button;
};

void FontButtonPrivate::pickFont()
{
    const QPointer guard{m_self};

    bool ok;
    const QFont chosenFont = QFontDialog::getFont(&ok, m_font, m_self, FontButton::tr("Select Font"));
    if(guard && ok && chosenFont != m_font) {
        m_changed = true;
        m_self->setButtonFont(chosenFont);
    }
}

void FontButtonPrivate::updateText()
{
    m_button->setText(u"%1 (%2)"_s.arg(m_font.family()).arg(m_font.pointSize()));
}

void FontButtonPrivate::updateButtonState()
{
    m_button->setEnabled(m_self->isChecked());
}

FontButton::FontButton(QWidget* parent)
    : FontButton{{}, {}, false, parent}
{ }

FontButton::FontButton(const QString& text, QWidget* parent)
    : FontButton{{}, text, false, parent}
{ }

FontButton::FontButton(const QString& text, bool checkable, QWidget* parent)
    : FontButton{{}, text, checkable, parent}
{ }

FontButton::FontButton(const QIcon& icon, const QString& text, QWidget* parent)
    : FontButton{icon, text, false, parent}
{ }

FontButton::FontButton(const QIcon& icon, const QString& text, bool checkable, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<FontButtonPrivate>(this, icon, text)}
{
    p->m_checkBox->setVisible(checkable);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(p->m_checkBox);
    layout->addWidget(p->m_button, 1);

    QObject::connect(p->m_button, &QPushButton::clicked, this, [this]() { p->pickFont(); });
    QObject::connect(p->m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        p->updateButtonState();
        Q_EMIT toggled(checked);
    });

    p->updateButtonState();
}

FontButton::~FontButton() = default;

QFont FontButton::buttonFont() const
{
    return p->m_font;
}

bool FontButton::fontChanged() const
{
    return p->m_changed;
}

bool FontButton::isCheckable() const
{
    return !p->m_checkBox->isHidden();
}

bool FontButton::isChecked() const
{
    return !isCheckable() || p->m_checkBox->isChecked();
}

int FontButton::labelWidthHint() const
{
    return p->m_checkBox->sizeHint().width();
}

QString FontButton::labelText() const
{
    return p->m_checkBox->text();
}

void FontButton::setButtonFont(const QFont& font)
{
    const bool changed = std::exchange(p->m_font, font) != font;

    p->m_button->setFont(p->m_font);
    p->updateText();

    if(changed) {
        Q_EMIT fontUpdated(font);
    }
}

void FontButton::setButtonFont(const QString& font)
{
    QFont newFont;
    if(!font.isEmpty()) {
        newFont.fromString(font);
    }
    setButtonFont(newFont);
}

void FontButton::setCheckable(bool checkable)
{
    p->m_checkBox->setToolTip(toolTip());
    p->m_checkBox->setVisible(checkable);
    p->updateButtonState();
    updateGeometry();
}

void FontButton::setChecked(bool checked)
{
    p->m_checkBox->setChecked(checked);
}

void FontButton::setLabelText(const QString& text)
{
    p->m_checkBox->setText(text);
    updateGeometry();
}

void FontButton::setLabelWidth(int width)
{
    p->m_checkBox->setMinimumWidth(std::max(width, labelWidthHint()));
    updateGeometry();
}

void FontButton::setToolTip(const QString& text)
{
    QWidget::setToolTip(text);
    p->m_checkBox->setToolTip(text);
    p->m_button->setToolTip(text);
}

} // namespace Fooyin
