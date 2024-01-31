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

#include <QJsonObject>
#include <QString>

namespace Fooyin {
struct Layout
{
    Layout() = default;
    Layout(QString name_, QJsonObject json_)
        : name{std::move(name_)}
        , json{std::move(json_)}
    { }
    QString name;
    QJsonObject json;
};
using LayoutList = std::vector<Layout>;

class FYGUI_EXPORT LayoutProvider
{
public:
    explicit LayoutProvider();
    ~LayoutProvider();

    [[nodiscard]] Layout currentLayout() const;
    [[nodiscard]] LayoutList layouts() const;

    void findLayouts();
    void loadCurrentLayout();
    void saveCurrentLayout();

    void registerLayout(const QByteArray& file);
    void changeLayout(const Layout& layout);

    static std::optional<Layout> readLayout(const QByteArray& json);

    std::optional<Layout> importLayout(const QString& path);
    bool exportLayout(const Layout& layout, const QString& path);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
