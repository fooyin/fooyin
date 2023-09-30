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

#include <gui/fywidget.h>

#include <QItemSelection>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Filters {
class FilterManager;

class FilterWidget : public Gui::Widgets::FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);
    ~FilterWidget() override;

    void setupConnections();

    void setField(const QString& name);

    bool isHeaderEnabled();
    void setHeaderEnabled(bool enabled);

    bool isScrollbarEnabled();
    void setScrollbarEnabled(bool enabled);

    bool hasAltColors();
    void setAltColors(bool enabled);

    [[nodiscard]] QString name() const override;

    void customHeaderMenuRequested(QPoint pos);

    void saveLayout(QJsonArray& array) override;
    void loadLayout(const QJsonObject& object) override;

signals:
    void typeChanged(int index);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fy
