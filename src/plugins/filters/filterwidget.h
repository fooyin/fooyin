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
    explicit FilterWidget(FilterManager* manager, Utils::SettingsManager* settings,
                          FilterType type = FilterType::AlbumArtist, QWidget* parent = nullptr);
    ~FilterWidget() override;

    void setupConnections();

    Filters::FilterType type();
    void setType(Filters::FilterType newType);

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    bool altRowColors();
    void setAltRowColors(bool altColours);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Utils::ActionContainer* menu) override;

    void customHeaderMenuRequested(QPoint pos);

signals:
    void typeChanged(int index);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void editFilter(QAction* action);
    void changeOrder(QAction* action);
    void resetByIndex(int idx);
    void resetByType();

    FilterManager* m_manager;
    Utils::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    LibraryFilter* m_filter;
    FilterView* m_view;
    FilterModel* m_model;
};

class GenreFilter : public FilterWidget
{
public:
    explicit GenreFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
};

class YearFilter : public FilterWidget
{
public:
    explicit YearFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
};

class AlbumArtistFilter : public FilterWidget
{
public:
    explicit AlbumArtistFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
};

class ArtistFilter : public FilterWidget
{
public:
    explicit ArtistFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
};

class AlbumFilter : public FilterWidget
{
public:
    explicit AlbumFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
};
} // namespace Filters
} // namespace Fy
