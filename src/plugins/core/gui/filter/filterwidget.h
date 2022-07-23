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
#include "utils/utils.h"

#include <QItemSelection>

class Settings;
class PlayerManager;
class QHBoxLayout;
class WidgetProvider;

namespace Library {
class FilterView;
class MusicLibrary;

class FilterWidget : public Widget
{
    Q_OBJECT

public:
    explicit FilterWidget(Filters::FilterType type, QWidget* parent = nullptr);
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
    void layoutEditingMenu(QMenu* menu) override;
    void saveLayout(QJsonArray& array) override;

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
    Filters::FilterType m_type;
    int m_index;
    MusicLibrary* m_library;
    Library::FilterView* m_filter;
    FilterModel* m_model;
    Settings* m_settings;
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

class AlbmArtistFilter : public FilterWidget
{
public:
    explicit AlbmArtistFilter(QWidget* parent = nullptr)
        : FilterWidget(Filters::FilterType::AlbumArtist, parent)
    { }
    ~AlbmArtistFilter() override = default;
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

}; // namespace Library
