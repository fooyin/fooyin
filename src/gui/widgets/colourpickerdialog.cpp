/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/colourpickerdialog.h>

#include <gui/widgets/colourpicker.h>

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace Fooyin {
ColourPickerDialog::ColourPickerDialog(const QColor& colour, QWidget* parent)
    : QDialog{parent}
    , m_picker{new ColourPicker(this)}
{
    setWindowTitle(tr("Select Colour"));

    m_picker->setOriginalColour(colour);
    m_picker->setColour(colour);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_picker);
    layout->addWidget(buttons);

    QObject::connect(this, &QDialog::accepted, this, [this] { Q_EMIT colourSelected(m_picker->colour()); });
}

QColor ColourPickerDialog::colour() const
{
    return m_picker->colour();
}
} // namespace Fooyin

#include "gui/widgets/moc_colourpickerdialog.cpp"
