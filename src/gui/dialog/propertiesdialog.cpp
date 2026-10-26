/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/coresettings.h>
#include <core/library/libraryutils.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/internalguisettings.h>
#include <gui/propertiesdialog.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <ranges>
#include <unordered_set>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

constexpr auto PropertiesDialogGroup   = "PropertiesDialog";
constexpr auto SidebarVisibleKey       = "PropertiesDialog/SidebarVisible";
constexpr auto SidebarWidthKey         = "PropertiesDialog/SidebarWidth";
constexpr auto BaseWidthKey            = "PropertiesDialog/BaseWidth";
constexpr auto PropertiesDialogContext = "Fooyin.Context.PropertiesDialog";

namespace {
Fooyin::TrackList filterDuplicateTracks(const Fooyin::TrackList& tracks)
{
    std::unordered_set<QString> seenTracks;
    seenTracks.reserve(tracks.size());

    Fooyin::TrackList filteredTracks;
    filteredTracks.reserve(tracks.size());

    for(const auto& track : tracks) {
        if(seenTracks.emplace(track.identityKey()).second) {
            filteredTracks.push_back(track);
        }
    }

    return filteredTracks;
}

bool metadataEqual(const Fooyin::Track& lhs, const Fooyin::Track& rhs)
{
    return lhs.metadata() == rhs.metadata() && lhs.serialiseExtraTags() == rhs.serialiseExtraTags()
        && lhs.removedTags() == rhs.removedTags() && lhs.rgTrackGain() == rhs.rgTrackGain()
        && lhs.rgTrackPeak() == rhs.rgTrackPeak() && lhs.rgAlbumGain() == rhs.rgAlbumGain()
        && lhs.rgAlbumPeak() == rhs.rgAlbumPeak();
}

bool statEqual(const Fooyin::Track& lhs, const Fooyin::Track& rhs)
{
    return qFuzzyCompare(lhs.rating(), rhs.rating());
}

bool editorEqual(const Fooyin::Track& lhs, const Fooyin::Track& rhs)
{
    return metadataEqual(lhs, rhs) && statEqual(lhs, rhs);
}
} // namespace

namespace Fooyin {
class PropertiesWriteProgress : public QObject
{
    Q_OBJECT

public:
    explicit PropertiesWriteProgress(int trackCount, QWidget* parent)
        : QObject{parent}
        , m_dialog{new ElapsedProgressDialog(tr("Writing changes…"), tr("Stop"), 0, 1, parent)}
    {
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        m_dialog->setModal(true);

        m_dialog->setMinimumDuration(2000ms);
        m_dialog->setBusy(true);
        m_dialog->setShowRemaining(false);
        m_dialog->setWindowTitle(tr("Writing Metadata"));
        m_dialog->setText(tr("Writing changes to %Ln track(s)…", nullptr, trackCount));
        m_dialog->startTimer();

        QObject::connect(m_dialog, &ElapsedProgressDialog::cancelled, this, [this]() { cancelRequests(); });
        QObject::connect(m_dialog, &QDialog::rejected, this, [this]() {
            if(!m_finishing) {
                cancelRequests();
            }
        });
    }

    void addRequest(WriteRequest request)
    {
        if(!request.finished.isValid()) {
            return;
        }

        ++m_pendingRequests;
        if(request.cancel) {
            m_cancelCallbacks.emplace_back(std::move(request.cancel));
        }

        request.finished.then(this, [this](const WriteResult& /*result*/) {
            --m_pendingRequests;
            finishIfIdle();
        });
    }

    void beginCollecting()
    {
        m_collecting = true;
    }

    void finishCollecting()
    {
        m_collecting = false;
        finishIfIdle();
    }

private:
    void finishIfIdle()
    {
        if(m_collecting || m_pendingRequests > 0 || m_finishing) {
            return;
        }

        m_finishing = true;
        if(m_dialog) {
            m_dialog->close();
        }

        deleteLater();
    }

    void cancelRequests()
    {
        if(std::exchange(m_cancelled, true)) {
            return;
        }

        for(const auto& cancel : m_cancelCallbacks) {
            cancel();
        }
    }

    QPointer<ElapsedProgressDialog> m_dialog;
    std::vector<std::function<void()>> m_cancelCallbacks;
    int m_pendingRequests{0};
    bool m_collecting{true};
    bool m_cancelled{false};
    bool m_finishing{false};
};

class PropertiesDialogSessionPrivate
{
public:
    TrackList m_originalTracks;
    TrackList m_workingTracks;
    std::set<int> m_activeTrackIndexes;
    std::vector<int> m_pendingRevisions;
    qsizetype m_pendingCount{0};
};

PropertiesDialogSession::PropertiesDialogSession()
    : p{std::make_unique<PropertiesDialogSessionPrivate>()}
{ }

PropertiesDialogSession::~PropertiesDialogSession() = default;

PropertiesDialogSession::PropertiesDialogSession(const PropertiesDialogSession& other)
    : p{std::make_unique<PropertiesDialogSessionPrivate>(*other.p)}
{ }

PropertiesDialogSession& PropertiesDialogSession::operator=(const PropertiesDialogSession& other)
{
    if(this != &other) {
        *p = *other.p;
    }
    return *this;
}

void PropertiesDialogSession::reset(const TrackList& tracks)
{
    const auto workingTracks = filterDuplicateTracks(tracks);
    p->m_originalTracks      = workingTracks;
    p->m_workingTracks       = workingTracks;
    p->m_activeTrackIndexes.clear();
    p->m_pendingRevisions.assign(static_cast<qsizetype>(workingTracks.size()), 0);
    p->m_pendingCount = 0;
}

const TrackList& PropertiesDialogSession::originalTracks() const
{
    return p->m_originalTracks;
}

const TrackList& PropertiesDialogSession::workingTracks() const
{
    return p->m_workingTracks;
}

TrackList PropertiesDialogSession::activeTracks() const
{
    if(p->m_activeTrackIndexes.empty()) {
        return p->m_workingTracks;
    }

    TrackList tracks;
    tracks.reserve(static_cast<TrackList::size_type>(p->m_activeTrackIndexes.size()));

    for(const int index : p->m_activeTrackIndexes) {
        if(index >= 0 && std::cmp_less(index, p->m_workingTracks.size())) {
            tracks.emplace_back(p->m_workingTracks.at(static_cast<TrackList::size_type>(index)));
        }
    }

    return tracks;
}

const std::set<int>& PropertiesDialogSession::activeTrackIndexes() const
{
    return p->m_activeTrackIndexes;
}

bool PropertiesDialogSession::isAllTracksScope() const
{
    return p->m_activeTrackIndexes.empty();
}

bool PropertiesDialogSession::setActiveTrackIndexes(std::set<int> trackIndexes)
{
    std::erase_if(trackIndexes, [this](int trackIndex) {
        return trackIndex < 0 || std::cmp_greater_equal(trackIndex, p->m_workingTracks.size());
    });

    if(trackIndexes.size() == p->m_workingTracks.size()) {
        trackIndexes.clear();
    }

    if(trackIndexes == p->m_activeTrackIndexes) {
        return false;
    }

    p->m_activeTrackIndexes = std::move(trackIndexes);
    return true;
}

void PropertiesDialogSession::updateTracks(const TrackList& tracks)
{
    for(const auto& updatedTrack : tracks) {
        const auto workingIt = std::ranges::find_if(
            p->m_workingTracks, [&updatedTrack](const Track& track) { return track.sameIdentityAs(updatedTrack); });
        if(workingIt == p->m_workingTracks.cend()) {
            continue;
        }
        const auto index = std::distance(p->m_workingTracks.begin(), workingIt);
        if(!editorEqual(p->m_originalTracks[index], updatedTrack)) {
            if(p->m_pendingRevisions[index] == 0) {
                p->m_pendingCount++;
            }
            p->m_pendingRevisions[index]++;
        }
        else if(p->m_pendingRevisions[index] > 0) {
            p->m_pendingRevisions[index] = 0;
            p->m_pendingCount            = std::max(qsizetype{0}, p->m_pendingCount - 1);
        }
        *workingIt = updatedTrack;
    }
}

void PropertiesDialogSession::acceptChanges()
{
    p->m_originalTracks = p->m_workingTracks;
    p->m_pendingRevisions.assign(p->m_originalTracks.size(), false);
    p->m_pendingCount = 0;
}

bool PropertiesDialogSession::hasChanges() const
{
    return p->m_pendingCount > 0 || p->m_originalTracks.size() != p->m_workingTracks.size();
}

bool PropertiesDialogSession::hasOnlyStatChanges() const
{
    bool hasStatChanges{false};

    for(size_t i{0}; i < p->m_originalTracks.size(); ++i) {
        const auto& originalTrack = p->m_originalTracks.at(i);
        const auto& workingTrack  = p->m_workingTracks.at(i);

        if(metadataEqual(originalTrack, workingTrack)) {
            if(!statEqual(originalTrack, workingTrack)) {
                hasStatChanges = true;
            }
            continue;
        }

        return false;
    }

    return hasStatChanges;
}

int PropertiesDialogSession::trackRevision(int index) const
{
    if(index < 0 || std::cmp_greater_equal(index, p->m_originalTracks.size())
       || std::cmp_greater_equal(index, p->m_workingTracks.size())
       || std::cmp_greater_equal(index, p->m_pendingRevisions.size())) {
        return 0;
    }

    return p->m_pendingRevisions[index];
}

bool PropertiesTabWidget::canApply() const
{
    return true;
}

void PropertiesTabWidget::load() { }

void PropertiesTabWidget::apply() { }

void PropertiesTabWidget::finish() { }

void PropertiesTabWidget::setSession(PropertiesDialogSession* /*session*/) { }

void PropertiesTabWidget::setTrackScope(const TrackList& tracks)
{
    updateTracks(tracks);
}

bool PropertiesTabWidget::isAvailableForScope(const TrackList& /*tracks*/) const
{
    return true;
}

bool PropertiesTabWidget::hasPendingScopeChanges() const
{
    return false;
}

bool PropertiesTabWidget::commitPendingChanges()
{
    return true;
}

void PropertiesTabWidget::updateTracks(const TrackList& /*tracks*/) { }

class PropertiesTabPrivate
{
public:
    PropertiesTabPrivate(QString title, WidgetBuilder widgetBuilder, int index)
        : m_index{index}
        , m_title{std::move(title)}
        , m_widgetBuilder{std::move(widgetBuilder)}
    { }

    int m_index;
    QString m_title;
    WidgetBuilder m_widgetBuilder;
    QPointer<PropertiesTabWidget> m_widget;
    bool m_loaded{false};
    bool m_visited{false};
};

PropertiesTab::PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index)
    : p{std::make_unique<PropertiesTabPrivate>(std::move(title), std::move(widgetBuilder), index)}
{ }

PropertiesTab::~PropertiesTab() = default;

PropertiesTab::PropertiesTab(const PropertiesTab& other)
    : p{std::make_unique<PropertiesTabPrivate>(*other.p)}
{ }

PropertiesTab& PropertiesTab::operator=(const PropertiesTab& other)
{
    if(this != &other) {
        *p = *other.p;
    }
    return *this;
}

int PropertiesTab::index() const
{
    return p->m_index;
}

QString PropertiesTab::title() const
{
    return p->m_title;
}

PropertiesTabWidget* PropertiesTab::widget(const TrackList& tracks) const
{
    if(!p->m_widget) {
        if(p->m_widgetBuilder) {
            p->m_widget = p->m_widgetBuilder(tracks);
        }
    }
    return p->m_widget;
}

bool PropertiesTab::hasVisited() const
{
    return p->m_visited;
}

void PropertiesTab::load(const TrackList& tracks)
{
    if(p->m_loaded) {
        return;
    }

    if(auto* propertiesWidget = widget(tracks)) {
        propertiesWidget->load();
        p->m_loaded = true;
    }
}

void PropertiesTab::updateIndex(int index)
{
    p->m_index = index;
}

void PropertiesTab::setVisited(bool visited)
{
    p->m_visited = visited;
}

void PropertiesTab::apply()
{
    if(p->m_widget) {
        p->m_widget->apply();
    }
}

void PropertiesTab::finish()
{
    p->m_loaded = false;
    setVisited(false);

    if(p->m_widget) {
        auto* widget = p->m_widget.data();
        p->m_widget  = nullptr;
        widget->finish();
        widget->deleteLater();
    }
}

class PropertiesDialogWidget : public QDialog
{
    Q_OBJECT

public:
    explicit PropertiesDialogWidget(ActionManager* actionManager, SettingsManager* settings, const TrackList& tracks,
                                    PropertiesDialog::TabList tabs);

    [[nodiscard]] QSize sizeHint() const override
    {
        return Utils::proportionateSize(this, 0.25, 0.5);
    }

    void saveState()
    {
        FyStateSettings stateSettings;
        Utils::saveState(this, stateSettings, QLatin1String{PropertiesDialogGroup});
        stateSettings.setValue(SidebarVisibleKey, m_scopePanelPreferredVisible);
        stateSettings.setValue(SidebarWidthKey, sidebarPanelWidth());
        stateSettings.setValue(BaseWidthKey, std::max(0, width() - scopeHostWidth()));
    }

    void restoreState()
    {
        const FyStateSettings stateSettings;
        Utils::restoreState(this, stateSettings, QLatin1String{PropertiesDialogGroup});

        m_scopePanelWidth = std::max(0, stateSettings.value(SidebarWidthKey, 0).toInt());

        const bool scopePanelVisible          = stateSettings.value(SidebarVisibleKey, false).toBool();
        const bool effectiveScopePanelVisible = scopePanelVisible && canShowScopePanel();
        const int fallbackBaseWidth           = std::max(0, width() - (scopePanelVisible ? sidebarPanelWidth() : 0));
        const int baseWidth                   = stateSettings.value(BaseWidthKey, fallbackBaseWidth).toInt();

        m_scopePanelPreferredVisible = scopePanelVisible;
        applyScopePanelState(effectiveScopePanelVisible, baseWidth);

        refreshScopeNavigation();
    }

    void done(int value) override;
    void accept() override;
    void reject() override;

    void moveScope(int offset);
    void setScopePanelVisible(bool visible, bool updatePreference = true);

private:
    void apply();
    void addWriteRequest(WriteRequest request);
    void updateTracks(const TrackList& tracks);

    void buildScopePanel();
    void refreshScopePanel();
    void syncScopeSelection();
    void updateScopePanelButtons(bool hasMultipleTracks);
    void refreshScopeNavigation();
    void switchScope();
    void applyTrackScope(PropertiesTabWidget* widget) const;
    void applyScopePanelState(bool visible, int baseWidth);
    void updateTabAvailability();

    [[nodiscard]] PropertiesTab* tabForIndex(int index);
    [[nodiscard]] const PropertiesTab* tabForIndex(int index) const;
    [[nodiscard]] PropertiesTabWidget* tabWidgetForIndex(int index) const;
    [[nodiscard]] PropertiesTabWidget* currentTabWidget() const;
    [[nodiscard]] bool commitPendingChanges(PropertiesTabWidget* widget);
    [[nodiscard]] bool commitCurrentTabPendingChanges();
    [[nodiscard]] bool canShowScopePanel() const;
    [[nodiscard]] QString scopeLabel(int trackIndex) const;
    [[nodiscard]] int sidebarPanelWidth() const;
    [[nodiscard]] int scopeHostWidth() const;

    void currentTabChanged(int index);

    bool m_closing{false};
    bool m_restoringTab{false};
    ActionManager* m_actionManager{nullptr};
    SettingsManager* m_settings{nullptr};
    mutable ScriptParser m_scriptParser;
    Id m_actionContext;
    WidgetContext* m_context;
    QAction* m_toggleScopeAction;
    QAction* m_previousTrackAction;
    QAction* m_nextTrackAction;
    QPushButton* m_applyButton{nullptr};
    QTabWidget* m_tabWidget{nullptr};
    ToolButton* m_scopeButton;
    ToolButton* m_previousTrackButton;
    ToolButton* m_nextTrackButton;
    QSplitter* m_splitter;
    QGridLayout* m_bottomLayout{nullptr};
    QWidget* m_scopeHost;
    QWidget* m_scopePanel;
    QLabel* m_scopeHeader;
    QListWidget* m_scopeList;
    PropertiesDialog::TabList m_tabs;
    PropertiesDialogSession m_session;
    int m_currentTabIndex{-1};
    bool m_scopePanelPreferredVisible{false};
    bool m_scopePanelVisible{false};
    int m_scopePanelWidth{0};
    std::vector<int> m_itemRevisions;
    QPointer<PropertiesWriteProgress> m_writeProgress;
};

PropertiesDialogWidget::PropertiesDialogWidget(ActionManager* actionManager, SettingsManager* settings,
                                               const TrackList& tracks, PropertiesDialog::TabList tabs)
    : QDialog{Utils::getMainWindow()}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_actionContext{Id{PropertiesDialogContext}.append(Utils::generateUniqueHash())}
    , m_context{new WidgetContext(this, Context{IdList{m_actionContext, Id{PropertiesDialogContext}}}, this)}
    , m_toggleScopeAction{new QAction(PropertiesDialog::tr("Toggle Sidebar"), this)}
    , m_previousTrackAction{new QAction(PropertiesDialog::tr("Previous Track"), this)}
    , m_nextTrackAction{new QAction(PropertiesDialog::tr("Next Track"), this)}
    , m_tabWidget(new QTabWidget(this))
    , m_scopeButton{new ToolButton(this)}
    , m_previousTrackButton{new ToolButton(this)}
    , m_nextTrackButton{new ToolButton(this)}
    , m_splitter{new QSplitter(Qt::Horizontal, this)}
    , m_bottomLayout(new QGridLayout())
    , m_scopeHost{new QWidget(this)}
    , m_scopePanel{new QWidget(m_scopeHost)}
    , m_scopeHeader{new QLabel(tr("Selection"), m_scopePanel)}
    , m_scopeList{new QListWidget(m_scopePanel)}
    , m_tabs{std::move(tabs)}
{
    setWindowTitle(tr("Properties"));

    m_actionManager->addContextObject(m_context);
    m_session.reset(tracks);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);

    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                     &PropertiesDialogWidget::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(m_tabWidget, &QTabWidget::currentChanged, this, &PropertiesDialogWidget::currentTabChanged);

    m_applyButton = buttonBox->button(QDialogButtonBox::Apply);
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    buttonBox->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
    m_tabWidget->setCurrentIndex(0);

    auto* scopeLayout = new QVBoxLayout(m_scopePanel);
    scopeLayout->setContentsMargins({});
    m_scopeHeader->setAlignment(Qt::AlignHCenter);
    scopeLayout->addWidget(m_scopeHeader);
    scopeLayout->addWidget(m_scopeList);
    m_scopePanel->hide();
    m_scopeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_scopeList->setUniformItemSizes(true);
    m_scopeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scopeList->setTextElideMode(Qt::ElideRight);
    m_scopeList->setWordWrap(false);
    m_scopeList->setMinimumWidth(m_scopeList->fontMetrics().averageCharWidth() * 16);

    m_toggleScopeAction->setCheckable(true);
    Gui::setThemeIcon(m_toggleScopeAction, Constants::Icons::SidebarRight);
    Gui::setThemeIcon(m_previousTrackAction, Constants::Icons::GoPrevious);
    Gui::setThemeIcon(m_nextTrackAction, Constants::Icons::GoNext);

    QObject::connect(m_toggleScopeAction, &QAction::triggered, this,
                     [this](bool visible) { setScopePanelVisible(visible); });
    QObject::connect(m_previousTrackAction, &QAction::triggered, this, [this]() { moveScope(-1); });
    QObject::connect(m_nextTrackAction, &QAction::triggered, this, [this]() { moveScope(1); });

    const Context actionContext{m_actionContext};
    auto* toggleCmd = m_actionManager->registerAction(m_toggleScopeAction,
                                                      Constants::Actions::TogglePropertiesTrackList, actionContext);
    m_scopeButton->setDefaultAction(toggleCmd->action());
    auto* previousCmd = m_actionManager->registerAction(m_previousTrackAction,
                                                        Constants::Actions::PropertiesPreviousTrack, actionContext);
    m_previousTrackButton->setDefaultAction(previousCmd->action());
    auto* nextCmd
        = m_actionManager->registerAction(m_nextTrackAction, Constants::Actions::PropertiesNextTrack, actionContext);
    m_nextTrackButton->setDefaultAction(nextCmd->action());

    QObject::connect(m_scopeList, &QListWidget::itemSelectionChanged, this, &PropertiesDialogWidget::switchScope);
    QObject::connect(m_scopeList, &QListWidget::currentRowChanged, this, [this](int) { refreshScopeNavigation(); });

    m_previousTrackButton->hide();
    m_nextTrackButton->hide();

    auto* scopeHostLayout = new QVBoxLayout(m_scopeHost);
    scopeHostLayout->addWidget(m_scopePanel);
    m_scopeHost->hide();

    QTimer::singleShot(0, this, [this]() {
        const QSize buttonSize = m_scopeButton->sizeHint();
        m_scopeButton->setMinimumHeight(buttonSize.height());
        m_previousTrackButton->setMinimumHeight(buttonSize.height());
        m_nextTrackButton->setMinimumHeight(buttonSize.height());
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    auto* mainWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins({});

    m_bottomLayout->setContentsMargins(5, 0, 5, 5);
    m_bottomLayout->addWidget(m_scopeButton, 1, 0);
    m_bottomLayout->addWidget(m_previousTrackButton, 1, 1);
    m_bottomLayout->addWidget(m_nextTrackButton, 1, 2);
    m_bottomLayout->addWidget(buttonBox, 1, 5);

    mainLayout->addWidget(m_tabWidget, 1);
    mainLayout->addLayout(m_bottomLayout);

    m_splitter->addWidget(mainWidget);
    m_splitter->addWidget(m_scopeHost);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    layout->addWidget(m_splitter, 1);

    QObject::connect(m_splitter, &QSplitter::splitterMoved, this, [this]() {
        if(m_scopePanelVisible) {
            m_scopePanelWidth = m_scopeHost->width();
        }
    });

    for(const auto& tab : m_tabs) {
        if(auto* tabPage = tab.widget(m_session.workingTracks())) {
            tabPage->setSession(&m_session);
            tabPage->setTrackScope(m_session.activeTracks());
            m_tabWidget->insertTab(tab.index(), tabPage, tab.title());
            QObject::connect(tabPage, &PropertiesTabWidget::tracksChanged, this, &PropertiesDialogWidget::updateTracks);
            QObject::connect(tabPage, &PropertiesTabWidget::pendingChangesStateChanged, this,
                             &PropertiesDialogWidget::refreshScopePanel);
            QObject::connect(tabPage, &PropertiesTabWidget::writeRequestStarted, this,
                             &PropertiesDialogWidget::addWriteRequest);
        }
    }

    buildScopePanel();
    updateTabAvailability();
    currentTabChanged(m_tabWidget->currentIndex());
}

void PropertiesDialogWidget::done(int value)
{
    QDialog::done(value);
}

void PropertiesDialogWidget::accept()
{
    apply();
    m_closing = true;
    for(PropertiesTab& tab : m_tabs) {
        tab.finish();
    }
    done(Accepted);
}

void PropertiesDialogWidget::reject()
{
    m_closing = true;
    for(PropertiesTab& tab : m_tabs) {
        tab.finish();
    }
    done(Rejected);
}

void PropertiesDialogWidget::apply()
{
    if(m_writeProgress) {
        m_writeProgress->beginCollecting();
    }

    auto visitedTabs = std::views::filter(m_tabs, [&](const PropertiesTab& tab) { return tab.hasVisited(); });
    for(PropertiesTab& tab : visitedTabs) {
        tab.apply();
    }

    if(m_writeProgress) {
        m_writeProgress->finishCollecting();
    }

    m_session.acceptChanges();
    refreshScopePanel();
}

void PropertiesDialogWidget::addWriteRequest(WriteRequest request)
{
    if(!request.finished.isValid()) {
        return;
    }

    if(!m_writeProgress) {
        m_writeProgress
            = new PropertiesWriteProgress(static_cast<int>(m_session.workingTracks().size()), Utils::getMainWindow());
    }
    m_writeProgress->addRequest(std::move(request));
}

void PropertiesDialogWidget::updateTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_session.updateTracks(tracks);
    refreshScopePanel();

    auto* sourceWidget = qobject_cast<PropertiesTabWidget*>(sender());
    for(auto& tab : m_tabs) {
        auto* widget = tab.widget(m_session.workingTracks());
        if(!widget || widget == sourceWidget) {
            continue;
        }

        widget->setSession(&m_session);
        applyTrackScope(widget);
    }
}

void PropertiesDialogWidget::currentTabChanged(int index)
{
    if(index < 0 || m_closing) {
        return;
    }

    if(!m_restoringTab && m_currentTabIndex >= 0 && m_currentTabIndex != index) {
        if(!commitPendingChanges(tabWidgetForIndex(m_currentTabIndex))) {
            const QSignalBlocker blocker{m_tabWidget};
            m_restoringTab = true;
            m_tabWidget->setCurrentIndex(m_currentTabIndex);
            m_restoringTab = false;
            return;
        }
    }

    auto* tab = tabForIndex(index);
    if(!tab) {
        return;
    }

    m_currentTabIndex = index;
    tab->setVisited(true);
    QString windowTitle         = tr("Properties");
    const TrackList titleTracks = m_session.activeTracks();
    windowTitle += u" - "_s
                 + (titleTracks.size() == 1 ? titleTracks.front().effectiveTitle()
                                            : tr("%Ln track(s)", nullptr, static_cast<int>(titleTracks.size())));
    windowTitle += u" - "_s + tab->title();

    setWindowTitle(windowTitle);

    if(auto* widget = tab->widget(m_session.workingTracks())) {
        widget->setSession(&m_session);
        applyTrackScope(widget);
        tab->load(m_session.workingTracks());
        if(m_applyButton) {
            m_applyButton->setHidden(!widget->canApply());
        }
    }
}

void PropertiesDialogWidget::moveScope(int offset)
{
    const int count = m_scopeList->count();
    if(count <= 1 || offset == 0) {
        return;
    }

    const int currentRow = std::max(m_scopeList->currentRow(), 0);
    const int nextRow    = std::clamp(currentRow + offset, 0, count - 1);
    if(nextRow == currentRow) {
        return;
    }

    {
        const QSignalBlocker blocker{m_scopeList};
        m_scopeList->clearSelection();
        if(auto* item = m_scopeList->item(nextRow)) {
            item->setSelected(true);
        }
        m_scopeList->setCurrentRow(nextRow);
    }

    switchScope();
}

void PropertiesDialogWidget::updateTabAvailability()
{
    const TrackList activeTracks = m_session.activeTracks();

    for(const auto& tab : m_tabs) {
        auto* widget = tab.widget(m_session.workingTracks());
        if(!widget) {
            continue;
        }

        m_tabWidget->setTabEnabled(tab.index(), widget->isAvailableForScope(activeTracks));
    }

    const int currentIndex = m_tabWidget->currentIndex();
    if(currentIndex >= 0 && m_tabWidget->isTabEnabled(currentIndex)) {
        return;
    }

    for(int i{0}; i < m_tabWidget->count(); ++i) {
        if(m_tabWidget->isTabEnabled(i)) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }
}

void PropertiesDialogWidget::buildScopePanel()
{
    const TrackList& tracks      = m_session.workingTracks();
    const bool hasMultipleTracks = canShowScopePanel();

    updateScopePanelButtons(hasMultipleTracks);

    const QSignalBlocker blocker{m_scopeList};
    m_scopeList->clear();

    if(!hasMultipleTracks) {
        setScopePanelVisible(m_scopePanelPreferredVisible, false);
        refreshScopeNavigation();
        return;
    }

    m_itemRevisions.resize(static_cast<qsizetype>(tracks.size()) + 1);

    m_scopeList->addItem(scopeLabel(-1));
    for(size_t i{0}; i < tracks.size(); ++i) {
        m_scopeList->addItem(scopeLabel(static_cast<int>(i)));
    }

    syncScopeSelection();

    setScopePanelVisible(m_scopePanelPreferredVisible, false);
    refreshScopeNavigation();
}

void PropertiesDialogWidget::refreshScopePanel()
{
    const bool hasMultipleTracks = canShowScopePanel();
    updateScopePanelButtons(hasMultipleTracks);

    if(!hasMultipleTracks) {
        setScopePanelVisible(m_scopePanelPreferredVisible, false);
        refreshScopeNavigation();
        return;
    }

    const auto* currentWidget         = currentTabWidget();
    const bool hasPendingScopeChanges = currentWidget && currentWidget->hasPendingScopeChanges();
    const bool pendingAllTracks       = hasPendingScopeChanges && m_session.isAllTracksScope();
    const bool pendingAnyTracks       = hasPendingScopeChanges && !m_session.activeTrackIndexes().empty();

    if(auto* allTracksItem = m_scopeList->item(0)) {
        const bool markAllTracksPending = m_session.hasChanges() || pendingAllTracks || pendingAnyTracks;
        allTracksItem->setText((markAllTracksPending ? u"* "_s : QString{}) + scopeLabel(-1));
    }

    const TrackList& tracks = m_session.workingTracks();

    for(size_t i{0}; i < tracks.size(); ++i) {
        const int trackIndex    = static_cast<int>(i);
        const bool pendingTrack = hasPendingScopeChanges && m_session.activeTrackIndexes().contains(trackIndex);
        int revision            = m_session.trackRevision(trackIndex);
        if(pendingTrack || revision != m_itemRevisions[trackIndex + 1]) {
            if(auto* item = m_scopeList->item(trackIndex + 1)) {
                item->setText((pendingTrack || revision > 0 ? u"* "_s : QString{}) + scopeLabel(trackIndex));
            }
            if(pendingTrack) {
                revision++;
            }
            m_itemRevisions[trackIndex + 1] = revision;
        }
    }

    setScopePanelVisible(m_scopePanelPreferredVisible, false);
    refreshScopeNavigation();
}

void PropertiesDialogWidget::updateScopePanelButtons(bool hasMultipleTracks)
{
    m_scopeButton->setVisible(hasMultipleTracks);
    m_scopeButton->setEnabled(hasMultipleTracks);
    m_previousTrackButton->setVisible(hasMultipleTracks);
    m_nextTrackButton->setVisible(hasMultipleTracks);
}

void PropertiesDialogWidget::syncScopeSelection()
{
    const QSignalBlocker blocker{m_scopeList};
    m_scopeList->clearSelection();

    if(m_session.isAllTracksScope()) {
        if(auto* item = m_scopeList->item(0)) {
            item->setSelected(true);
        }
        m_scopeList->setCurrentRow(0);
        return;
    }

    bool currentSet{false};
    for(const int index : m_session.activeTrackIndexes()) {
        if(auto* item = m_scopeList->item(index + 1)) {
            item->setSelected(true);
            if(!currentSet) {
                m_scopeList->setCurrentRow(index + 1);
                currentSet = true;
            }
        }
    }
}

void PropertiesDialogWidget::setScopePanelVisible(bool visible, bool updatePreference)
{
    if(updatePreference) {
        m_scopePanelPreferredVisible = visible;
    }

    const bool canShow = visible && canShowScopePanel();
    if(canShow == m_scopePanelVisible) {
        return;
    }

    applyScopePanelState(canShow, std::max(0, width() - scopeHostWidth()));
}

void PropertiesDialogWidget::refreshScopeNavigation()
{
    const bool hasMultipleTracks = canShowScopePanel();
    const int currentRow         = std::max(m_scopeList->currentRow(), 0);
    const int count              = m_scopeList->count();
    const bool canMovePrevious   = hasMultipleTracks && currentRow > 0;
    const bool canMoveNext       = hasMultipleTracks && currentRow >= 0 && currentRow < count - 1;

    m_toggleScopeAction->setEnabled(hasMultipleTracks);
    m_toggleScopeAction->setChecked(m_scopePanelVisible);
    m_previousTrackAction->setEnabled(canMovePrevious);
    m_nextTrackAction->setEnabled(canMoveNext);
}

void PropertiesDialogWidget::switchScope()
{
    if(!commitCurrentTabPendingChanges()) {
        syncScopeSelection();
        refreshScopePanel();
        return;
    }

    auto selectedIndexes = m_scopeList->selectionModel()->selectedRows();

    std::vector<int> selectedRows;
    selectedRows.reserve(selectedIndexes.size());

    for(const QModelIndex& index : std::as_const(selectedIndexes)) {
        selectedRows.emplace_back(index.row());
    }

    const int currentRow = m_scopeList->currentRow();
    if(selectedRows.empty() && currentRow >= 0) {
        selectedRows.emplace_back(currentRow);
    }

    std::set<int> selectedTrackIndexes;
    bool allTracksSelected{false};

    if(selectedRows.size() > 1) {
        const QSignalBlocker blocker{m_scopeList};
        if(currentRow <= 0) {
            for(int row{1}; row < m_scopeList->count(); ++row) {
                if(auto* item = m_scopeList->item(row)) {
                    item->setSelected(false);
                }
            }
            selectedRows = {0};
        }
        else if(auto* allTracksItem = m_scopeList->item(0)) {
            if(std::ranges::find(selectedRows, 0) != selectedRows.cend()) {
                allTracksItem->setSelected(false);
                std::erase(selectedRows, 0);
            }
        }
    }

    for(const int row : selectedRows) {
        if(row == 0) {
            allTracksSelected = true;
            break;
        }

        selectedTrackIndexes.emplace(row - 1);
    }

    if(allTracksSelected || selectedTrackIndexes.empty()) {
        selectedTrackIndexes.clear();
    }

    if(!m_session.setActiveTrackIndexes(std::move(selectedTrackIndexes))) {
        return;
    }

    if(auto* widget = currentTabWidget()) {
        applyTrackScope(widget);
    }

    refreshScopePanel();
    updateTabAvailability();
    currentTabChanged(m_tabWidget->currentIndex());
}

void PropertiesDialogWidget::applyTrackScope(PropertiesTabWidget* widget) const
{
    if(widget) {
        widget->setTrackScope(m_session.activeTracks());
    }
}

void PropertiesDialogWidget::applyScopePanelState(bool visible, int baseWidth)
{
    m_scopePanelVisible = visible;
    m_scopeHost->setVisible(visible);
    m_scopePanel->setVisible(visible);

    const int margin = visible ? m_scopeHost->layout()->contentsMargins().left() : 0;
    Gui::setContentsMargins(m_splitter->widget(0)->layout(), Qt::RightEdge, margin);
    layout()->activate();

    const int sidebarWidth  = visible ? sidebarPanelWidth() : 0;
    const int reservedWidth = visible ? sidebarWidth + m_splitter->handleWidth() : 0;
    resize(std::max(minimumWidth(), baseWidth + reservedWidth), height());
    m_splitter->setSizes({std::max(0, width() - reservedWidth), sidebarWidth});

    m_toggleScopeAction->setChecked(visible);
}

PropertiesTab* PropertiesDialogWidget::tabForIndex(int index)
{
    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    return tabIt != m_tabs.end() ? &*tabIt : nullptr;
}

const PropertiesTab* PropertiesDialogWidget::tabForIndex(int index) const
{
    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    return tabIt != m_tabs.cend() ? &*tabIt : nullptr;
}

PropertiesTabWidget* PropertiesDialogWidget::currentTabWidget() const
{
    if(!m_tabWidget) {
        return nullptr;
    }

    return tabWidgetForIndex(m_tabWidget->currentIndex());
}

PropertiesTabWidget* PropertiesDialogWidget::tabWidgetForIndex(int index) const
{
    const auto* tab = tabForIndex(index);
    if(!tab) {
        return nullptr;
    }

    return tab->widget(m_session.workingTracks());
}

bool PropertiesDialogWidget::commitPendingChanges(PropertiesTabWidget* widget)
{
    if(!widget || widget->commitPendingChanges()) {
        return true;
    }

    QMessageBox::warning(this, tr("Pending Changes"),
                         tr("Apply or clear the current tab’s pending changes before switching tracks or tabs."));
    return false;
}

bool PropertiesDialogWidget::commitCurrentTabPendingChanges()
{
    return commitPendingChanges(currentTabWidget());
}

bool PropertiesDialogWidget::canShowScopePanel() const
{
    return m_session.workingTracks().size() > 1;
}

QString PropertiesDialogWidget::scopeLabel(int trackIndex) const
{
    if(trackIndex < 0) {
        return tr("All Tracks (%1)").arg(m_session.workingTracks().size());
    }

    if(std::cmp_greater_equal(trackIndex, m_session.workingTracks().size())) {
        return {};
    }

    const Track& track = m_session.workingTracks()[trackIndex];

    QString script = m_settings->value<Settings::Gui::Internal::PropertiesSidebarTrackScript>();
    if(script.isEmpty()) {
        script = u"[%track%. ]%title%"_s;
    }

    const QString trackScopeLabel = m_scriptParser.evaluate(script, track);
    return trackScopeLabel;
}

int PropertiesDialogWidget::scopeHostWidth() const
{
    return m_scopePanelVisible ? m_scopeHost->width() + m_splitter->handleWidth() : 0;
}

int PropertiesDialogWidget::sidebarPanelWidth() const
{
    const int preferredWidth = m_scopePanelWidth > 0 ? m_scopePanelWidth : m_scopePanel->sizeHint().width();
    return std::max(preferredWidth, m_scopeHost->minimumSizeHint().width());
}

class PropertiesDialogPrivate
{
public:
    PropertiesDialogPrivate(ActionManager* actionManager, SettingsManager* settings)
        : m_actionManager{actionManager}
        , m_settings{settings}
    { }

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    PropertiesDialog::TabList m_tabs;
};

PropertiesDialog::PropertiesDialog(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PropertiesDialogPrivate>(actionManager, settings)}
{
    const QStringList category{tr("Tracks"), tr("Properties")};

    auto* toggleCmd = p->m_actionManager->registerCommand(Constants::Actions::TogglePropertiesTrackList);
    toggleCmd->setCategories(category);
    toggleCmd->setDescription(tr("Toggle Sidebar"));
    toggleCmd->setShortcutConflictScope(PropertiesDialogContext);
    toggleCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_B});

    auto* previousCmd = p->m_actionManager->registerCommand(Constants::Actions::PropertiesPreviousTrack);
    previousCmd->setCategories(category);
    previousCmd->setDescription(tr("Previous Track"));
    previousCmd->setShortcutConflictScope(PropertiesDialogContext);

    auto* nextCmd = p->m_actionManager->registerCommand(Constants::Actions::PropertiesNextTrack);
    nextCmd->setCategories(category);
    nextCmd->setDescription(tr("Next Track"));
    nextCmd->setShortcutConflictScope(PropertiesDialogContext);
}

PropertiesDialog::~PropertiesDialog() = default;

void PropertiesDialog::addTab(const QString& title, const WidgetBuilder& widgetBuilder)
{
    const int index = static_cast<int>(p->m_tabs.size());
    p->m_tabs.emplace_back(title, widgetBuilder, index);
}

void PropertiesDialog::addTab(const PropertiesTab& tab)
{
    const int index = static_cast<int>(p->m_tabs.size());
    PropertiesTab newTab{tab};
    newTab.updateIndex(index);
    p->m_tabs.emplace_back(std::move(newTab));
}

void PropertiesDialog::insertTab(int index, const QString& title, const WidgetBuilder& widgetBuilder)
{
    p->m_tabs.insert(p->m_tabs.begin() + index, {title, widgetBuilder, index});

    const int count = static_cast<int>(p->m_tabs.size());
    for(int i{0}; i < count; ++i) {
        p->m_tabs[i].updateIndex(i);
    }
}

void PropertiesDialog::show(const TrackList& tracks)
{
    auto* dialog = new PropertiesDialogWidget(p->m_actionManager, p->m_settings, tracks, p->m_tabs);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::finished, dialog, &PropertiesDialogWidget::saveState);

    dialog->show();

    dialog->restoreState();
}
} // namespace Fooyin

#include "gui/moc_propertiesdialog.cpp"
#include "propertiesdialog.moc"
