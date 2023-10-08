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

#pragma once

#include "fygui_export.h"

#include <QString>

namespace Fy::Gui {
struct Layout
{
    Layout() = default;
    Layout(QString name, QByteArray json)
        : name{std::move(name)}
        , json{std::move(json)}
    { }
    QString name;
    QByteArray json;
};
using LayoutList = std::vector<Layout>;

class FYGUI_EXPORT LayoutProvider
{
public:
    explicit LayoutProvider();
    ~LayoutProvider();

    void findLayouts();

    [[nodiscard]] Layout currentLayout() const;
    void loadCurrentLayout();
    void saveCurrentLayout(const QByteArray& json);

    [[nodiscard]] LayoutList layouts() const;
    void registerLayout(const QString& name, const QByteArray& json);
    void registerLayout(const QString& file);

    void importLayout();
    void exportLayout(const QByteArray& json);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Gui
