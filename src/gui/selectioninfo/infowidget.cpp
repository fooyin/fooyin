/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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
#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tooltipfilter.h>

#include <QBasicTimer>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto PropertiesInfoState = "PropertiesDialog/SelectionInfo"_L1;

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

class InfoPanel : public QWidget
{
    Q_OBJECT

public:
    InfoPanel(TrackList tracks, LibraryManager* libraryManager, ActionManager* actionManager,
              bool persistSettings = false, QWidget* parent = nullptr);
    InfoPanel(Application* app, ActionManager* actionManager, TrackSelectionController* selectionController,
              QWidget* parent = nullptr);
    ~InfoPanel() override;

    void saveLayoutData(QJsonObject& layout) const;
    void loadLayoutData(const QJsonObject& layout);
    void finalise();
    void updateTracks(const TrackList& tracks);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void setupActions();
    void updateActions() const;

    void copySelection() const;
    [[nodiscard]] QString selectionText() const;
    [[nodiscard]] bool persistSettings() const;
    void loadPersistentSettings();
    void savePersistentSettings() const;
    void queueViewReset();
    void resetModel();
    void resetView();

    ActionManager* m_actionManager;
    TrackSelectionController* m_selectionController;
    PlayerController* m_playerController;
    SettingsManager* m_settings;
    bool m_persistSettings;

    InfoView* m_view;
    InfoFilterModel* m_proxyModel;
    InfoModel* m_model;
    WidgetContext* m_context;
    QAction* m_copyAction;
    Command* m_copyCmd;
    QBasicTimer m_resetTimer;
    SelectionDisplay m_displayOption;
    TrackList m_tracks;
    int m_scrollPos;

    bool m_showHeader;
    bool m_showVerticalScrollbar;
    bool m_showHorizontalScrollbar;
    bool m_alternatingColours;
    bool m_pendingViewReset;
};

void setupInfoHost(QWidget* host, QWidget* panel)
{
    auto* layout = new QHBoxLayout(host);
    layout->setContentsMargins({});
    layout->addWidget(panel);
}

InfoPanel::InfoPanel(TrackList tracks, LibraryManager* libraryManager, ActionManager* actionManager,
                     bool persistSettings, QWidget* parent)
    : QWidget{parent}
    , m_actionManager{actionManager}
    , m_selectionController{nullptr}
    , m_playerController{nullptr}
    , m_settings{nullptr}
    , m_persistSettings{persistSettings}
    , m_view{new InfoView(this)}
    , m_proxyModel{new InfoFilterModel(this)}
    , m_model{new InfoModel(libraryManager, this)}
    , m_context{new WidgetContext(
          this, Context{Id{"Fooyin.Context.SelectionInfo."}.append(reinterpret_cast<uintptr_t>(this))}, this)}
    , m_copyAction{new QAction(tr("&Copy"), this)}
    , m_copyCmd{m_actionManager->registerAction(m_copyAction, Constants::Actions::Copy, m_context->context())}
    , m_displayOption{SelectionDisplay::PreferSelection}
    , m_tracks{std::move(tracks)}
    , m_scrollPos{-1}
    , m_showHeader{true}
    , m_showVerticalScrollbar{true}
    , m_showHorizontalScrollbar{false}
    , m_alternatingColours{true}
    , m_pendingViewReset{false}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_view->viewport()->installEventFilter(new ToolTipFilter(this));
    m_proxyModel->setSourceModel(m_model);
    m_view->setModel(m_proxyModel);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_view->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { queueViewReset(); });

    setupActions();
    loadPersistentSettings();
    finalise();
    resetModel();
}

InfoPanel::InfoPanel(Application* app, ActionManager* actionManager, TrackSelectionController* selectionController,
                     QWidget* parent)
    : QWidget{parent}
    , m_actionManager{actionManager}
    , m_selectionController{selectionController}
    , m_playerController{app->playerController()}
    , m_settings{app->settingsManager()}
    , m_persistSettings{false}
    , m_view{new InfoView(this)}
    , m_proxyModel{new InfoFilterModel(this)}
    , m_model{new InfoModel(app->libraryManager(), this)}
    , m_context{new WidgetContext(
          this, Context{Id{"Fooyin.Context.SelectionInfo."}.append(reinterpret_cast<uintptr_t>(this))}, this)}
    , m_copyAction{new QAction(tr("&Copy"), this)}
    , m_copyCmd{m_actionManager->registerAction(m_copyAction, Constants::Actions::Copy, m_context->context())}
    , m_displayOption{static_cast<SelectionDisplay>(m_settings->value<Settings::Gui::Internal::InfoDisplayPrefer>())}
    , m_scrollPos{-1}
    , m_showHeader{true}
    , m_showVerticalScrollbar{true}
    , m_showHorizontalScrollbar{false}
    , m_alternatingColours{true}
    , m_pendingViewReset{false}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ItemDelegate(this));
    m_view->viewport()->installEventFilter(new ToolTipFilter(this));
    m_proxyModel->setSourceModel(m_model);
    m_view->setModel(m_proxyModel);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_view->header()->setSectionResizeMode(1, QHeaderView::Fixed);

    QObject::connect(selectionController, &TrackSelectionController::selectionChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this]() { m_resetTimer.start(50, this); });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { queueViewReset(); });

    setupActions();

    using namespace Settings::Gui::Internal;

    m_settings->subscribe<InfoDisplayPrefer>(this, [this](const int option) {
        m_displayOption = static_cast<SelectionDisplay>(option);
        resetModel();
    });

    resetModel();
}

InfoPanel::~InfoPanel()
{
    savePersistentSettings();
}

InfoWidget::InfoWidget(const TrackList& tracks, LibraryManager* libraryManager, ActionManager* actionManager,
                       QWidget* parent)
    : FyWidget{parent}
    , m_panel{new InfoPanel(tracks, libraryManager, actionManager, false, this)}
{
    setObjectName(InfoWidget::name());
    setupInfoHost(this, m_panel);
}

InfoWidget::InfoWidget(Application* app, ActionManager* actionManager, TrackSelectionController* selectionController,
                       QWidget* parent)
    : FyWidget{parent}
    , m_panel{new InfoPanel(app, actionManager, selectionController, this)}
{
    setObjectName(InfoWidget::name());
    setupInfoHost(this, m_panel);
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
    m_panel->saveLayoutData(layout);
}

void InfoWidget::loadLayoutData(const QJsonObject& layout)
{
    m_panel->loadLayoutData(layout);
}

void InfoWidget::finalise()
{
    m_panel->finalise();
}

InfoPropertiesTab::InfoPropertiesTab(const TrackList& tracks, LibraryManager* libraryManager,
                                     ActionManager* actionManager, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_panel{new InfoPanel(tracks, libraryManager, actionManager, true, this)}
{
    setupInfoHost(this, m_panel);
}

InfoPropertiesTab::~InfoPropertiesTab() = default;

void InfoPropertiesTab::updateTracks(const TrackList& tracks)
{
    m_panel->updateTracks(tracks);
}

bool InfoPropertiesTab::canApply() const
{
    return false;
}

void InfoPanel::saveLayoutData(QJsonObject& layout) const
{
    layout["Options"_L1]                 = static_cast<int>(m_model->options());
    layout["ShowHeader"_L1]              = m_showHeader;
    layout["ShowVerticalScrollbar"_L1]   = m_showVerticalScrollbar;
    layout["ShowHorizontalScrollbar"_L1] = m_showHorizontalScrollbar;
    layout["AlternatingRows"_L1]         = m_alternatingColours;
    layout["State"_L1]                   = QString::fromUtf8(m_view->header()->saveState().toBase64());
}

void InfoPanel::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Options"_L1)) {
        const auto options = static_cast<InfoItem::Options>(layout.value("Options"_L1).toInt());
        m_model->setOptions(options);
    }
    if(layout.contains("ShowHeader"_L1)) {
        m_showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowVerticalScrollbar"_L1)) {
        m_showVerticalScrollbar = layout.value("ShowVerticalScrollbar"_L1).toBool();
    }
    if(layout.contains("ShowHorizontalScrollbar"_L1)) {
        m_showHorizontalScrollbar = layout.value("ShowHorizontalScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        m_alternatingColours = layout.value("AlternatingRows"_L1).toBool();
    }
    if(layout.contains("State"_L1)) {
        const auto state = QByteArray::fromBase64(layout["State"_L1].toString().toUtf8());
        m_view->header()->restoreState(state);
    }
}

void InfoPanel::finalise()
{
    m_view->setHeaderHidden(!m_showHeader);
    m_view->setVerticalScrollBarPolicy(m_showVerticalScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setHorizontalScrollBarPolicy(m_showHorizontalScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setElideText(!m_showHorizontalScrollbar);
    m_view->setAlternatingRowColors(m_alternatingColours);
}

void InfoPanel::updateTracks(const TrackList& tracks)
{
    if(m_selectionController) {
        return;
    }

    m_tracks = tracks;
    resetModel();
}

bool InfoPanel::persistSettings() const
{
    // Only persist separately for prop tab
    return m_persistSettings && !m_selectionController;
}

void InfoPanel::loadPersistentSettings()
{
    if(!persistSettings()) {
        return;
    }

    const FyStateSettings settings;
    const QByteArray layoutData = QByteArray::fromBase64(settings.value(PropertiesInfoState).toByteArray());
    if(layoutData.isEmpty()) {
        return;
    }

    const QJsonObject layout = QJsonDocument::fromJson(layoutData).object();
    loadLayoutData(layout);
}

void InfoPanel::savePersistentSettings() const
{
    if(!persistSettings()) {
        return;
    }

    QJsonObject layout;
    saveLayoutData(layout);

    const QByteArray layoutData = QJsonDocument{layout}.toJson(QJsonDocument::Compact).toBase64();
    FyStateSettings settings;
    settings.setValue(PropertiesInfoState, layoutData);
}

void InfoPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);

    m_view->setUpdatesEnabled(false);
    m_view->viewport()->setUpdatesEnabled(false);
    m_view->header()->setUpdatesEnabled(false);
}

void InfoPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    m_view->setUpdatesEnabled(true);
    m_view->viewport()->setUpdatesEnabled(true);
    m_view->header()->setUpdatesEnabled(true);

    if(m_pendingViewReset) {
        queueViewReset();
    }
}

void InfoPanel::queueViewReset()
{
    if(!isVisible()) {
        m_pendingViewReset = true;
        return;
    }

    m_pendingViewReset = false;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            if(!isVisible()) {
                m_pendingViewReset = true;
                return;
            }

            m_view->resizeView();
            resetView();
        },
        Qt::QueuedConnection);
}

void InfoPanel::contextMenuEvent(QContextMenuEvent* event)
{
    using namespace Settings::Gui::Internal;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    updateActions();
    menu->addAction(m_copyCmd->action());

    menu->addSeparator();

    auto* showHeaders = new QAction(tr("Show header"), menu);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!m_view->isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this](bool checked) {
        m_showHeader = checked;
        finalise();
        savePersistentSettings();
    });

    auto* showVerticalScrollBar = new QAction(tr("Show scrollbar (vertical)"), menu);
    showVerticalScrollBar->setCheckable(true);
    showVerticalScrollBar->setChecked(m_view->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showVerticalScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_showVerticalScrollbar = checked;
        finalise();
        savePersistentSettings();
    });

    auto* showHorizontalScrollBar = new QAction(tr("Show scrollbar (horizontal)"), menu);
    showHorizontalScrollBar->setCheckable(true);
    showHorizontalScrollBar->setChecked(m_view->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QAction::connect(showHorizontalScrollBar, &QAction::triggered, this, [this](bool checked) {
        m_showHorizontalScrollbar = checked;
        finalise();
        savePersistentSettings();
    });

    auto* altColours = new QAction(tr("Alternating row colours"), menu);
    altColours->setCheckable(true);
    altColours->setChecked(m_view->alternatingRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this](bool checked) {
        m_alternatingColours = checked;
        finalise();
        savePersistentSettings();
    });

    const auto options = m_model->options();

    auto* showMetadata = new QAction(tr("Metadata"), menu);
    showMetadata->setCheckable(true);
    showMetadata->setChecked(options & InfoItem::Metadata);
    QAction::connect(showMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Metadata, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showExtendedMetadata = new QAction(tr("Extended metadata"), menu);
    showExtendedMetadata->setCheckable(true);
    showExtendedMetadata->setChecked(options & InfoItem::ExtendedMetadata);
    QAction::connect(showExtendedMetadata, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::ExtendedMetadata, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showLocation = new QAction(tr("Location"), menu);
    showLocation->setCheckable(true);
    showLocation->setChecked(options & InfoItem::Location);
    QAction::connect(showLocation, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Location, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showGeneral = new QAction(tr("General"), menu);
    showGeneral->setCheckable(true);
    showGeneral->setChecked(options & InfoItem::General);
    QAction::connect(showGeneral, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::General, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showPlayStats = new QAction(tr("Playback Statistics"), menu);
    showPlayStats->setCheckable(true);
    showPlayStats->setChecked(options & InfoItem::PlayStats);
    QAction::connect(showPlayStats, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::PlayStats, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showReplayGain = new QAction(tr("ReplayGain"), menu);
    showReplayGain->setCheckable(true);
    showReplayGain->setChecked(options & InfoItem::ReplayGain);
    QAction::connect(showReplayGain, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::ReplayGain, checked);
        savePersistentSettings();
        resetModel();
    });

    auto* showOther = new QAction(tr("Other"), menu);
    showOther->setCheckable(true);
    showOther->setChecked(options & InfoItem::Other);
    QAction::connect(showOther, &QAction::triggered, this, [this](bool checked) {
        m_model->setOption(InfoItem::Other, checked);
        savePersistentSettings();
        resetModel();
    });

    menu->addAction(showHeaders);
    menu->addAction(showVerticalScrollBar);
    menu->addAction(showHorizontalScrollBar);
    menu->addAction(altColours);
    menu->addSeparator();
    menu->addAction(showMetadata);
    menu->addAction(showExtendedMetadata);
    menu->addAction(showLocation);
    menu->addAction(showGeneral);
    menu->addAction(showPlayStats);
    menu->addAction(showReplayGain);
    menu->addAction(showOther);

    menu->popup(event->globalPos());
}

void InfoPanel::setupActions()
{
    m_actionManager->addContextObject(m_context);

    QObject::connect(m_copyAction, &QAction::triggered, this, &InfoPanel::copySelection);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &InfoPanel::updateActions);

    m_copyCmd->setCategories({tr("Edit")});
    m_copyCmd->setDefaultShortcut(QKeySequence::Copy);

    updateActions();
}

void InfoPanel::updateActions() const
{
    m_copyAction->setEnabled(!selectionText().isEmpty());
}

void InfoPanel::copySelection() const
{
    const QString text = selectionText();
    if(text.isEmpty()) {
        return;
    }

    QGuiApplication::clipboard()->setText(text);
}

QString InfoPanel::selectionText() const
{
    const QModelIndex index = m_view->currentIndex();
    if(!index.isValid()) {
        return {};
    }

    return index.siblingAtColumn(1).data().toString();
}

void InfoPanel::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_resetTimer.timerId()) {
        m_resetTimer.stop();
        resetModel();
    }

    QWidget::timerEvent(event);
}

void InfoPanel::resetModel()
{
    if(!m_playerController || !m_selectionController) {
        m_model->resetModel(m_tracks);
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

void InfoPanel::resetView()
{
    if(m_scrollPos >= 0) {
        m_view->verticalScrollBar()->setValue(std::exchange(m_scrollPos, -1));
    }
}
} // namespace Fooyin

#include "infowidget.moc"
#include "moc_infowidget.cpp"
