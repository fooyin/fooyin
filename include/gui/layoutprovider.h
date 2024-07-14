/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fylayout.h>

#include <QJsonObject>
#include <QString>

namespace Fooyin {
class LayoutProviderPrivate;

class FYGUI_EXPORT LayoutProvider
{
public:
    explicit LayoutProvider();
    ~LayoutProvider();

    [[nodiscard]] FyLayout currentLayout() const;
    [[nodiscard]] LayoutList layouts() const;

    void findLayouts();
    void loadCurrentLayout();
    void saveCurrentLayout();

    void registerLayout(const FyLayout& layout);
    void registerLayout(const QByteArray& json);
    void changeLayout(const FyLayout& layout);

    FyLayout importLayout(const QString& path);
    bool exportLayout(const FyLayout& layout, const QString& path);

private:
    std::unique_ptr<LayoutProviderPrivate> p;
};
} // namespace Fooyin
