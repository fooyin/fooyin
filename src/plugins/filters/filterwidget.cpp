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

#include "filterwidget.h"

#include "constants.h"
#include "filterdelegate.h"
#include "filtermanager.h"
#include "filtermodel.h"
#include "filtersettings.h"
#include "filterview.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>

#include <utils/actions/actioncontainer.h>
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>

namespace Filters {
FilterWidget::FilterWidget(FilterManager* manager, Utils::SettingsManager* settings, Filters::FilterType type,
                           QWidget* parent)
    : FyWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_type{type}
    , m_index{0}
    , m_manager{manager}
    , m_filter{new FilterView(this)}
    , m_model{new FilterModel(m_type, m_index, m_filter)}
    , m_settings{settings}
{
    setObjectName(FilterWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_filter);
    m_filter->setModel(m_model);
    m_filter->setItemDelegate(new FilterDelegate(this));

    m_index = m_manager->registerFilter(m_type);

    setupConnections();
    setHeaderHidden(m_settings->value<Settings::FilterHeader>());
    setScrollbarHidden(m_settings->value<Settings::FilterScrollBar>());
    setAltRowColors(m_settings->value<Settings::FilterAltColours>());

    resetByType(m_type);
}

FilterWidget::~FilterWidget()
{
    m_manager->unregisterFilter(m_type);
}

void FilterWidget::setupConnections()
{
    m_settings->subscribe<Settings::FilterAltColours>(this, &FilterWidget::setAltRowColors);
    m_settings->subscribe<Settings::FilterHeader>(this, &FilterWidget::setHeaderHidden);
    m_settings->subscribe<Settings::FilterScrollBar>(this, &FilterWidget::setScrollbarHidden);

    connect(m_filter->header(), &FilterView::customContextMenuRequested, this,
            &FilterWidget::customHeaderMenuRequested);
    connect(m_filter, &QTreeView::doubleClicked, this, []() {
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
    auto oldType = m_type;
    m_type       = type;
    m_model->setType(type);
    emit typeChanged(oldType, type);
    m_filter->clearSelection();
    m_filter->scrollToTop();
    resetByIndex(-1);
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
        case(Core::Library::SortOrder::TitleAsc):
            return m_manager->changeFilterOrder(m_type, Core::Library::SortOrder::TitleDesc);
        case(Core::Library::SortOrder::TitleDesc):
            return m_manager->changeFilterOrder(m_type, Core::Library::SortOrder::TitleAsc);
        case(Core::Library::SortOrder::YearAsc):
            return m_manager->changeFilterOrder(m_type, Core::Library::SortOrder::YearDesc);
        case(Core::Library::SortOrder::YearDesc):
            return m_manager->changeFilterOrder(m_type, Core::Library::SortOrder::YearAsc);
        case(Core::Library::SortOrder::NoSorting):
            return m_manager->changeFilterOrder(m_type, Core::Library::SortOrder::TitleAsc);
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

void FilterWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    auto* showHeaders = new QAction("Show Header", this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_settings->set<Settings::FilterHeader>(checked);
    });

    auto* showScrollBar = new QAction("Show Scrollbar", menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_settings->set<Settings::FilterScrollBar>(checked);
    });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction("Alternate Row Colours", this);
    altColours->setCheckable(true);
    altColours->setChecked(altRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_settings->set<Settings::FilterAltColours>(checked);
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
}

void FilterWidget::customHeaderMenuRequested(QPoint pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* orderMenu = new QMenu(menu);
    orderMenu->setTitle("Sort Order");

    const auto order = m_manager->filterOrder(m_type);

    auto* editFilters = new QActionGroup{menu};
    auto* sortOrder   = new QActionGroup{menu};

    auto* titleSort = new QAction(orderMenu);
    titleSort->setText("Title");
    titleSort->setData(QVariant::fromValue<Core::Library::SortOrder>(Core::Library::SortOrder::TitleAsc));
    titleSort->setCheckable(true);
    titleSort->setChecked(order == Core::Library::SortOrder::TitleAsc || order == Core::Library::SortOrder::TitleDesc);
    orderMenu->addAction(titleSort);

    auto* yearSort = new QAction(orderMenu);
    yearSort->setText("Year");
    yearSort->setData(QVariant::fromValue<Core::Library::SortOrder>(Core::Library::SortOrder::YearAsc));
    yearSort->setCheckable(true);
    yearSort->setChecked(order == Core::Library::SortOrder::YearAsc || order == Core::Library::SortOrder::YearDesc);
    orderMenu->addAction(yearSort);

    sortOrder->addAction(titleSort);
    sortOrder->addAction(yearSort);

    auto* genre = new QAction(menu);
    genre->setText("Genre");
    genre->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Genre));
    genre->setCheckable(true);
    genre->setChecked(m_type == Filters::FilterType::Genre);
    genre->setDisabled(!genre->isChecked() && m_manager->hasFilter(Filters::FilterType::Genre));
    menu->addAction(genre);

    auto* year = new QAction(menu);
    year->setText("Year");
    year->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Year));
    year->setCheckable(true);
    year->setChecked(m_type == Filters::FilterType::Year);
    year->setDisabled(!year->isChecked() && m_manager->hasFilter(Filters::FilterType::Year));
    menu->addAction(year);

    auto* albumArtist = new QAction(menu);
    albumArtist->setText("Album Artist");
    albumArtist->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::AlbumArtist));
    albumArtist->setCheckable(true);
    albumArtist->setChecked(m_type == Filters::FilterType::AlbumArtist);
    albumArtist->setDisabled(!albumArtist->isChecked() && m_manager->hasFilter(Filters::FilterType::AlbumArtist));
    menu->addAction(albumArtist);

    auto* artist = new QAction(menu);
    artist->setText("Artist");
    artist->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Artist));
    artist->setCheckable(true);
    artist->setChecked(m_type == Filters::FilterType::Artist);
    artist->setDisabled(!artist->isChecked() && m_manager->hasFilter(Filters::FilterType::Artist));
    menu->addAction(artist);

    auto* album = new QAction(menu);
    album->setText("Album");
    album->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Album));
    album->setCheckable(true);
    album->setChecked(m_type == Filters::FilterType::Album);
    album->setDisabled(!album->isChecked() && m_manager->hasFilter(Filters::FilterType::Album));
    menu->addAction(album);

    editFilters->addAction(genre);
    editFilters->addAction(year);
    editFilters->addAction(albumArtist);
    editFilters->addAction(artist);
    editFilters->addAction(album);

    menu->addSeparator();
    menu->addMenu(orderMenu);

    menu->setDefaultAction(editFilters->checkedAction());

    connect(editFilters, &QActionGroup::triggered, this, &FilterWidget::editFilter);
    connect(sortOrder, &QActionGroup::triggered, this, &FilterWidget::changeOrder);

    menu->popup(mapToGlobal(pos));
}

void FilterWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)
    const QModelIndexList indexes = m_filter->selectionModel()->selectedIndexes();

    if(indexes.isEmpty()) {
        return;
    }

    if(!m_model) {
        return;
    }

    Core::IdSet ids;
    for(const auto& index : indexes) {
        if(index.isValid()) {
            const int id = index.data(Filters::Constants::Role::Id).toInt();
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
    auto order = action->data().value<Core::Library::SortOrder>();
    m_manager->changeFilterOrder(m_type, order);
}

void FilterWidget::dataLoaded(Filters::FilterType type, const FilterEntries& result)
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

GenreFilter::GenreFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FilterWidget(manager, settings, Filters::FilterType::Genre, parent)
{ }

QString GenreFilter::name() const
{
    return "Genre Filter";
}

QString GenreFilter::layoutName() const
{
    return "FilterGenre";
}

YearFilter::YearFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FilterWidget(manager, settings, Filters::FilterType::Year, parent)
{ }

QString YearFilter::name() const
{
    return "Year Filter";
}

QString YearFilter::layoutName() const
{
    return "FilterYear";
}

AlbumArtistFilter::AlbumArtistFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FilterWidget(manager, settings, Filters::FilterType::AlbumArtist, parent)
{ }

QString AlbumArtistFilter::name() const
{
    return "Album Artist Filter";
}

QString AlbumArtistFilter::layoutName() const
{
    return "FilterAlbumArtist";
}

ArtistFilter::ArtistFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FilterWidget(manager, settings, Filters::FilterType::Artist, parent)
{ }

QString ArtistFilter::name() const
{
    return "Artist Filter";
}

QString ArtistFilter::layoutName() const
{
    return "FilterArtist";
}

AlbumFilter::AlbumFilter(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FilterWidget(manager, settings, Filters::FilterType::Album, parent)
{ }

QString AlbumFilter::name() const
{
    return "Album Filter";
}

QString AlbumFilter::layoutName() const
{
    return "FilterAlbum";
}

} // namespace Filters
