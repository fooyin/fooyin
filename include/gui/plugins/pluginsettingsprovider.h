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

#pragma once

#include "fygui_export.h"

#include <QDialog>

class QWidget;

namespace Fooyin {
class PluginSettingsProviderPrivate;

/*! Provides a single-instance plugin settings dialog. */
class FYGUI_EXPORT PluginSettingsProvider
{
public:
    PluginSettingsProvider();
    virtual ~PluginSettingsProvider();

    void showSettings(QWidget* parent);

protected:
    /*! Creates the settings dialog when no instance is currently open. */
    [[nodiscard]] virtual QDialog* createSettings(QWidget* parent) = 0;

private:
    std::unique_ptr<PluginSettingsProviderPrivate> p;
};
} // namespace Fooyin
