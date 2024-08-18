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

namespace Fooyin {
InfoWidget::InfoWidget(const TrackList& tracks, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_selectionController{nullptr}
    , m_playerController{nullptr}
    , m_settings{nullptr}
    , m_view{new InfoView(this)}
    , m_model{new InfoModel(this)}
    , m_displayOption{SelectionDisplay::PreferSelection}
    , m_scrollPos{-1}
{
    setObjectName(InfoWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_view->setModel(m_model);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { resetView(); });

    m_model->resetModel(tracks);
}

InfoWidget::InfoWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                       SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_selectionController{selectionController}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_view{new InfoView(this)}
    , m_model{new InfoModel(this)}
    , m_displayOption{static_cast<SelectionDisplay>(m_settings->value<Settings::Gui::Internal::InfoDisplayPrefer>())}
    , m_scrollPos{-1}
{
    setObjectName(InfoWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_view->setModel(m_model);

    m_view->setHeaderHidden(!settings->value<Settings::Gui::Internal::InfoHeader>());
    m_view->setVerticalScrollBarPolicy(
        settings->value<Settings::Gui::Internal::InfoScrollBar>() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setAlternatingRowColors(settings->value<Settings::Gui::Internal::InfoAltColours>());

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { resetView(); });

    using namespace Settings::Gui::Internal;

    m_settings->subscribe<InfoHeader>(this, [this](bool enabled) { m_view->setHeaderHidden(!enabled); });
    m_settings->subscribe<InfoScrollBar>(this, [this](bool enabled) {
        m_view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    });
    m_settings->subscribe<InfoAltColours>(this, [this](bool enabled) { m_view->setAlternatingRowColors(enabled); });
    m_settings->subscribe<Settings::Gui::Internal::InfoDisplayPrefer>(this, [this](const int option) {
        m_displayOption = static_cast<SelectionDisplay>(option);
        resetModel();
    });

    resetModel();
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
    layout[u"Options"] = static_cast<int>(m_model->options());
    layout[u"State"]   = QString::fromUtf8(m_view->header()->saveState().toBase64());
}

void InfoWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Options")) {
        const auto options = static_cast<InfoItem::Options>(layout.value(u"Options").toInt());
        m_model->setOptions(options);
    }
    if(layout.contains(u"State")) {
        const auto state = QByteArray::fromBase64(layout[u"State"].toString().toUtf8());
        m_view->header()->restoreState(state);
    }
}

void InfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(!m_settings) {
        return;
    }

    using namespace Settings::Gui::Internal;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(tr("Show header"), this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!m_view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<InfoHeader>(checked); });

    auto* showScrollBar = new QAction(tr("Show scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(m_view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showScrollBar, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<InfoScrollBar>(checked); });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(tr("Alternating row colours"), this);
    altColours->setCheckable(true);
    altColours->setChecked(m_view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<InfoAltColours>(checked); });

    const auto options = m_model->options();

    auto* showMetadata = new QAction(tr("Metadata"), this);
    showMetadata->setCheckable(true);
    showMetadata->setChecked(options & InfoItem::Metadata);
    QAction::connect(showMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Metadata, checked);
        resetModel();
    });

    auto* showExtendedMetadata = new QAction(tr("Extended metadata"), this);
    showExtendedMetadata->setCheckable(true);
    showExtendedMetadata->setChecked(options & InfoItem::ExtendedMetadata);
    QAction::connect(showExtendedMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::ExtendedMetadata, checked);
        resetModel();
    });

    auto* showLocation = new QAction(tr("Location"), this);
    showLocation->setCheckable(true);
    showLocation->setChecked(options & InfoItem::Location);
    QAction::connect(showLocation, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Location, checked);
        resetModel();
    });

    auto* showGeneral = new QAction(tr("General"), this);
    showGeneral->setCheckable(true);
    showGeneral->setChecked(options & InfoItem::General);
    QAction::connect(showGeneral, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::General, checked);
        resetModel();
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
    menu->addSeparator();
    menu->addAction(showMetadata);
    menu->addAction(showExtendedMetadata);
    menu->addAction(showLocation);
    menu->addAction(showGeneral);

    menu->popup(event->globalPos());
}

void InfoWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_resetTimer.timerId()) {
        m_resetTimer.stop();
        resetModel();
    }

    PropertiesTabWidget::timerEvent(event);
}

void InfoWidget::resetModel()
{
    if(!m_playerController || !m_selectionController) {
        return;
    }

    m_scrollPos = m_view->verticalScrollBar()->value();

    const Track currentTrack = m_playerController->currentTrack();

    if(m_displayOption == SelectionDisplay::PreferPlaying && currentTrack.isValid()) {
        m_model->resetModel({currentTrack});
    }
    else if(m_selectionController->hasTracks()) {
        m_model->resetModel(m_selectionController->selectedTracks());
    }
    else if(currentTrack.isValid()) {
        m_model->resetModel({currentTrack});
    }
    else {
        m_model->resetModel({});
    }
}

void InfoWidget::resetView()
{
    if(m_scrollPos >= 0) {
        m_view->verticalScrollBar()->setValue(std::exchange(m_scrollPos, -1));
    }
}
} // namespace Fooyin
