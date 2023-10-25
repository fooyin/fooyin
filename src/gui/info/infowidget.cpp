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

#include "infowidget.h"

#include "gui/guisettings.h"
#include "gui/trackselectioncontroller.h"
#include "infodelegate.h"
#include "infomodel.h"

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/actions/actioncontainer.h>
#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QMenu>
#include <QTreeView>

namespace Fy::Gui::Widgets::Info {
InfoWidget::InfoWidget(Core::Player::PlayerManager* playerManager, TrackSelectionController* selectionController,
                       Utils::SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_view{new QTreeView(this)}
    , m_model{new InfoModel(playerManager, this)}
{
    setObjectName("Info Panel");
    setupUi();

    setHeaderHidden(m_settings->value<Settings::InfoHeader>());
    setScrollbarHidden(m_settings->value<Settings::InfoScrollBar>());
    setAltRowColors(m_settings->value<Settings::InfoAltColours>());

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, m_model, &InfoModel::resetModel);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        spanHeaders();
        m_view->expandAll();
    });

    m_settings->subscribe<Settings::InfoAltColours>(this, &InfoWidget::setAltRowColors);
    m_settings->subscribe<Settings::InfoHeader>(this, &InfoWidget::setHeaderHidden);
    m_settings->subscribe<Settings::InfoScrollBar>(this, &InfoWidget::setScrollbarHidden);

    m_model->resetModel(selectionController->selectedTracks());
}

void InfoWidget::setupUi()
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_view->setRootIsDecorated(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setMouseTracking(true);
    m_view->setItemsExpandable(false);
    m_view->setIndentation(0);
    m_view->setExpandsOnDoubleClick(false);
    m_view->setWordWrap(true);
    m_view->setTextElideMode(Qt::ElideLeft);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setSortingEnabled(false);
    m_view->setAlternatingRowColors(true);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_view->setModel(m_model);

    m_layout->addWidget(m_view);

    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    spanHeaders();
    m_view->expandAll();
}

void InfoWidget::spanHeaders()
{
    const int rowCount = m_model->rowCount({});

    for(int i = 0; i < rowCount; ++i) {
        auto type = m_model->index(i, 0, {}).data(InfoItem::Type).value<InfoItem::ItemType>();
        if(type == InfoItem::Header) {
            m_view->setFirstColumnSpanned(i, {}, true);
        }
    }
}

bool InfoWidget::isHeaderHidden()
{
    return m_view->isHeaderHidden();
}

void InfoWidget::setHeaderHidden(bool showHeader)
{
    m_view->setHeaderHidden(!showHeader);
}

bool InfoWidget::isScrollbarHidden()
{
    return m_view->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void InfoWidget::setScrollbarHidden(bool showScrollBar)
{
    m_view->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

bool InfoWidget::altRowColors()
{
    return m_view->alternatingRowColors();
}

void InfoWidget::setAltRowColors(bool altColours)
{
    m_view->setAlternatingRowColors(altColours);
}

QString InfoWidget::name() const
{
    return QStringLiteral("Info");
}

void InfoWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    auto* showHeaders = new QAction(QStringLiteral("Show Header"), this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Settings::InfoHeader>(checked); });

    auto* showScrollBar = new QAction(QStringLiteral("Show Scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Settings::InfoScrollBar>(checked); });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(QStringLiteral("Alternate Row Colours"), this);
    altColours->setCheckable(true);
    altColours->setChecked(altRowColors());
    QAction::connect(altColours, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Settings::InfoAltColours>(checked); });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
}
} // namespace Fy::Gui::Widgets::Info
