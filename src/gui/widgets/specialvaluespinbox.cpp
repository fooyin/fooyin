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

#include <gui/widgets/specialvaluespinbox.h>

#include <QLineEdit>
#include <QStyleOptionSpinBox>

namespace Fooyin {
QString SpecialValueSpinBox::specialValue(int val)
{
    if(m_specialValues.contains(val)) {
        return m_specialValues.at(val);
    }
    return {};
}

void SpecialValueSpinBox::addSpecialValue(int val, const QString& text)
{
    m_specialValues.emplace(val, text);
    updateSize();
}

void SpecialValueSpinBox::setSpecialPrefix(const QString& prefix)
{
    m_prefix = prefix;
    updateSize();
}

void SpecialValueSpinBox::setSpecialSuffix(const QString& suffix)
{
    m_suffix = suffix;
    updateSize();
}

QSize SpecialValueSpinBox::sizeHint() const
{
    if(m_cachedSizeHint.isEmpty()) {
        ensurePolished();

        const QFontMetrics fm{fontMetrics()};
        const int height = lineEdit()->sizeHint().height();
        int width{0};

        const QString fixedContent = m_prefix + m_suffix + u' ';
        QString text               = textFromValue(minimum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));
        text  = textFromValue(maximum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));

        for(const auto& [_, specialText] : m_specialValues) {
            width = std::max(width, fm.horizontalAdvance(specialText));
        }

        width += 2; // cursor blinking space

        QStyleOptionSpinBox opt;
        initStyleOption(&opt);
        const QSize hint{width, height};

        m_cachedSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this);
    }

    return m_cachedSizeHint;
}

QSize SpecialValueSpinBox::minimumSizeHint() const
{
    if(m_cachedMinimumSizeHint.isEmpty()) {
        ensurePolished();

        const QFontMetrics fm{fontMetrics()};
        const int height = lineEdit()->minimumSizeHint().height();
        int width{0};

        const QString fixedContent = m_prefix + u' ';
        QString text               = textFromValue(minimum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));
        text  = textFromValue(maximum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));

        for(const auto& [_, specialText] : m_specialValues) {
            width = std::max(width, fm.horizontalAdvance(specialText));
        }

        width += 2; // cursor blinking space

        QStyleOptionSpinBox opt;
        initStyleOption(&opt);
        const QSize hint{width, height};

        m_cachedMinimumSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this);
    }

    return m_cachedMinimumSizeHint;
}

int SpecialValueSpinBox::valueFromText(const QString& text) const
{
    for(const auto& [val, str] : m_specialValues) {
        if(str == text) {
            return val;
        }
    }
    return QSpinBox::valueFromText(text);
}

QString SpecialValueSpinBox::textFromValue(int val) const
{
    if(m_specialValues.contains(val)) {
        return m_specialValues.at(val);
    }
    return m_prefix + QSpinBox::textFromValue(val) + m_suffix;
}

void SpecialValueSpinBox::updateSize()
{
    m_cachedSizeHint        = {};
    m_cachedMinimumSizeHint = {};
    updateGeometry();
}

QString SpecialValueDoubleSpinBox::specialValue(double val)
{
    if(m_specialValues.contains(val)) {
        return m_specialValues.at(val);
    }
    return {};
}

void SpecialValueDoubleSpinBox::addSpecialValue(double val, const QString& text)
{
    m_specialValues.emplace(val, text);
    updateSize();
}

void SpecialValueDoubleSpinBox::setSpecialPrefix(const QString& prefix)
{
    m_prefix = prefix;
    updateSize();
}

void SpecialValueDoubleSpinBox::setSpecialSuffix(const QString& suffix)
{
    m_suffix = suffix;
    updateSize();
}

QSize SpecialValueDoubleSpinBox::sizeHint() const
{
    if(m_cachedSizeHint.isEmpty()) {
        ensurePolished();

        const QFontMetrics fm{fontMetrics()};
        const int height = lineEdit()->sizeHint().height();
        int width{0};

        const QString fixedContent = m_prefix + m_suffix + u' ';
        QString text               = textFromValue(minimum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));
        text  = textFromValue(maximum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));

        for(const auto& [_, specialText] : m_specialValues) {
            width = std::max(width, fm.horizontalAdvance(specialText));
        }

        width += 2; // cursor blinking space

        QStyleOptionSpinBox opt;
        initStyleOption(&opt);
        const QSize hint{width, height};

        m_cachedSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this);
    }

    return m_cachedSizeHint;
}

QSize SpecialValueDoubleSpinBox::minimumSizeHint() const
{
    if(m_cachedMinimumSizeHint.isEmpty()) {
        ensurePolished();

        const QFontMetrics fm{fontMetrics()};
        const int height = lineEdit()->minimumSizeHint().height();
        int width{0};

        const QString fixedContent = m_prefix + u' ';
        QString text               = textFromValue(minimum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));
        text  = textFromValue(maximum());
        text.truncate(18);
        text += fixedContent;
        width = std::max(width, fm.horizontalAdvance(text));

        for(const auto& [_, specialText] : m_specialValues) {
            width = std::max(width, fm.horizontalAdvance(specialText));
        }

        width += 2; // cursor blinking space

        QStyleOptionSpinBox opt;
        initStyleOption(&opt);
        const QSize hint{width, height};

        m_cachedMinimumSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this);
    }

    return m_cachedMinimumSizeHint;
}

double SpecialValueDoubleSpinBox::valueFromText(const QString& text) const
{
    for(const auto& [val, str] : m_specialValues) {
        if(str == text) {
            return val;
        }
    }
    return QDoubleSpinBox::valueFromText(text);
}

QString SpecialValueDoubleSpinBox::textFromValue(double val) const
{
    if(m_specialValues.contains(val)) {
        return m_specialValues.at(val);
    }
    return m_prefix + QDoubleSpinBox::textFromValue(val) + m_suffix;
}

void SpecialValueDoubleSpinBox::updateSize()
{
    m_cachedSizeHint        = {};
    m_cachedMinimumSizeHint = {};
    updateGeometry();
}
} // namespace Fooyin
