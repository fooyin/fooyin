/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/theme/fytheme.h>
#include <utils/itemregistry.h>

namespace Fooyin {
class SettingsManager;

class FYGUI_EXPORT ThemeRegistry : public ItemRegistry<FyTheme>
{
    Q_OBJECT

public:
    struct FontEntry
    {
        QString title;
        QString parentClassName;
    };

    explicit ThemeRegistry(SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] std::map<QString, FontEntry> fontEntries() const;
    void registerFontEntry(const QString& title, const QString& className, const QString& parentClassName = {});

    static QFont defaultFont(const QString& className, const QFont& systemFont);
    [[nodiscard]] QFont resolveFont(const QString& className, const FyTheme& theme, const QFont& systemFont) const;

protected:
    void loadDefaults() override;

private:
    std::map<QString, FontEntry> m_fontEntries;
};
} // namespace Fooyin
