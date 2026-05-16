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

#include <gui/dsp/dsplayouteditor.h>

namespace Fooyin {
DspLayoutEditor::DspLayoutEditor(QWidget* parent)
    : QWidget{parent}
{ }

DspLayoutEditor::~DspLayoutEditor() = default;

void DspLayoutEditor::setControlsEnabled(const bool enabled)
{
    const auto childWidgets = findChildren<QWidget*>(QString{}, Qt::FindDirectChildrenOnly);
    for(auto* widget : childWidgets) {
        widget->setEnabled(enabled);
        widget->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    }
}

void DspLayoutEditor::restoreDefaults() { }

void DspLayoutEditor::populateContextMenu(QMenu* /*menu*/) { }

void DspLayoutEditor::saveLayoutData(QJsonObject& /*layout*/) { }

void DspLayoutEditor::loadLayoutData(const QJsonObject& /*layout*/) { }
} // namespace Fooyin

#include "gui/dsp/moc_dsplayouteditor.cpp"
