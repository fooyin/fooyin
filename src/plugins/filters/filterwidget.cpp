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

namespace Fy::Filters {
FilterWidget::FilterWidget(FilterManager* manager, Utils::SettingsManager* settings, Filters::FilterType type,
                           QWidget* parent)
    : FyWidget{parent}
    , m_manager{manager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_filter{m_manager->registerFilter(type)}
    , m_view{new FilterView(this)}
    , m_model{new FilterModel(type, this)}
{
    setObjectName(FilterWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);

    m_view->setModel(m_model);
    m_view->setItemDelegate(new FilterDelegate(this));

    m_layout->addWidget(m_view);

    setupConnections();
    setHeaderHidden(m_settings->value<Settings::FilterHeader>());
    setScrollbarHidden(m_settings->value<Settings::FilterScrollBar>());
    setAltRowColors(m_settings->value<Settings::FilterAltColours>());

    resetByType();
}

FilterWidget::~FilterWidget()
{
    m_manager->unregisterFilter(m_filter->type);
}

void FilterWidget::setupConnections()
{
    m_settings->subscribe<Settings::FilterAltColours>(this, &FilterWidget::setAltRowColors);
    m_settings->subscribe<Settings::FilterHeader>(this, &FilterWidget::setHeaderHidden);
    m_settings->subscribe<Settings::FilterScrollBar>(this, &FilterWidget::setScrollbarHidden);

    connect(m_view->header(), &FilterView::customContextMenuRequested, this, &FilterWidget::customHeaderMenuRequested);
    connect(m_view, &QTreeView::doubleClicked, this, []() {
        //        m_library->prepareTracks();
    });
    //    connect(m_view->header(), &QHeaderView::sectionClicked, this, &FilterWidget::switchOrder);
    connect(this, &FilterWidget::typeChanged, m_manager, &FilterManager::changeFilter);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilterWidget::selectionChanged);

    connect(m_manager, &FilterManager::filteredItems, this, &FilterWidget::resetByIndex);
}

Filters::FilterType FilterWidget::type()
{
    return m_filter->type;
}

void FilterWidget::setType(Filters::FilterType newType)
{
    m_filter->type = newType;
    m_model->setType(newType);
    emit typeChanged(m_filter->index);
    m_view->clearSelection();
    m_view->scrollToTop();
    resetByIndex(-1);
}

bool FilterWidget::isHeaderHidden()
{
    return m_view->isHeaderHidden();
}

void FilterWidget::setHeaderHidden(bool showHeader)
{
    m_view->setHeaderHidden(!showHeader);
}

bool FilterWidget::isScrollbarHidden()
{
    return m_view->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void FilterWidget::setScrollbarHidden(bool showScrollBar)
{
    m_view->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

bool FilterWidget::altRowColors()
{
    return m_view->alternatingRowColors();
}

void FilterWidget::setAltRowColors(bool altColours)
{
    m_view->setAlternatingRowColors(altColours);
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

    auto* editFilters = new QActionGroup{menu};

    const FilterType type = m_filter->type;

    auto* genre = new QAction(menu);
    genre->setText("Genre");
    genre->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Genre));
    genre->setCheckable(true);
    genre->setChecked(type == Filters::FilterType::Genre);
    genre->setDisabled(!genre->isChecked() && m_manager->hasFilter(Filters::FilterType::Genre));
    menu->addAction(genre);

    auto* year = new QAction(menu);
    year->setText("Year");
    year->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Year));
    year->setCheckable(true);
    year->setChecked(type == Filters::FilterType::Year);
    year->setDisabled(!year->isChecked() && m_manager->hasFilter(Filters::FilterType::Year));
    menu->addAction(year);

    auto* albumArtist = new QAction(menu);
    albumArtist->setText("Album Artist");
    albumArtist->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::AlbumArtist));
    albumArtist->setCheckable(true);
    albumArtist->setChecked(type == Filters::FilterType::AlbumArtist);
    albumArtist->setDisabled(!albumArtist->isChecked() && m_manager->hasFilter(Filters::FilterType::AlbumArtist));
    menu->addAction(albumArtist);

    auto* artist = new QAction(menu);
    artist->setText("Artist");
    artist->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Artist));
    artist->setCheckable(true);
    artist->setChecked(type == Filters::FilterType::Artist);
    artist->setDisabled(!artist->isChecked() && m_manager->hasFilter(Filters::FilterType::Artist));
    menu->addAction(artist);

    auto* album = new QAction(menu);
    album->setText("Album");
    album->setData(QVariant::fromValue<Filters::FilterType>(Filters::FilterType::Album));
    album->setCheckable(true);
    album->setChecked(type == Filters::FilterType::Album);
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

    menu->popup(mapToGlobal(pos));
}

void FilterWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    m_filter->tracks.clear();

    const QModelIndexList indexes = m_view->selectionModel()->selectedIndexes();

    if(indexes.isEmpty()) {
        return;
    }

    if(!m_model) {
        return;
    }

    Core::TrackPtrList tracks;
    for(const auto& index : indexes) {
        if(index.isValid()) {
            const auto newTracks = index.data(Filters::Constants::Role::Tracks).value<Core::TrackPtrList>();
            tracks.insert(tracks.end(), newTracks.cbegin(), newTracks.cend());
        }
    }
    m_manager->selectionChanged(*m_filter, tracks);
}

void FilterWidget::editFilter(QAction* action)
{
    auto type = action->data().value<Filters::FilterType>();
    setType(type);
}

void FilterWidget::changeOrder(QAction* action)
{
    auto order = action->data().value<Core::Library::SortOrder>();
    Q_UNUSED(order)
}

void FilterWidget::resetByIndex(int idx)
{
    if(idx < m_filter->index) {
        m_model->reload(m_manager->tracks());
    }
}

void FilterWidget::resetByType()
{
    m_model->reload(m_manager->tracks());
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

} // namespace Fy::Filters
