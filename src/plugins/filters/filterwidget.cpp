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

#include "filterwidget.h"

#include "filterdelegate.h"
#include "filtermanager.h"
#include "filtermodel.h"
#include "filterview.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/settings/settings.h>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

namespace Library {
FilterWidget::FilterWidget(Filters::FilterType type, QWidget* parent)
    : FyWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_type(type)
    , m_index(0)
    , m_manager(PluginSystem::object<FilterManager>())
    , m_filter(new Library::FilterView(PluginSystem::object<PlayerManager>(), this))
    , m_model(new FilterModel(m_type, m_index, m_filter))
    , m_settings(PluginSystem::object<Settings>())
{
    setObjectName(FilterWidget::name());
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_filter);
    m_filter->setModel(m_model);
    m_filter->setItemDelegate(new FilterDelegate(this));
    m_index = m_manager->registerFilter(m_type);
    setupConnections();
    setHeaderHidden(m_settings->value(Settings::Setting::FilterHeader).toBool());
    setScrollbarHidden(m_settings->value(Settings::Setting::FilterScrollBar).toBool());
    setAltRowColors(m_settings->value(Settings::Setting::FilterAltColours).toBool());

    resetByIndex(-1);
}

FilterWidget::~FilterWidget()
{
    m_manager->unregisterFilter(m_index);
}

void FilterWidget::setupConnections()
{
    connect(m_settings, &Settings::filterAltColorsChanged, this, &FilterWidget::setAltRowColors);
    connect(m_settings, &Settings::filterHeaderChanged, this, &FilterWidget::setHeaderHidden);
    connect(m_settings, &Settings::filterScrollBarChanged, this, &FilterWidget::setScrollbarHidden);

    connect(m_filter->header(), &FilterView::customContextMenuRequested, this,
            &FilterWidget::customHeaderMenuRequested);
    connect(m_filter, &QTreeView::doubleClicked, this, [this] {
        //        m_library->prepareTracks();
    });
    connect(m_filter->header(), &QHeaderView::sectionClicked, this, &FilterWidget::switchOrder);
    connect(this, &FilterWidget::typeChanged, m_manager, &FilterManager::changeFilter);
    //    connect(m_model, &FilterModel::modelReset, this, [this] {
    //        m_library->resetFilter(m_type);
    //    });
    connect(m_filter->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilterWidget::selectionChanged);

    connect(m_manager, &FilterManager::filteredItems, this, &FilterWidget::resetByIndex);
    connect(m_manager, &FilterManager::orderedFilter, this, &FilterWidget::resetByType);
    connect(m_manager, &FilterManager::itemsLoaded, this, &FilterWidget::dataLoaded);
}

Filters::FilterType FilterWidget::type()
{
    return m_type;
}

void FilterWidget::setType(Filters::FilterType type)
{
    m_type = type;
    m_model->setType(type);
    emit typeChanged(m_index);
    m_manager->registerFilter(m_type);
    m_filter->clearSelection();
    m_filter->scrollToTop();
}

int FilterWidget::index() const
{
    return m_index;
}

void FilterWidget::setIndex(int index)
{
    m_index = index;
    m_model->setIndex(index);
}

void FilterWidget::switchOrder()
{
    const auto order = m_manager->filterOrder(m_type);
    switch(order) {
        case(Library::SortOrder::TitleAsc):
            return m_manager->changeFilterOrder(m_type, Library::SortOrder::TitleDesc);
        case(Library::SortOrder::TitleDesc):
            return m_manager->changeFilterOrder(m_type, Library::SortOrder::TitleAsc);
        case(Library::SortOrder::YearAsc):
            return m_manager->changeFilterOrder(m_type, Library::SortOrder::YearDesc);
        case(Library::SortOrder::YearDesc):
            return m_manager->changeFilterOrder(m_type, Library::SortOrder::YearAsc);
        case(Library::SortOrder::NoSorting):
            return m_manager->changeFilterOrder(m_type, Library::SortOrder::TitleAsc);
    }
}

bool FilterWidget::isHeaderHidden()
{
    return m_filter->isHeaderHidden();
}

void FilterWidget::setHeaderHidden(bool showHeader)
{
    m_filter->setHeaderHidden(!showHeader);
}

bool FilterWidget::isScrollbarHidden()
{
    return m_filter->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void FilterWidget::setScrollbarHidden(bool showScrollBar)
{
    m_filter->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

bool FilterWidget::altRowColors()
{
    return m_filter->alternatingRowColors();
}

void FilterWidget::setAltRowColors(bool altColours)
{
    m_filter->setAlternatingRowColors(altColours);
}

QString FilterWidget::name() const
{
    return "Filter";
}

void FilterWidget::layoutEditingMenu(QMenu* menu)
{
    auto* showHeaders = new QAction("Show Header", this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::Setting::FilterHeader, checked);
    });

    auto* showScrollBar = new QAction("Show Scrollbar", menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::Setting::FilterScrollBar, checked);
    });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction("Alternate Row Colours", this);
    altColours->setCheckable(true);
    altColours->setChecked(altRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::Setting::FilterAltColours, checked);
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
}

void FilterWidget::saveLayout(QJsonArray& array)
{
    QJsonObject object;
    QJsonObject filterOptions;
    filterOptions["Type"] = EnumHelper::toString<Filters::FilterType>(type());
    object[name()] = filterOptions;
    array.append(object);
}

void FilterWidget::loadLayout(QJsonObject& object)
{
    auto type = EnumHelper::fromString<Filters::FilterType>(object["Type"].toString());
    setType(type);
}

void FilterWidget::customHeaderMenuRequested(QPoint pos)
{
    QMenu menu;
    QMenu orderMenu;
    orderMenu.setTitle("Sort Order");

    const auto filters = m_manager->filters();
    const auto order = m_manager->filterOrder(m_type);

    QActionGroup editFilters{&menu};
    QActionGroup sortOrder{&menu};

    QAction titleSort;
    titleSort.setText("Title");
    titleSort.setData(QVariant::fromValue<Library::SortOrder>(Library::SortOrder::TitleAsc));
    titleSort.setCheckable(true);
    titleSort.setChecked(order == Library::SortOrder::TitleAsc || order == Library::SortOrder::TitleDesc);
    orderMenu.addAction(&titleSort);

    QAction yearSort;
    yearSort.setText("Year");
    yearSort.setData(QVariant::fromValue<Library::SortOrder>(Library::SortOrder::YearAsc));
    yearSort.setCheckable(true);
    yearSort.setChecked(order == Library::SortOrder::YearAsc || order == Library::SortOrder::YearDesc);
    orderMenu.addAction(&yearSort);

    sortOrder.addAction(&titleSort);
    sortOrder.addAction(&yearSort);

    QAction genre;
    genre.setText("Genre");
    genre.setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Genre));
    genre.setCheckable(true);
    genre.setChecked(m_type == Filters::FilterType::Genre);
    genre.setDisabled(!genre.isChecked() && filters.contains(Filters::FilterType::Genre));
    menu.addAction(&genre);

    QAction year;
    year.setText("Year");
    year.setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Year));
    year.setCheckable(true);
    year.setChecked(m_type == Filters::FilterType::Year);
    year.setDisabled(!year.isChecked() && filters.contains(Filters::FilterType::Year));
    menu.addAction(&year);

    QAction albumArtist;
    albumArtist.setText("Album Artist");
    albumArtist.setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::AlbumArtist));
    albumArtist.setCheckable(true);
    albumArtist.setChecked(m_type == Filters::FilterType::AlbumArtist);
    albumArtist.setDisabled(!albumArtist.isChecked() && filters.contains(Filters::FilterType::AlbumArtist));
    menu.addAction(&albumArtist);

    QAction artist;
    artist.setText("Artist");
    artist.setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Artist));
    artist.setCheckable(true);
    artist.setChecked(m_type == Filters::FilterType::Artist);
    artist.setDisabled(!artist.isChecked() && filters.contains(Filters::FilterType::Artist));
    menu.addAction(&artist);

    QAction album;
    album.setText("Album");
    album.setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Album));
    album.setCheckable(true);
    album.setChecked(m_type == Filters::FilterType::Album);
    album.setDisabled(!album.isChecked() && filters.contains(Filters::FilterType::Album));
    menu.addAction(&album);

    editFilters.addAction(&genre);
    editFilters.addAction(&year);
    editFilters.addAction(&albumArtist);
    editFilters.addAction(&artist);
    editFilters.addAction(&album);

    menu.addSeparator();
    menu.addMenu(&orderMenu);

    menu.setDefaultAction(editFilters.checkedAction());

    connect(&editFilters, &QActionGroup::triggered, this, &FilterWidget::editFilter);
    connect(&sortOrder, &QActionGroup::triggered, this, &FilterWidget::changeOrder);

    menu.exec(mapToGlobal(pos));
}

void FilterWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)
    QModelIndexList indexes = m_filter->selectionModel()->selectedIndexes();

    if(indexes.isEmpty()) {
        return;
    }

    if(!m_model) {
        return;
    }

    IdSet ids;
    for(const auto& index : indexes) {
        if(index.isValid()) {
            const int id = index.data(FilterRole::Id).toInt();
            ids.insert(id);
        }
    }
    m_manager->selectionChanged(ids, m_type, m_index);
}

void FilterWidget::editFilter(QAction* action)
{
    auto type = action->data().value<Filters::FilterType>();
    setType(type);
}

void FilterWidget::changeOrder(QAction* action)
{
    auto order = action->data().value<Library::SortOrder>();
    m_manager->changeFilterOrder(m_type, order);
}

void FilterWidget::dataLoaded(Filters::FilterType type, const FilterList& result)
{
    if(type != m_type) {
        return;
    }
    m_model->reload(result);
}

void FilterWidget::resetByIndex(int idx)
{
    if(idx < index()) {
        m_manager->items(m_type);
    }
}

void FilterWidget::resetByType(Filters::FilterType type)
{
    if(type == m_type) {
        m_manager->items(m_type);
    }
}

} // namespace Library
