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
#include <QJsonObject>
#include <QMenu>
#include <QScrollBar>
#include <QTreeView>

namespace Fooyin {
struct InfoWidget::Private
{
    InfoWidget* m_self;

    TrackSelectionController* m_selectionController;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    InfoView* m_view;
    InfoModel* m_model;

    int m_scrollPos{-1};

    Private(InfoWidget* self, TrackSelectionController* selectionController, PlayerController* playerController,
            SettingsManager* settings)
        : m_self{self}
        , m_selectionController{selectionController}
        , m_playerController{playerController}
        , m_settings{settings}
        , m_view{new InfoView(m_self)}
        , m_model{new InfoModel(m_self)}
    {
        auto* layout = new QHBoxLayout(m_self);
        layout->setContentsMargins(0, 0, 0, 0);

        m_view->setRootIsDecorated(false);
        m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view->setItemsExpandable(false);
        m_view->setIndentation(10);
        m_view->setExpandsOnDoubleClick(false);
        m_view->setTextElideMode(Qt::ElideRight);
        m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_view->setSortingEnabled(false);
        m_view->setAlternatingRowColors(true);
        m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

        m_view->setItemDelegate(new ItemDelegate(m_self));
        m_view->setModel(m_model);

        layout->addWidget(m_view);

        spanHeaders();
        m_view->expandAll();

        setHeaderHidden(settings->value<Settings::Gui::Internal::InfoHeader>());
        setScrollbarHidden(settings->value<Settings::Gui::Internal::InfoScrollBar>());
        setAltRowColors(settings->value<Settings::Gui::Internal::InfoAltColours>());
    }

    void resetView()
    {
        spanHeaders();
        m_view->expandAll();

        if(m_scrollPos >= 0) {
            m_view->verticalScrollBar()->setValue(std::exchange(m_scrollPos, -1));
        }
    }

    void spanHeaders() const
    {
        const int rowCount = m_model->rowCount({});

        for(int row{0}; row < rowCount; ++row) {
            const auto index = m_model->index(row, 0, {});
            if(m_model->hasChildren(index)) {
                m_view->setFirstColumnSpanned(row, {}, true);
            }
        }
    }

    void setHeaderHidden(bool showHeader) const
    {
        m_view->setHeaderHidden(!showHeader);
    }

    void setScrollbarHidden(bool showScrollBar) const
    {
        m_view->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }

    void setAltRowColors(bool altColours) const
    {
        m_view->setAlternatingRowColors(altColours);
    }

    void resetModel()
    {
        m_scrollPos = m_view->verticalScrollBar()->value();
        m_model->resetModel(m_selectionController->selectedTracks(), m_playerController->currentTrack());
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

    QObject::connect(p->m_model, &QAbstractItemModel::modelReset, this, [this]() { p->resetView(); });

    using namespace Settings::Gui::Internal;

    p->m_settings->subscribe<InfoHeader>(this, [this](bool enabled) { p->setHeaderHidden(enabled); });
    p->m_settings->subscribe<InfoScrollBar>(this, [this](bool enabled) { p->setScrollbarHidden(enabled); });
    p->m_settings->subscribe<InfoAltColours>(this, [this](bool enabled) { p->setAltRowColors(enabled); });

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

void InfoWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("Options")] = static_cast<int>(p->m_model->options());
}

void InfoWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("Options"))) {
        const auto options = static_cast<InfoItem::Options>(layout.value(QStringLiteral("Options")).toInt());
        p->m_model->setOptions(options);
    }
}

void InfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    using namespace Settings::Gui::Internal;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(QStringLiteral("Show Header"), this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!p->m_view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<InfoHeader>(checked); });

    auto* showScrollBar = new QAction(QStringLiteral("Show Scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(p->m_view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showScrollBar, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<InfoScrollBar>(checked); });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(QStringLiteral("Alternating Row Colours"), this);
    altColours->setCheckable(true);
    altColours->setChecked(p->m_view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this,
                     [this](bool checked) { p->m_settings->set<InfoAltColours>(checked); });

    const auto options = p->m_model->options();

    auto* showMetadata = new QAction(QStringLiteral("Metadata"), this);
    showMetadata->setCheckable(true);
    showMetadata->setChecked(options & InfoItem::Metadata);
    QAction::connect(showMetadata, &QAction::triggered, this, [this](bool checked) {
        p->m_model->setOption(InfoItem::Metadata, checked);
        p->resetModel();
    });

    auto* showLocation = new QAction(QStringLiteral("Location"), this);
    showLocation->setCheckable(true);
    showLocation->setChecked(options & InfoItem::Location);
    QAction::connect(showLocation, &QAction::triggered, this, [this](bool checked) {
        p->m_model->setOption(InfoItem::Location, checked);
        p->resetModel();
    });

    auto* showGeneral = new QAction(QStringLiteral("General"), this);
    showGeneral->setCheckable(true);
    showGeneral->setChecked(options & InfoItem::General);
    QAction::connect(showGeneral, &QAction::triggered, this, [this](bool checked) {
        p->m_model->setOption(InfoItem::General, checked);
        p->resetModel();
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
    menu->addSeparator();
    menu->addAction(showMetadata);
    menu->addAction(showLocation);
    menu->addAction(showGeneral);

    menu->popup(event->globalPos());
}
} // namespace Fooyin
