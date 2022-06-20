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

#include "filtermodel.h"
#include "gui/widgets/widget.h"
#include "utils/typedefs.h"

#include <QItemSelection>

class Settings;
class PlayerManager;
class QHBoxLayout;

namespace Library {
class FilterView;
class MusicLibrary;

class FilterWidget : public Widget
{
    Q_OBJECT

public:
    FilterWidget(Filters::FilterType type, int index, PlayerManager* playerManager, MusicLibrary* library,
                 QWidget* parent = nullptr);
    ~FilterWidget() override;

    void setupConnections();

    Filters::FilterType type();
    void setType(Filters::FilterType type);

    [[nodiscard]] int index() const;
    void setIndex(int index);

    void switchOrder();

    bool isHeaderHidden();
    void setHeaderHidden(bool b);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool b);

    bool altRowColors();
    void setAltRowColors(bool b);

    void layoutEditingMenu(QMenu* menu) override;
    void customHeaderMenuRequested(QPoint pos);

signals:
    void typeChanged(int index);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void editFilter(QAction* action);
    void changeOrder(QAction* action);
    void dataLoaded(Filters::FilterType type, const FilterList& result);
    void resetByIndex(int idx);
    void resetByType(Filters::FilterType type);

private:
    QHBoxLayout* m_layout;
    int m_index;
    Filters::FilterType m_type;
    MusicLibrary* m_library;
    Library::FilterView* m_filter;
    FilterModel* m_model;
    Settings* m_settings;
};
}; // namespace Library
