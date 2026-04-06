/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/propertiesdialog.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>

#include <algorithm>
#include <ranges>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto PropertiesDialogGroup   = "PropertiesDialog";
constexpr auto SidebarVisibleKey       = "PropertiesDialog/SidebarVisible";
constexpr auto PropertiesDialogContext = "Fooyin.Context.PropertiesDialog";

namespace {
bool metadataEqual(const Fooyin::Track& lhs, const Fooyin::Track& rhs)
{
    return lhs.metadata() == rhs.metadata() && lhs.serialiseExtraTags() == rhs.serialiseExtraTags()
        && lhs.removedTags() == rhs.removedTags();
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
void PropertiesDialogSession::reset(const TrackList& tracks)
{
    m_originalTracks = tracks;
    m_workingTracks  = tracks;
    m_activeTrackIndexes.clear();
}

const TrackList& PropertiesDialogSession::originalTracks() const
{
    return m_originalTracks;
}

const TrackList& PropertiesDialogSession::workingTracks() const
{
    return m_workingTracks;
}

TrackList PropertiesDialogSession::activeTracks() const
{
    if(m_activeTrackIndexes.empty()) {
        return m_workingTracks;
    }

    TrackList tracks;
    tracks.reserve(static_cast<TrackList::size_type>(m_activeTrackIndexes.size()));

    for(const int index : m_activeTrackIndexes) {
        if(index >= 0 && std::cmp_less(index, m_workingTracks.size())) {
            tracks.emplace_back(m_workingTracks.at(static_cast<TrackList::size_type>(index)));
        }
    }

    return tracks;
}

const std::set<int>& PropertiesDialogSession::activeTrackIndexes() const
{
    return m_activeTrackIndexes;
}

bool PropertiesDialogSession::isAllTracksScope() const
{
    return m_activeTrackIndexes.empty();
}

bool PropertiesDialogSession::setActiveTrackIndexes(std::set<int> trackIndexes)
{
    std::erase_if(trackIndexes, [this](int trackIndex) {
        return trackIndex < 0 || std::cmp_greater_equal(trackIndex, m_workingTracks.size());
    });

    if(trackIndexes.size() == m_workingTracks.size()) {
        trackIndexes.clear();
    }

    if(trackIndexes == m_activeTrackIndexes) {
        return false;
    }

    m_activeTrackIndexes = std::move(trackIndexes);
    return true;
}

void PropertiesDialogSession::updateTracks(const TrackList& tracks)
{
    for(const auto& updatedTrack : tracks) {
        const auto workingIt = std::ranges::find_if(
            m_workingTracks, [&updatedTrack](const Track& track) { return track.sameIdentityAs(updatedTrack); });
        if(workingIt == m_workingTracks.cend()) {
            continue;
        }
        *workingIt = updatedTrack;
    }
}

void PropertiesDialogSession::acceptChanges()
{
    m_originalTracks = m_workingTracks;
}

bool PropertiesDialogSession::hasChanges() const
{
    if(m_originalTracks.size() != m_workingTracks.size()) {
        return true;
    }

    for(size_t i{0}; i < m_originalTracks.size(); ++i) {
        if(!editorEqual(m_originalTracks.at(i), m_workingTracks.at(i))) {
            return true;
        }
    }

    return false;
}

bool PropertiesDialogSession::hasOnlyStatChanges() const
{
    bool hasStatChanges{false};

    for(size_t i{0}; i < m_originalTracks.size(); ++i) {
        const auto& originalTrack = m_originalTracks.at(i);
        const auto& workingTrack  = m_workingTracks.at(i);

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

bool PropertiesDialogSession::isTrackDirty(int index) const
{
    if(index < 0 || std::cmp_greater_equal(index, m_originalTracks.size())
       || std::cmp_greater_equal(index, m_workingTracks.size())) {
        return false;
    }

    return !editorEqual(m_originalTracks.at(static_cast<size_t>(index)),
                        m_workingTracks.at(static_cast<size_t>(index)));
}

bool PropertiesTabWidget::canApply() const
{
    return true;
}

bool PropertiesTabWidget::hasTools() const
{
    return false;
}

void PropertiesTabWidget::load() { }

void PropertiesTabWidget::apply() { }

void PropertiesTabWidget::finish() { }

void PropertiesTabWidget::addTools(QMenu* /*menu*/) { }

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

PropertiesTab::PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index)
    : m_index{index}
    , m_title{std::move(title)}
    , m_widgetBuilder{std::move(widgetBuilder)}
    , m_widget{nullptr}
    , m_loaded{false}
    , m_visited{false}
{ }

int PropertiesTab::index() const
{
    return m_index;
}

QString PropertiesTab::title() const
{
    return m_title;
}

PropertiesTabWidget* PropertiesTab::widget(const TrackList& tracks) const
{
    if(!m_widget) {
        if(m_widgetBuilder) {
            m_widget = m_widgetBuilder(tracks);
        }
    }
    return m_widget;
}

bool PropertiesTab::hasVisited() const
{
    return m_visited;
}

void PropertiesTab::load(const TrackList& tracks)
{
    if(m_loaded) {
        return;
    }

    if(auto* propertiesWidget = widget(tracks)) {
        propertiesWidget->load();
        m_loaded = true;
    }
}

void PropertiesTab::updateIndex(int index)
{
    m_index = index;
}

void PropertiesTab::setVisited(bool visited)
{
    m_visited = visited;
}

void PropertiesTab::apply()
{
    if(m_widget) {
        m_widget->apply();
    }
}

void PropertiesTab::finish()
{
    m_loaded = false;
    setVisited(false);

    if(m_widget) {
        auto* widget = m_widget.data();
        m_widget     = nullptr;
        widget->finish();
        widget->deleteLater();
    }
}

class PropertiesDialogWidget : public QDialog
{
    Q_OBJECT

public:
    explicit PropertiesDialogWidget(ActionManager* actionManager, const TrackList& tracks,
                                    PropertiesDialog::TabList tabs);

    [[nodiscard]] QSize sizeHint() const override
    {
        return Utils::proportionateSize(this, 0.25, 0.5);
    }

    void saveState()
    {
        FyStateSettings stateSettings;
        Utils::saveState(this, stateSettings, QLatin1String{PropertiesDialogGroup});
        stateSettings.setValue(QLatin1String{SidebarVisibleKey}, m_scopePanelVisible);
    }

    void restoreState()
    {
        const FyStateSettings stateSettings;
        Utils::restoreState(this, stateSettings, QLatin1String{PropertiesDialogGroup});

        const bool scopePanelVisible = stateSettings.value(QLatin1String{SidebarVisibleKey}, false).toBool();
        const bool hasMultipleTracks = m_session.workingTracks().size() > 1;
        const bool canShowScopePanel = scopePanelVisible && hasMultipleTracks;
        const int reservedWidth      = scopeHostWidth(canShowScopePanel);

        m_previousTrackButton->setVisible(hasMultipleTracks);
        m_nextTrackButton->setVisible(hasMultipleTracks);
        m_scopeHost->setFixedWidth(reservedWidth);
        m_scopeHost->setVisible(reservedWidth > 0);
        m_scopePanelVisible = canShowScopePanel;
        m_scopePanel->setVisible(m_scopePanelVisible);
        m_scopePanelReservedWidth = reservedWidth;

        if(m_scopeButton->isChecked() != canShowScopePanel) {
            const QSignalBlocker blocker{m_scopeButton};
            m_scopeButton->setChecked(canShowScopePanel);
        }

        refreshScopeNavigation();
    }

    void done(int value) override;
    void accept() override;
    void reject() override;

    [[nodiscard]] bool scopePanelVisible() const
    {
        return m_scopePanelVisible;
    }
    void moveScope(int offset);
    void setScopePanelVisible(bool visible);

private:
    void apply();
    void updateTracks(const TrackList& tracks);

    void refreshScopePanel();
    void refreshScopeNavigation();
    void switchScope();
    void applyTrackScope(PropertiesTabWidget* widget) const;
    void updateTabAvailability();

    [[nodiscard]] PropertiesTab* tabForIndex(int index);
    [[nodiscard]] PropertiesTabWidget* tabWidgetForIndex(int index) const;
    [[nodiscard]] PropertiesTabWidget* currentTabWidget() const;
    [[nodiscard]] bool commitPendingChanges(PropertiesTabWidget* widget);
    [[nodiscard]] bool commitCurrentTabPendingChanges();
    [[nodiscard]] QString scopeLabel(int trackIndex) const;
    [[nodiscard]] int scopeHostWidth(bool scopePanelVisible) const;

    void currentTabChanged(int index);

    bool m_closing{false};
    bool m_restoringTab{false};
    ActionManager* m_actionManager{nullptr};
    WidgetContext* m_context;
    QPushButton* m_applyButton{nullptr};
    QTabWidget* m_tabWidget{nullptr};
    ToolButton* m_scopeButton;
    ToolButton* m_previousTrackButton;
    ToolButton* m_nextTrackButton;
    QToolButton* m_toolsButton;
    QMenu* m_toolsMenu;
    QHBoxLayout* m_contentLayout{nullptr};
    QGridLayout* m_bottomLayout{nullptr};
    QWidget* m_scopeHost;
    QWidget* m_scopePanel;
    QLabel* m_scopeHeader;
    QListWidget* m_scopeList;
    PropertiesDialog::TabList m_tabs;
    PropertiesDialogSession m_session;
    int m_currentTabIndex{-1};
    bool m_scopePanelVisible{false};
    int m_scopePanelReservedWidth{0};
};

namespace {
PropertiesDialogWidget* activePropertiesDialogWidget()
{
    QWidget* widget = QApplication::focusWidget();
    while(widget) {
        if(auto* dialog = qobject_cast<PropertiesDialogWidget*>(widget)) {
            return dialog;
        }
        widget = widget->parentWidget();
    }
    return nullptr;
}
} // namespace

PropertiesDialogWidget::PropertiesDialogWidget(ActionManager* actionManager, const TrackList& tracks,
                                               PropertiesDialog::TabList tabs)
    : m_actionManager{actionManager}
    , m_context{new WidgetContext(this, Context{PropertiesDialogContext}, this)}
    , m_tabWidget(new QTabWidget(this))
    , m_scopeButton{new ToolButton(this)}
    , m_previousTrackButton{new ToolButton(this)}
    , m_nextTrackButton{new ToolButton(this)}
    , m_toolsButton{new QToolButton(this)}
    , m_toolsMenu{new QMenu(tr("Tools"), this)}
    , m_contentLayout(new QHBoxLayout())
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

    if(auto* toggleCmd = m_actionManager->command(Constants::Actions::TogglePropertiesTrackList)) {
        m_scopeButton->setDefaultAction(toggleCmd->action());
    }
    if(auto* previousCmd = m_actionManager->command(Constants::Actions::PropertiesPreviousTrack)) {
        m_previousTrackButton->setDefaultAction(previousCmd->action());
    }
    if(auto* nextCmd = m_actionManager->command(Constants::Actions::PropertiesNextTrack)) {
        m_nextTrackButton->setDefaultAction(nextCmd->action());
    }

    QObject::connect(m_scopeList, &QListWidget::itemSelectionChanged, this, &PropertiesDialogWidget::switchScope);
    QObject::connect(m_scopeList, &QListWidget::currentRowChanged, this, [this](int) { refreshScopeNavigation(); });

    m_previousTrackButton->hide();
    m_nextTrackButton->hide();

    auto* scopeHostLayout = new QVBoxLayout(m_scopeHost);
    scopeHostLayout->addWidget(m_scopePanel);
    m_scopeHost->setFixedWidth(0);

    m_toolsButton->setText(tr("Tools"));
    m_toolsButton->setMenu(m_toolsMenu);
    m_toolsButton->setPopupMode(QToolButton::InstantPopup);

    QTimer::singleShot(0, this, [this]() {
        const QSize buttonSize = m_toolsButton->sizeHint().expandedTo(m_scopeButton->sizeHint());
        m_scopeButton->setMinimumHeight(buttonSize.height());
        m_previousTrackButton->setMinimumHeight(buttonSize.height());
        m_nextTrackButton->setMinimumHeight(buttonSize.height());
        m_toolsButton->setMinimumHeight(buttonSize.height());
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
    m_bottomLayout->addWidget(m_toolsButton, 1, 3);
    m_bottomLayout->addWidget(buttonBox, 1, 5);

    mainLayout->addWidget(m_tabWidget, 1);
    mainLayout->addLayout(m_bottomLayout);

    m_contentLayout->setContentsMargins({});
    m_contentLayout->addWidget(mainWidget, 1);
    m_contentLayout->addWidget(m_scopeHost);
    layout->addLayout(m_contentLayout, 1);

    for(const auto& tab : m_tabs) {
        if(auto* tabPage = tab.widget(m_session.workingTracks())) {
            tabPage->setSession(&m_session);
            tabPage->setTrackScope(m_session.activeTracks());
            m_tabWidget->insertTab(tab.index(), tabPage, tab.title());
            QObject::connect(tabPage, &PropertiesTabWidget::tracksChanged, this, &PropertiesDialogWidget::updateTracks);
            QObject::connect(tabPage, &PropertiesTabWidget::pendingChangesStateChanged, this,
                             &PropertiesDialogWidget::refreshScopePanel);
        }
    }

    refreshScopePanel();
    updateTabAvailability();
    currentTabChanged(m_tabWidget->currentIndex());
}

void PropertiesDialogWidget::done(int value)
{
    QDialog::done(value);
}

void PropertiesDialogWidget::accept()
{
    if(!commitCurrentTabPendingChanges()) {
        return;
    }

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
    if(!commitCurrentTabPendingChanges()) {
        return;
    }

    auto visitedTabs = std::views::filter(m_tabs, [&](const PropertiesTab& tab) { return tab.hasVisited(); });
    for(PropertiesTab& tab : visitedTabs) {
        tab.apply();
    }

    m_session.acceptChanges();
    refreshScopePanel();
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
        if(widget->hasTools()) {
            m_toolsButton->show();
            m_toolsMenu->clear();
            widget->addTools(m_toolsMenu);
            return;
        }
    }

    m_toolsButton->hide();
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

void PropertiesDialogWidget::refreshScopePanel()
{
    const TrackList& tracks           = m_session.workingTracks();
    const bool hasMultipleTracks      = tracks.size() > 1;
    const auto* currentWidget         = currentTabWidget();
    const bool hasPendingScopeChanges = currentWidget && currentWidget->hasPendingScopeChanges();
    const bool pendingAllTracks       = hasPendingScopeChanges && m_session.isAllTracksScope();
    const bool pendingAnyTracks       = hasPendingScopeChanges && !m_session.activeTrackIndexes().empty();

    m_scopeButton->setVisible(true);
    m_scopeButton->setEnabled(hasMultipleTracks);
    m_previousTrackButton->setVisible(hasMultipleTracks);
    m_nextTrackButton->setVisible(hasMultipleTracks);

    const QSignalBlocker blocker{m_scopeList};
    m_scopeList->clear();

    if(!hasMultipleTracks) {
        setScopePanelVisible(false);
        refreshScopeNavigation();
        return;
    }

    m_scopeList->addItem(((m_session.hasChanges() || pendingAllTracks || pendingAnyTracks) ? u"* "_s : QString{})
                         + scopeLabel(-1));

    for(size_t i{0}; i < tracks.size(); ++i) {
        const int trackIndex = static_cast<int>(i);
        const bool pendingTrack
            = hasPendingScopeChanges && !pendingAllTracks && m_session.activeTrackIndexes().contains(trackIndex);
        m_scopeList->addItem(((m_session.isTrackDirty(trackIndex) || pendingTrack) ? u"* "_s : QString{})
                             + scopeLabel(trackIndex));
    }

    if(m_session.isAllTracksScope()) {
        if(auto* item = m_scopeList->item(0)) {
            item->setSelected(true);
        }
        m_scopeList->setCurrentRow(0);
        setScopePanelVisible(m_scopePanelVisible);
        refreshScopeNavigation();
        return;
    }

    bool currentSet{false};
    for(const int index : m_session.activeTrackIndexes()) {
        if(index + 1 >= m_scopeList->count()) {
            continue;
        }

        if(auto* item = m_scopeList->item(index + 1)) {
            item->setSelected(true);
            if(!currentSet) {
                m_scopeList->setCurrentRow(index + 1);
                currentSet = true;
            }
        }
    }

    setScopePanelVisible(m_scopePanelVisible);
    refreshScopeNavigation();
}

void PropertiesDialogWidget::setScopePanelVisible(bool visible)
{
    const bool canShow    = visible && m_session.workingTracks().size() > 1;
    const int targetWidth = scopeHostWidth(canShow);
    const int widthDelta  = targetWidth - m_scopePanelReservedWidth;

    if(widthDelta != 0) {
        m_scopeHost->setFixedWidth(targetWidth);
        m_scopeHost->updateGeometry();
        layout()->activate();
        resize(std::max(minimumWidth(), width() + widthDelta), height());
        m_scopePanelReservedWidth = targetWidth;
    }

    m_scopePanelVisible = canShow;
    m_scopeHost->setVisible(targetWidth > 0);
    m_scopePanel->setVisible(canShow);

    if(m_scopeButton->isChecked() != canShow) {
        const QSignalBlocker blocker{m_scopeButton};
        m_scopeButton->setChecked(canShow);
    }

    if(auto* command = m_actionManager->command(Constants::Actions::TogglePropertiesTrackList)) {
        if(auto* action = command->actionForContext(PropertiesDialogContext)) {
            const QSignalBlocker blocker{action};
            action->setChecked(canShow);
        }
    }
}

void PropertiesDialogWidget::refreshScopeNavigation()
{
    const bool hasMultipleTracks = m_session.workingTracks().size() > 1;
    const int currentRow         = std::max(m_scopeList->currentRow(), 0);
    const int count              = m_scopeList->count();
    const bool canMovePrevious   = hasMultipleTracks && currentRow > 0;
    const bool canMoveNext       = hasMultipleTracks && currentRow >= 0 && currentRow < count - 1;

    if(auto* command = m_actionManager->command(Constants::Actions::TogglePropertiesTrackList)) {
        if(auto* action = command->actionForContext(PropertiesDialogContext)) {
            action->setEnabled(hasMultipleTracks);
            action->setChecked(m_scopePanelVisible);
        }
    }
    if(auto* command = m_actionManager->command(Constants::Actions::PropertiesPreviousTrack)) {
        if(auto* action = command->actionForContext(PropertiesDialogContext)) {
            action->setEnabled(canMovePrevious);
        }
    }
    if(auto* command = m_actionManager->command(Constants::Actions::PropertiesNextTrack)) {
        if(auto* action = command->actionForContext(PropertiesDialogContext)) {
            action->setEnabled(canMoveNext);
        }
    }
}

void PropertiesDialogWidget::switchScope()
{
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

    if(!commitCurrentTabPendingChanges()) {
        const QSignalBlocker blocker{m_scopeList};
        refreshScopePanel();
        return;
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

PropertiesTab* PropertiesDialogWidget::tabForIndex(int index)
{
    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    return tabIt != m_tabs.end() ? &*tabIt : nullptr;
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
    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    if(tabIt == m_tabs.cend()) {
        return nullptr;
    }

    return tabIt->widget(m_session.workingTracks());
}

bool PropertiesDialogWidget::commitPendingChanges(PropertiesTabWidget* widget)
{
    if(!widget || widget->commitPendingChanges()) {
        return true;
    }

    QMessageBox::warning(this, tr("Pending Changes"),
                         tr("Apply or clear the current tab's pending changes before switching tracks or tabs."));
    return false;
}

bool PropertiesDialogWidget::commitCurrentTabPendingChanges()
{
    return commitPendingChanges(currentTabWidget());
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
    return u"%1. %2"_s.arg(trackIndex + 1).arg(track.filenameExt());
}

int PropertiesDialogWidget::scopeHostWidth(bool scopePanelVisible) const
{
    if(m_session.workingTracks().size() <= 1 || !scopePanelVisible) {
        return 0;
    }

    return std::max(m_scopePanel->sizeHint().width(), m_scopePanel->minimumWidth());
}

PropertiesDialog::PropertiesDialog(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_toggleScopeAction{new QAction(tr("Toggle Sidebar"), this)}
    , m_previousTrackAction{new QAction(tr("Previous Track"), this)}
    , m_nextTrackAction{new QAction(tr("Next Track"), this)}
{
    const QStringList category{tr("Tracks"), tr("Properties")};

    m_toggleScopeAction->setCheckable(true);
    m_toggleScopeAction->setIcon(Gui::iconFromTheme(Constants::Icons::SidebarRight));
    m_previousTrackAction->setIcon(Gui::iconFromTheme(Constants::Icons::GoPrevious));
    m_nextTrackAction->setIcon(Gui::iconFromTheme(Constants::Icons::GoNext));
    QObject::connect(m_toggleScopeAction, &QAction::triggered, this, [this]() {
        if(auto* dialog = activePropertiesDialogWidget()) {
            const bool visible = !dialog->scopePanelVisible();
            dialog->setScopePanelVisible(visible);
            m_toggleScopeAction->setChecked(visible);
        }
    });
    auto* toggleCmd = m_actionManager->registerAction(
        m_toggleScopeAction, Constants::Actions::TogglePropertiesTrackList, Context{PropertiesDialogContext});
    toggleCmd->setCategories(category);
    toggleCmd->setDescription(tr("Toggle Sidebar"));
    toggleCmd->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_B});

    QObject::connect(m_previousTrackAction, &QAction::triggered, this, []() {
        if(auto* dialog = activePropertiesDialogWidget()) {
            dialog->moveScope(-1);
        }
    });
    auto* previousCmd = m_actionManager->registerAction(
        m_previousTrackAction, Constants::Actions::PropertiesPreviousTrack, Context{PropertiesDialogContext});
    previousCmd->setCategories(category);
    previousCmd->setDescription(tr("Previous Track"));

    QObject::connect(m_nextTrackAction, &QAction::triggered, this, []() {
        if(auto* dialog = activePropertiesDialogWidget()) {
            dialog->moveScope(1);
        }
    });
    auto* nextCmd = m_actionManager->registerAction(m_nextTrackAction, Constants::Actions::PropertiesNextTrack,
                                                    Context{PropertiesDialogContext});
    nextCmd->setCategories(category);
    nextCmd->setDescription(tr("Next Track"));
}

PropertiesDialog::~PropertiesDialog()
{
    m_tabs.clear();
}

void PropertiesDialog::addTab(const QString& title, const WidgetBuilder& widgetBuilder)
{
    const int index = static_cast<int>(m_tabs.size());
    m_tabs.emplace_back(title, widgetBuilder, index);
}

void PropertiesDialog::addTab(const PropertiesTab& tab)
{
    const int index = static_cast<int>(m_tabs.size());
    PropertiesTab newTab{tab};
    newTab.updateIndex(index);
    m_tabs.emplace_back(tab);
}

void PropertiesDialog::insertTab(int index, const QString& title, const WidgetBuilder& widgetBuilder)
{
    m_tabs.insert(m_tabs.begin() + index, {title, widgetBuilder, index});

    const int count = static_cast<int>(m_tabs.size());
    for(int i{0}; i < count; ++i) {
        m_tabs[i].updateIndex(i);
    }
}

void PropertiesDialog::show(const TrackList& tracks)
{
    auto* dialog = new PropertiesDialogWidget(m_actionManager, tracks, m_tabs);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::finished, dialog, &PropertiesDialogWidget::saveState);

    dialog->show();

    dialog->restoreState();
}
} // namespace Fooyin

#include "gui/moc_propertiesdialog.cpp"
#include "propertiesdialog.moc"
