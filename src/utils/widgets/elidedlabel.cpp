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

#include <utils/widgets/elidedlabel.h>

#include <QResizeEvent>

#include <utility>

namespace Fooyin {
ElidedLabel::ElidedLabel(QWidget* parent)
    : ElidedLabel{{}, Qt::ElideRight, parent}
{ }

ElidedLabel::ElidedLabel(Qt::TextElideMode elideMode, QWidget* parent)
    : ElidedLabel{{}, elideMode, parent}
{ }

ElidedLabel::ElidedLabel(QString text, Qt::TextElideMode elideMode, QWidget* parent)
    : QLabel{parent}
    , m_elideMode{elideMode}
    , m_text{std::move(text)}
{ }

Qt::TextElideMode ElidedLabel::elideMode() const
{
    return m_elideMode;
}

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    m_elideMode = elideMode;
    update();
}

QString ElidedLabel::text() const
{
    return m_text;
}

void ElidedLabel::setText(const QString& text)
{
    m_text = text;
    QLabel::setText(text);
    update();
}

void ElidedLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);

    const QFontMetrics fm{fontMetrics()};
    const QString elidedText = fm.elidedText(m_text, m_elideMode, width());
    QLabel::setText(elidedText);
}
} // namespace Fooyin
