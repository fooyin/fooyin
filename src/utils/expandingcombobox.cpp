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

#include <utils/expandingcombobox.h>

#include <QAbstractItemView>

namespace Fy::Utils {
ExpandingComboBox::ExpandingComboBox(QWidget* parent)
    : QComboBox{parent}
{
    QObject::connect(this, &QComboBox::currentIndexChanged, this, &ExpandingComboBox::resizeToFitCurrent);
}

void ExpandingComboBox::resizeToFitCurrent()
{
    const QFontMetrics fontMetrics{font()};

    int maxWidth{0};

    const QStringList lines = currentText().split('\n');

    for(const QString& line : lines) {
        maxWidth = std::max(maxWidth, fontMetrics.horizontalAdvance(line));
    }

    const int totalLineHeight = fontMetrics.lineSpacing() * static_cast<int>(lines.size());
    const int maxHeight       = totalLineHeight + fontMetrics.height();

    setMinimumSize({maxWidth, maxHeight});
}

void ExpandingComboBox::resizeDropDown()
{
    const QFontMetrics fontMetrics{font()};

    int maxWidth{0};

    for(int i = 0; i < count(); ++i) {
        maxWidth = std::max(maxWidth, fontMetrics.horizontalAdvance(itemText(i)));
    }

    if(view()->minimumWidth() < maxWidth) {
        maxWidth += style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        maxWidth += view()->autoScrollMargin();
        view()->setMinimumWidth(maxWidth);
    }
}
} // namespace Fy::Utils
