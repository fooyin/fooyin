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

#include "filterfwd.h"

#include <gui/fywidget.h>

#include <QItemSelection>

class QHBoxLayout;

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Filters {
class FilterManager;
class FilterView;
class FilterModel;

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
    void layoutEditingMenu(Utils::ActionContainer* menu) override;

    void customHeaderMenuRequested(QPoint pos);

    void saveLayout(QJsonArray& array) override;
    void loadLayout(QJsonObject& object) override;

signals:
    void typeChanged(int index);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void fieldChanged(const FilterField& field);
    void editFilter(int index, const QString& name);
    void changeOrder();
    void resetByIndex(int idx);
    void resetByType();

    FilterManager* m_manager;
    Utils::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    LibraryFilter* m_filter;
    FilterView* m_view;
    FilterModel* m_model;
    Qt::SortOrder m_sortOrder;
};
} // namespace Filters
} // namespace Fy
