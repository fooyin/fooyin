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
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
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
    , m_changed{false}
    , m_checkBox{new QCheckBox(text, this)}
    , m_button{new QPushButton(icon, {}, this)}
{
    m_checkBox->setVisible(checkable);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_checkBox);
    layout->addWidget(m_button, 1);

    QObject::connect(m_button, &QPushButton::clicked, this, &FontButton::pickFont);
    QObject::connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        updateButtonState();
        Q_EMIT toggled(checked);
    });

    updateButtonState();
}

QFont FontButton::buttonFont() const
{
    return m_font;
}

bool FontButton::fontChanged() const
{
    return m_changed;
}

bool FontButton::isCheckable() const
{
    return !m_checkBox->isHidden();
}

bool FontButton::isChecked() const
{
    return !isCheckable() || m_checkBox->isChecked();
}

int FontButton::labelWidthHint() const
{
    return m_checkBox->sizeHint().width();
}

QString FontButton::labelText() const
{
    return m_checkBox->text();
}

void FontButton::setButtonFont(const QFont& font)
{
    m_font = font;
    m_button->setFont(m_font);
    updateText();
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
    m_checkBox->setToolTip(toolTip());
    m_checkBox->setVisible(checkable);
    updateButtonState();
    updateGeometry();
}

void FontButton::setChecked(bool checked)
{
    m_checkBox->setChecked(checked);
}

void FontButton::setLabelText(const QString& text)
{
    m_checkBox->setText(text);
    updateGeometry();
}

void FontButton::setLabelWidth(int width)
{
    m_checkBox->setMinimumWidth(std::max(width, labelWidthHint()));
    updateGeometry();
}

void FontButton::setToolTip(const QString& text)
{
    QWidget::setToolTip(text);
    m_checkBox->setToolTip(text);
    m_button->setToolTip(text);
}

void FontButton::pickFont()
{
    bool ok;
    const QFont chosenFont = QFontDialog::getFont(&ok, m_font, this, tr("Select Font"));
    if(ok && chosenFont != m_font) {
        setButtonFont(chosenFont);
        m_changed = true;
    }
}

void FontButton::updateText()
{
    m_button->setText(u"%1 (%2)"_s.arg(m_font.family()).arg(m_font.pointSize()));
}

void FontButton::updateButtonState()
{
    m_button->setEnabled(isChecked());
}
} // namespace Fooyin
