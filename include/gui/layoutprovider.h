/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

class FYGUI_EXPORT LayoutProvider : public QObject
{
    Q_OBJECT

public:
    explicit LayoutProvider(QObject* parent = nullptr);
    ~LayoutProvider() override;

    [[nodiscard]] FyLayout currentLayout() const;
    [[nodiscard]] LayoutList layouts() const;
    [[nodiscard]] FyLayout layoutByName(const QString& name) const;
    [[nodiscard]] QString uniqueLayoutName(const QString& name) const;
    [[nodiscard]] bool canDeleteLayout(const QString& name) const;
    [[nodiscard]] bool canResetLayout(const QString& name) const;

    void findLayouts() const;
    void saveCurrentLayout();

    void registerLayout(const FyLayout& layout);
    void registerLayout(const QByteArray& json);
    void changeLayout(const FyLayout& layout);
    bool createLayout(const QString& name, const FyLayout& baseLayout);
    bool deleteLayout(const QString& name);
    bool renameLayout(const QString& oldName, const QString& newName);
    bool duplicateLayout(const QString& sourceName, const QString& newName);
    bool resetLayout(const QString& name);

    FyLayout importLayout(const QString& path);
    void importLayout(QWidget* parent);
    bool exportLayout(const FyLayout& layout, const QString& path);

Q_SIGNALS:
    void layoutAdded(const Fooyin::FyLayout& layout);
    void layoutChanged(const Fooyin::FyLayout& layout);
    void layoutRemoved(const QString& name);
    void currentLayoutChanged(const Fooyin::FyLayout& layout);
    void requestChangeLayout(const Fooyin::FyLayout& layout);

private:
    std::unique_ptr<LayoutProviderPrivate> p;
};
} // namespace Fooyin
