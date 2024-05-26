/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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
#include "infoview.h"
#include "internalguisettings.h"

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QTreeView>

namespace Fooyin {
struct InfoWidget::Private
{
    InfoWidget* self;

    TrackSelectionController* selectionController;
    PlayerController* playerController;
    SettingsManager* settings;

    InfoView* view;
    InfoModel* model;

    int scrollPos{-1};

    Private(InfoWidget* self_, TrackSelectionController* selectionController_, PlayerController* playerController_,
            SettingsManager* settings_)
        : self{self_}
        , selectionController{selectionController_}
        , playerController{playerController_}
        , settings{settings_}
        , view{new InfoView(self)}
        , model{new InfoModel(self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        view->setRootIsDecorated(false);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setItemsExpandable(false);
        view->setIndentation(10);
        view->setExpandsOnDoubleClick(false);
        view->setTextElideMode(Qt::ElideRight);
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setSortingEnabled(false);
        view->setAlternatingRowColors(true);
        view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

        view->setItemDelegate(new ItemDelegate(self));
        view->setModel(model);

        layout->addWidget(view);

        spanHeaders();
        view->expandAll();

        setHeaderHidden(settings->value<Settings::Gui::Internal::InfoHeader>());
        setScrollbarHidden(settings->value<Settings::Gui::Internal::InfoScrollBar>());
        setAltRowColors(settings->value<Settings::Gui::Internal::InfoAltColours>());
    }

    void spanHeaders() const
    {
        const int rowCount = model->rowCount({});

        for(int row{0}; row < rowCount; ++row) {
            const auto index = model->index(row, 0, {});
            const auto type  = index.data(InfoItem::Type).value<InfoItem::ItemType>();
            if(type == InfoItem::Header) {
                view->setFirstColumnSpanned(row, {}, true);
                view->expand(index);
            }
        }
    }

    void setHeaderHidden(bool showHeader) const
    {
        view->setHeaderHidden(!showHeader);
    }

    void setScrollbarHidden(bool showScrollBar) const
    {
        view->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }

    void setAltRowColors(bool altColours) const
    {
        view->setAlternatingRowColors(altColours);
    }

    void resetModel()
    {
        scrollPos = view->verticalScrollBar()->value();
        model->resetModel(selectionController->selectedTracks(), playerController->currentTrack());
    }
};

InfoWidget::InfoWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                       SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , p{std::make_unique<Private>(this, selectionController, playerController, settings)}
{
    setObjectName(InfoWidget::name());

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { p->resetModel(); });

    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() {
        p->spanHeaders();
        if(p->scrollPos >= 0) {
            p->view->verticalScrollBar()->setValue(std::exchange(p->scrollPos, -1));
        }
    });

    using namespace Settings::Gui::Internal;

    p->settings->subscribe<InfoHeader>(this, [this](bool enabled) { p->setHeaderHidden(enabled); });
    p->settings->subscribe<InfoScrollBar>(this, [this](bool enabled) { p->setScrollbarHidden(enabled); });
    p->settings->subscribe<InfoAltColours>(this, [this](bool enabled) { p->setAltRowColors(enabled); });

    p->resetModel();
}

InfoWidget::~InfoWidget() = default;

QString InfoWidget::name() const
{
    return tr("Selection Info");
}

QString InfoWidget::layoutName() const
{
    return QStringLiteral("SelectionInfo");
}

void InfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    using namespace Settings::Gui::Internal;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(QStringLiteral("Show Header"), this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!p->view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<InfoHeader>(checked); });

    auto* showScrollBar = new QAction(QStringLiteral("Show Scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(p->view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showScrollBar, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<InfoScrollBar>(checked); });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(QStringLiteral("Alternate Row Colours"), this);
    altColours->setCheckable(true);
    altColours->setChecked(p->view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<InfoAltColours>(checked); });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "infowidget.moc"
