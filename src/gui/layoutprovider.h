/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QFile>
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

class LayoutProvider
{
public:
    using LayoutList = std::vector<Layout>;

    explicit LayoutProvider();

    void findLayouts();

    [[nodiscard]] Layout currentLayout() const;
    void loadCurrentLayout();
    void saveCurrentLayout(const QByteArray& json);

    [[nodiscard]] LayoutList layouts() const;
    void registerLayout(const QString& name, const QByteArray& json);
    void registerLayout(const QString& file);

private:
    void addLayout(const QString& file);

    LayoutList m_layouts;
    Layout m_currentLayout;
    QFile m_layoutFile;
};
} // namespace Fy::Gui
