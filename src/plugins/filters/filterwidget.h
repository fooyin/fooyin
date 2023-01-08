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

#include "filterfwd.h"

#include <QItemSelection>
#include <core/gui/fywidget.h>
#include <core/typedefs.h>

class QHBoxLayout;

namespace Core {
class Settings;

namespace Player {
class PlayerManager;
};

namespace Library {
class MusicLibrary;
};

namespace Widgets {
class WidgetProvider;
};
}; // namespace Core

namespace Filters {
class FilterManager;
class FilterView;
class FilterModel;

class FilterWidget : public Core::Widgets::FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(FilterType type = FilterType::AlbumArtist, QWidget* parent = nullptr);
    ~FilterWidget() override;

    void setupConnections();

    Filters::FilterType type();
    void setType(Filters::FilterType type);

    [[nodiscard]] int index() const;
    void setIndex(int index);

    void switchOrder();

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    bool altRowColors();
    void setAltRowColors(bool altColours);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Core::ActionContainer* menu) override;
    void saveLayout(QJsonArray& array) override;
    void loadLayout(QJsonObject& object) override;

    void customHeaderMenuRequested(QPoint pos);

signals:
    void typeChanged(int index);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void editFilter(QAction* action);
    void changeOrder(QAction* action);
    void dataLoaded(Filters::FilterType type, const FilterEntries& result);
    void resetByIndex(int idx);
    void resetByType(Filters::FilterType type);

private:
    QHBoxLayout* m_layout;
    Filters::FilterType m_type;
    int m_index;
    FilterManager* m_manager;
    FilterView* m_filter;
    FilterModel* m_model;
    Core::Settings* m_settings;
};

class GenreFilter : public FilterWidget
{
public:
    explicit GenreFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::Genre, parent)
    { }
    ~GenreFilter() override = default;
};

class YearFilter : public FilterWidget
{
public:
    explicit YearFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::Year, parent)
    { }
    ~YearFilter() override = default;
};

class AlbumArtistFilter : public FilterWidget
{
public:
    explicit AlbumArtistFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::AlbumArtist, parent)
    { }
    ~AlbumArtistFilter() override = default;
};

class ArtistFilter : public FilterWidget
{
public:
    explicit ArtistFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::Artist, parent)
    { }
    ~ArtistFilter() override = default;
};

class AlbumFilter : public FilterWidget
{
public:
    explicit AlbumFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::Album, parent)
    { }
    ~AlbumFilter() override = default;
};
}; // namespace Filters
