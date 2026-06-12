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

#include <gui/guistyleprovider.h>

#include <utils/settings/settingsmanager.h>

namespace Fooyin {
GuiStyleProvider::GuiStyleProvider(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_isResolved{false}
{
    settings->subscribe<Settings::Gui::ResolvedAppStyle>(
        this, [this](const QVariant& var) { updateStyle(var.value<ResolvedAppStyle>()); });
    updateStyle(settings->value<Settings::Gui::ResolvedAppStyle>().value<ResolvedAppStyle>());
}

bool GuiStyleProvider::isResolved() const
{
    return m_isResolved;
}

const ResolvedAppStyle& GuiStyleProvider::style() const
{
    return m_style;
}

QPalette GuiStyleProvider::palette() const
{
    return m_style.palette;
}

QFont GuiStyleProvider::font(const QString& className) const
{
    return m_style.font(className);
}

void GuiStyleProvider::updateStyle(const ResolvedAppStyle& style)
{
    if(style.revision == 0) {
        return;
    }

    m_style      = style;
    m_isResolved = true;
    Q_EMIT styleChanged(m_style);
}
} // namespace Fooyin

#include "gui/moc_guistyleprovider.cpp"
