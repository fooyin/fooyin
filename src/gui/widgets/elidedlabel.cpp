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

#include <gui/widgets/elidedlabel.h>

#include <QResizeEvent>

#include <utility>

using namespace Qt::StringLiterals;

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
    , m_isElided{false}
{ }

Qt::TextElideMode ElidedLabel::elideMode() const
{
    return m_elideMode;
}

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    m_elideMode = elideMode;
    elideText(width());
}

QString ElidedLabel::text() const
{
    return m_text;
}

void ElidedLabel::setText(const QString& text)
{
    m_text = text;
    elideText(width());
}

QSize ElidedLabel::sizeHint() const
{
    const QFontMetrics fm{fontMetrics()};
    const QSize size{fm.horizontalAdvance(m_text), QLabel::sizeHint().height()};
    return size;
}

QSize ElidedLabel::minimumSizeHint() const
{
    const QFontMetrics fm{fontMetrics()};
    const QSize size{fm.horizontalAdvance(m_text.left(2) + u"…"_s), QLabel::sizeHint().height()};
    return size;
}

void ElidedLabel::resizeEvent(QResizeEvent* event)
{
    elideText(event->size().width());

    QLabel::resizeEvent(event);
}

void ElidedLabel::elideText(int width)
{
    const QFontMetrics fm{fontMetrics()};

    QString elidedText = fm.elidedText(m_text, m_elideMode, width - (margin() * 2) - indent());

    if(elidedText == "…"_L1) {
        elidedText = m_text.at(0);
    }

    m_isElided = elidedText != m_text;

    QLabel::setText(elidedText);
}
} // namespace Fooyin
