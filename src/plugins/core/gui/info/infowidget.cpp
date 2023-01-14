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

#include "infowidget.h"

#include "core/actions/actioncontainer.h"
#include "core/coresettings.h"
#include "core/gui/info/infoitem.h"
#include "core/gui/info/itemdelegate.h"
#include "core/library/models/track.h"
#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"

#include <QHeaderView>
#include <QMenu>
#include <QTableWidget>
#include <pluginsystem/pluginmanager.h>

namespace Core::Widgets {
InfoWidget::InfoWidget(QWidget* parent)
    : FyWidget(parent)
    , m_settings(PluginSystem::object<SettingsManager>())
    , m_playerManager(PluginSystem::object<Player::PlayerManager>())
    , m_library(PluginSystem::object<Library::MusicLibrary>())
    , m_layout(new QHBoxLayout(this))
{
    setObjectName("Info Panel");
    setupUi();

    setHeaderHidden(m_settings->value(Settings::InfoHeader).toBool());
    setScrollbarHidden(m_settings->value(Settings::InfoScrollBar).toBool());
    setAltRowColors(m_settings->value(Settings::InfoAltColours).toBool());

    connect(m_playerManager, &Player::PlayerManager::currentTrackChanged, this, &Widgets::InfoWidget::refreshTrack);
    connect(m_playerManager, &Player::PlayerManager::currentTrackChanged, &m_model, &InfoModel::reset);

    //    connect(m_settings, &Settings::infoAltColorsChanged, this, &InfoWidget::setAltRowColors);
    //    connect(m_settings, &Settings::infoHeaderChanged, this, &InfoWidget::setHeaderHidden);
    //    connect(m_settings, &Settings::infoScrollBarChanged, this, &InfoWidget::setScrollbarHidden);

    spanHeaders();
}

InfoWidget::~InfoWidget() = default;

void InfoWidget::setupUi()
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_view.setItemDelegate(new ItemDelegate(this));
    m_view.setModel(&m_model);

    m_layout->addWidget(&m_view);

    m_view.header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    spanHeaders();
}

void InfoWidget::spanHeaders()
{
    for(int i = 0; i < m_model.rowCount({}); i++) {
        auto type = m_model.index(i, 0, {}).data(Role::Type).value<InfoItem::Type>();
        if(type == InfoItem::Type::Header) {
            m_view.setFirstColumnSpanned(i, {}, true);
        }
    }
}

void InfoWidget::refreshTrack(Track* track)
{
    if(track) {
        //        spanHeaders();
        //        m_table.resetLayout();
        //        m_table.resizeColumnToContents(0);
    }
}

bool InfoWidget::isHeaderHidden()
{
    return m_view.isHeaderHidden();
}

void InfoWidget::setHeaderHidden(bool showHeader)
{
    m_view.setHeaderHidden(!showHeader);
}

bool InfoWidget::isScrollbarHidden()
{
    return m_view.verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void InfoWidget::setScrollbarHidden(bool showScrollBar)
{
    m_view.setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

bool InfoWidget::altRowColors()
{
    return m_view.alternatingRowColors();
}

void InfoWidget::setAltRowColors(bool altColours)
{
    m_view.setAlternatingRowColors(altColours);
}

QString InfoWidget::name() const
{
    return "Info";
}

void InfoWidget::layoutEditingMenu(ActionContainer* menu)
{
    auto* showHeaders = new QAction("Show Header", this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::InfoHeader, checked);
    });

    auto* showScrollBar = new QAction("Show Scrollbar", menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::InfoScrollBar, checked);
    });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction("Alternate Row Colours", this);
    altColours->setCheckable(true);
    altColours->setChecked(altRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_settings->set(Settings::InfoAltColours, checked);
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
}
}; // namespace Core::Widgets
