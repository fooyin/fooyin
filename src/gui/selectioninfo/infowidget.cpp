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

#include <core/application.h>
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
#include <QSortFilterProxyModel>

using namespace Qt::StringLiterals;

namespace Fooyin {
class InfoFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit InfoFilterModel(QObject* parent = nullptr)
        : QSortFilterProxyModel{parent}
    { }

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const QModelIndex index = sourceModel()->index(sourceRow, 1, sourceParent);
        if(sourceModel()->hasChildren(index)) {
            return true;
        }
        return !sourceModel()->data(index).toString().isEmpty();
    }
};

InfoWidget::InfoWidget(const TrackList& tracks, LibraryManager* libraryManager, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_selectionController{nullptr}
    , m_playerController{nullptr}
    , m_settings{nullptr}
    , m_view{new InfoView(this)}
    , m_proxyModel{new InfoFilterModel(this)}
    , m_model{new InfoModel(libraryManager, this)}
    , m_displayOption{SelectionDisplay::PreferSelection}
    , m_scrollPos{-1}
    , m_showHeader{true}
    , m_showScrollbar{true}
    , m_alternatingColours{true}
{
    setObjectName(InfoWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_proxyModel->setSourceModel(m_model);
    m_view->setModel(m_proxyModel);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { resetView(); });

    m_model->resetModel(tracks);
}

InfoWidget::InfoWidget(Application* app, TrackSelectionController* selectionController, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_selectionController{selectionController}
    , m_playerController{app->playerController()}
    , m_settings{app->settingsManager()}
    , m_view{new InfoView(this)}
    , m_proxyModel{new InfoFilterModel(this)}
    , m_model{new InfoModel(app->libraryManager(), this)}
    , m_displayOption{static_cast<SelectionDisplay>(m_settings->value<Settings::Gui::Internal::InfoDisplayPrefer>())}
    , m_scrollPos{-1}
    , m_showHeader{true}
    , m_showScrollbar{true}
    , m_alternatingColours{true}
{
    setObjectName(InfoWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_proxyModel->setSourceModel(m_model);
    m_view->setModel(m_proxyModel);

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { resetView(); });

    using namespace Settings::Gui::Internal;

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
    return u"SelectionInfo"_s;
}

void InfoWidget::saveLayoutData(QJsonObject& layout)
{
    layout["Options"_L1]         = static_cast<int>(m_model->options());
    layout["ShowHeader"_L1]      = m_showHeader;
    layout["ShowScrollbar"_L1]   = m_showScrollbar;
    layout["AlternatingRows"_L1] = m_alternatingColours;
    layout["State"_L1]           = QString::fromUtf8(m_view->header()->saveState().toBase64());
}

void InfoWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Options"_L1)) {
        const auto options = static_cast<InfoItem::Options>(layout.value("Options"_L1).toInt());
        m_model->setOptions(options);
    }
    if(layout.contains("ShowHeader"_L1)) {
        m_showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        m_showScrollbar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        m_alternatingColours = layout.value("AlternatingRows"_L1).toBool();
    }
    if(layout.contains("State"_L1)) {
        const auto state = QByteArray::fromBase64(layout["State"_L1].toString().toUtf8());
        m_view->header()->restoreState(state);
    }
}

void InfoWidget::finalise()
{
    m_view->setHeaderHidden(!m_showHeader);
    m_view->setVerticalScrollBarPolicy(m_showScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setAlternatingRowColors(m_alternatingColours);
}

bool InfoWidget::canApply() const
{
    return false;
}

void InfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(!m_settings) {
        return;
    }

    using namespace Settings::Gui::Internal;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showHeaders = new QAction(tr("Show header"), menu);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!m_view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_showHeader = checked;
        finalise();
    });

    auto* showScrollBar = new QAction(tr("Show scrollbar"), menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(m_view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_showScrollbar = checked;
        finalise();
    });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction(tr("Alternating row colours"), menu);
    altColours->setCheckable(true);
    altColours->setChecked(m_view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_alternatingColours = checked;
        finalise();
    });

    const auto options = m_model->options();

    auto* showMetadata = new QAction(tr("Metadata"), menu);
    showMetadata->setCheckable(true);
    showMetadata->setChecked(options & InfoItem::Metadata);
    QAction::connect(showMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Metadata, checked);
        resetModel();
    });

    auto* showExtendedMetadata = new QAction(tr("Extended metadata"), menu);
    showExtendedMetadata->setCheckable(true);
    showExtendedMetadata->setChecked(options & InfoItem::ExtendedMetadata);
    QAction::connect(showExtendedMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::ExtendedMetadata, checked);
        resetModel();
    });

    auto* showLocation = new QAction(tr("Location"), menu);
    showLocation->setCheckable(true);
    showLocation->setChecked(options & InfoItem::Location);
    QAction::connect(showLocation, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Location, checked);
        resetModel();
    });

    auto* showGeneral = new QAction(tr("General"), menu);
    showGeneral->setCheckable(true);
    showGeneral->setChecked(options & InfoItem::General);
    QAction::connect(showGeneral, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::General, checked);
        resetModel();
    });

    auto* showReplayGain = new QAction(tr("ReplayGain"), menu);
    showReplayGain->setCheckable(true);
    showReplayGain->setChecked(options & InfoItem::ReplayGain);
    QAction::connect(showReplayGain, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::ReplayGain, checked);
        resetModel();
    });

    auto* showOther = new QAction(tr("Other"), menu);
    showOther->setCheckable(true);
    showOther->setChecked(options & InfoItem::Other);
    QAction::connect(showOther, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Other, checked);
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
    menu->addAction(showReplayGain);
    menu->addAction(showOther);

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

#include "infowidget.moc"
