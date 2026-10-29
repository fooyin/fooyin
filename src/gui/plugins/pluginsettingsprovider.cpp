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

#include <gui/plugins/pluginsettingsprovider.h>

#include <QDialog>
#include <QPointer>

namespace Fooyin {
class PluginSettingsProviderPrivate
{
public:
    QPointer<QDialog> m_dialog;
};

PluginSettingsProvider::PluginSettingsProvider()
    : p{std::make_unique<PluginSettingsProviderPrivate>()}
{ }

PluginSettingsProvider::~PluginSettingsProvider() = default;

void PluginSettingsProvider::showSettings(QWidget* parent)
{
    if(p->m_dialog) {
        p->m_dialog->show();
        p->m_dialog->raise();
        p->m_dialog->activateWindow();
        return;
    }

    p->m_dialog = createSettings(parent);
    if(!p->m_dialog) {
        return;
    }

    p->m_dialog->setAttribute(Qt::WA_DeleteOnClose);
    p->m_dialog->show();
}
} // namespace Fooyin