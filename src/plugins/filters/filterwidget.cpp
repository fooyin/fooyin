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

#include "filterdelegate.h"
#include "filteritem.h"
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
FilterWidget::FilterWidget(FilterManager* manager, Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_manager{manager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_filter{m_manager->registerFilter("")}
    , m_view{new FilterView(this)}
    , m_model{new FilterModel(&m_filter->field, this)}
    , m_sortOrder{Qt::AscendingOrder}
{
    setObjectName(FilterWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);

    m_view->setModel(m_model);
    m_view->setItemDelegate(new FilterDelegate(this));

    m_layout->addWidget(m_view);

    setupConnections();
    setHeaderEnabled(m_settings->value<Settings::FilterHeader>());
    setScrollbarEnabled(m_settings->value<Settings::FilterScrollBar>());
    setAltColors(m_settings->value<Settings::FilterAltColours>());

    resetByType();
}

FilterWidget::~FilterWidget()
{
    m_manager->unregisterFilter(m_filter->index);
}

void FilterWidget::setupConnections()
{
    m_settings->subscribe<Settings::FilterAltColours>(this, &FilterWidget::setAltColors);
    m_settings->subscribe<Settings::FilterHeader>(this, &FilterWidget::setHeaderEnabled);
    m_settings->subscribe<Settings::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);

    connect(m_view->header(), &FilterView::customContextMenuRequested, this, &FilterWidget::customHeaderMenuRequested);
    connect(m_view, &QTreeView::doubleClicked, this, []() {
        //        m_library->prepareTracks();
    });
    connect(m_view->header(), &QHeaderView::sectionClicked, this, &FilterWidget::changeOrder);
    connect(this, &FilterWidget::typeChanged, m_manager, &FilterManager::changeFilter);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilterWidget::selectionChanged);

    connect(m_manager, &FilterManager::filteredItems, this, &FilterWidget::resetByIndex);
    connect(m_manager, &FilterManager::filterChanged, this, &FilterWidget::editFilter);
    connect(m_manager, &FilterManager::fieldChanged, this, &FilterWidget::fieldChanged);
}

void FilterWidget::setField(const QString& name)
{
    m_filter->field = m_manager->findField(name);
    m_model->setField(&m_filter->field);
    emit typeChanged(m_filter->index);
    m_view->clearSelection();
    m_view->scrollToTop();
}

bool FilterWidget::isHeaderEnabled()
{
    return !m_view->isHeaderHidden();
}

void FilterWidget::setHeaderEnabled(bool enabled)
{
    m_view->setHeaderHidden(!enabled);
}

bool FilterWidget::isScrollbarEnabled()
{
    return m_view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    m_view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

bool FilterWidget::hasAltColors()
{
    return m_view->alternatingRowColors();
}

void FilterWidget::setAltColors(bool enabled)
{
    m_view->setAlternatingRowColors(enabled);
}

QString FilterWidget::name() const
{
    return "Filter";
}

void FilterWidget::customHeaderMenuRequested(QPoint pos)
{
    if(!m_filter) {
        return;
    }
    auto* menu = m_manager->filterHeaderMenu(m_filter->index, &m_filter->field);

    if(!menu) {
        return;
    }

    menu->popup(mapToGlobal(pos));
}

void FilterWidget::saveLayout(QJsonArray& array)
{
    if(!m_filter) {
        return;
    }
    QJsonObject options;
    options["Type"] = m_filter->field.name;
    options["Sort"] = Utils::EnumHelper::toString(m_sortOrder);

    QJsonObject filter;
    filter[name()] = options;
    array.append(filter);
}

void FilterWidget::loadLayout(QJsonObject& object)
{
    setField(object["Type"].toString());
    m_sortOrder = Utils::EnumHelper::fromString<Qt::SortOrder>(object["Sort"].toString());
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

    Core::TrackList tracks;
    for(const auto& index : indexes) {
        if(index.isValid()) {
            const auto newTracks = index.data(FilterItemRole::Tracks).value<Core::TrackList>();
            tracks.insert(tracks.end(), newTracks.cbegin(), newTracks.cend());
        }
    }
    m_filter->tracks = tracks;
    m_manager->selectionChanged(m_filter->index);
}

void FilterWidget::fieldChanged(const FilterField& field)
{
    if(m_filter->field.index == field.index) {
        m_filter->field = field;
        emit typeChanged(m_filter->index);
        m_model->reload(m_manager->tracks());
        m_model->sortFilter(m_sortOrder);
    }
}

void FilterWidget::editFilter(int index, const QString& name)
{
    if(m_filter->index == index) {
        setField(name);
    }
}

void FilterWidget::changeOrder()
{
    if(m_sortOrder == Qt::AscendingOrder) {
        m_sortOrder = Qt::DescendingOrder;
    }
    else {
        m_sortOrder = Qt::AscendingOrder;
    }
    m_model->sortFilter(m_sortOrder);
}

void FilterWidget::resetByIndex(int idx)
{
    if(idx < m_filter->index) {
        m_model->reload(m_manager->tracks());
        m_model->sortFilter(m_sortOrder);
    }
}

void FilterWidget::resetByType()
{
    m_model->reload(m_manager->tracks());
    m_model->sortFilter(m_sortOrder);
}
} // namespace Fy::Filters
