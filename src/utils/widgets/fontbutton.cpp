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

#include <utils/widgets/fontbutton.h>

#include <QFontDialog>

namespace Fooyin {
FontButton::FontButton(QWidget* parent)
    : FontButton{{}, QStringLiteral(""), parent}
{ }

FontButton::FontButton(const QString& text, QWidget* parent)
    : FontButton{{}, text, parent}
{ }

FontButton::FontButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QPushButton{icon, text, parent}
    , m_changed{false}
{
    QObject::connect(this, &QPushButton::clicked, this, &FontButton::pickFont);
}

QFont FontButton::font() const
{
    return m_font;
}

bool FontButton::fontChanged() const
{
    return m_changed;
}

void FontButton::setFont(const QFont& font)
{
    m_font = font;
    setText(QStringLiteral("%1 (%2)").arg(m_font.family()).arg(m_font.pointSize()));
}

void FontButton::setFont(const QString& font)
{
    m_font.fromString(font);
    setText(QStringLiteral("%1 (%2)").arg(m_font.family()).arg(m_font.pointSize()));
}

void FontButton::pickFont()
{
    bool ok;
    const QFont chosenFont = QFontDialog::getFont(&ok, m_font, this, tr("Select Font"));
    if(ok && chosenFont != m_font) {
        m_font    = chosenFont;
        m_changed = true;
    }
}
} // namespace Fooyin
