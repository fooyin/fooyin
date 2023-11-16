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

#include "infodelegate.h"
#include "infomodel.h"

#include <core/player/playermanager.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QTreeView>

namespace Fooyin {
struct InfoWidget::Private
{
    InfoWidget* self;

    TrackSelectionController* selectionController;
    PlayerManager* playerManager;
    SettingsManager* settings;

    QTreeView* view;
    InfoModel* model;

    Private(InfoWidget* self, TrackSelectionController* selectionController, PlayerManager* playerManager,
            SettingsManager* settings)
        : self{self}
        , selectionController{selectionController}
        , playerManager{playerManager}
        , settings{settings}
        , view{new QTreeView(self)}
        , model{new InfoModel(self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        view->setRootIsDecorated(false);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setMouseTracking(true);
        view->setItemsExpandable(false);
        view->setIndentation(0);
        view->setExpandsOnDoubleClick(false);
        view->setWordWrap(true);
        view->setTextElideMode(Qt::ElideLeft);
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setSortingEnabled(false);
        view->setAlternatingRowColors(true);
        view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

        view->setItemDelegate(new ItemDelegate(self));
        view->setModel(model);

        layout->addWidget(view);

        spanHeaders();
        view->expandAll();

        setHeaderHidden(settings->value<Settings::Gui::InfoHeader>());
        setScrollbarHidden(settings->value<Settings::Gui::InfoScrollBar>());
        setAltRowColors(settings->value<Settings::Gui::InfoAltColours>());
    }

    void spanHeaders()
    {
        const int rowCount = model->rowCount({});

        for(int i = 0; i < rowCount; ++i) {
            auto type = model->index(i, 0, {}).data(InfoItem::Type).value<InfoItem::ItemType>();
            if(type == InfoItem::Header) {
                view->setFirstColumnSpanned(i, {}, true);
            }
        }
    }

    void setHeaderHidden(bool showHeader)
    {
        view->setHeaderHidden(!showHeader);
    }

    void setScrollbarHidden(bool showScrollBar)
    {
        view->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }

    void setAltRowColors(bool altColours)
    {
        view->setAlternatingRowColors(altColours);
    }

    void resetModel()
    {
        model->resetModel(selectionController->selectedTracks(), playerManager->currentTrack());
    }
};

InfoWidget::InfoWidget(PlayerManager* playerManager, TrackSelectionController* selectionController,
                       SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , p{std::make_unique<Private>(this, selectionController, playerManager, settings)}
{
    setObjectName("Info Panel");

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { p->resetModel(); });

    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() {
        p->spanHeaders();
        p->view->expandAll();
    });

    p->settings->subscribe<Settings::Gui::InfoHeader>(this, [this](bool enabled) { p->setHeaderHidden(enabled); });
    p->settings->subscribe<Settings::Gui::InfoScrollBar>(this,
                                                         [this](bool enabled) { p->setScrollbarHidden(enabled); });
    p->settings->subscribe<Settings::Gui::InfoAltColours>(this, [this](bool enabled) { p->setAltRowColors(enabled); });

    p->resetModel();
}

InfoWidget::~InfoWidget() = default;

QString InfoWidget::name() const
{
    return QStringLiteral("Info");
}

void InfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(QStringLiteral("Show Header"), this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!p->view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::InfoHeader>(checked); });

    auto* showScrollBar = new QAction(QStringLiteral("Show Scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(p->view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showScrollBar, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::InfoScrollBar>(checked); });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(QStringLiteral("Alternate Row Colours"), this);
    altColours->setCheckable(true);
    altColours->setChecked(p->view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::InfoAltColours>(checked); });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);

    menu->popup(event->globalPos());
}
} // namespace Fooyin
