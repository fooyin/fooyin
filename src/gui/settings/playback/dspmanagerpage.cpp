/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "dspmanagerpage.h"

#include "dsp/dsppresetregistry.h"
#include "dsp/dspsettingsregistry.h"
#include "dspdelegate.h"
#include "dspmodel.h"
#include "widgets/titletooltipgroupbox.h"

#include <core/engine/dsp/dspchainstore.h>
#include <core/engine/dsp/dspnode.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
Engine::DspChain mergeVisibleAndPending(const Engine::DspChain& previousChain, const Engine::DspChain& visibleChain,
                                        const Engine::DspChain& pendingRemoved)
{
    if(pendingRemoved.empty()) {
        return visibleChain;
    }

    Engine::DspChain merged;
    merged.reserve(previousChain.size() + visibleChain.size());

    Engine::DspChain pendingRemaining = pendingRemoved;
    auto visibleIt                    = visibleChain.cbegin();

    for(const auto& previousEntry : previousChain) {
        if(const auto pendingIt = std::ranges::find(pendingRemaining, previousEntry);
           pendingIt != pendingRemaining.end()) {
            merged.push_back(*pendingIt);
            pendingRemaining.erase(pendingIt);
        }
        else if(visibleIt != visibleChain.cend()) {
            merged.push_back(*visibleIt);
            ++visibleIt;
        }
    }

    merged.insert(merged.end(), visibleIt, visibleChain.cend());

    return merged;
}

Engine::DspChain visibleChain(const Engine::DspChain& chain, const Engine::DspChain& pendingRemoved)
{
    if(pendingRemoved.empty()) {
        return chain;
    }

    auto visible = chain;
    for(const auto& removed : pendingRemoved) {
        if(const auto it = std::ranges::find(visible, removed); it != visible.end()) {
            visible.erase(it);
        }
    }

    return visible;
}

void removeEntriesFromChain(Engine::DspChain& chain, const Engine::DspChain& entries)
{
    for(const auto& removed : entries) {
        if(removed.instanceId != 0) {
            if(const auto it = std::ranges::find(chain, removed.instanceId, &Engine::DspDefinition::instanceId);
               it != chain.end()) {
                chain.erase(it);
                continue;
            }
        }

        if(const auto it = std::ranges::find(chain, removed); it != chain.end()) {
            chain.erase(it);
        }
    }
}

bool equalIgnoringEnabled(Engine::DspChains lhs, Engine::DspChains rhs)
{
    const auto clearEnabled = [](Engine::DspChain& chain) {
        for(auto& dsp : chain) {
            dsp.enabled = false;
        }
    };

    clearEnabled(lhs.perTrackChain);
    clearEnabled(lhs.masterChain);
    clearEnabled(rhs.perTrackChain);
    clearEnabled(rhs.masterChain);
    return lhs == rhs;
}

struct LocatedDsp
{
    Engine::DspDefinition dsp;
    Engine::DspChainScope scope;

    bool operator==(const LocatedDsp&) const = default;
};
using LocatedDsps = std::unordered_map<uint64_t, LocatedDsp>;

LocatedDsps locateDsps(const Engine::DspChains& chains)
{
    LocatedDsps located;
    located.reserve(chains.perTrackChain.size() + chains.masterChain.size());

    const auto addChain = [&located](const Engine::DspChain& chain, Engine::DspChainScope scope) {
        for(const auto& dsp : chain) {
            located.insert_or_assign(dsp.instanceId, LocatedDsp{dsp, scope});
        }
    };

    addChain(chains.perTrackChain, Engine::DspChainScope::PerTrack);
    addChain(chains.masterChain, Engine::DspChainScope::Master);
    return located;
}

std::optional<LocatedDsp> findDsp(const LocatedDsps& dsps, uint64_t instanceId)
{
    if(const auto it = dsps.find(instanceId); it != dsps.end()) {
        return it->second;
    }
    return {};
}

void avoidAdditionIdCollisions(const LocatedDsps& baseline, const LocatedDsps& external, Engine::DspChains& draft)
{
    std::unordered_set<uint64_t> usedIds;
    uint64_t maxId{0};

    const auto noteIds = [&usedIds, &maxId](const LocatedDsps& dsps) {
        for(const auto instanceId : dsps | std::views::keys) {
            usedIds.emplace(instanceId);
            maxId = std::max(maxId, instanceId);
        }
    };
    noteIds(baseline);
    noteIds(external);

    const auto nextId = [&usedIds, &maxId]() {
        while(true) {
            maxId = maxId == std::numeric_limits<uint64_t>::max() ? 1 : maxId + 1;
            if(!usedIds.contains(maxId)) {
                usedIds.emplace(maxId);
                return maxId;
            }
        }
    };

    const auto updateChain = [&baseline, &external, &usedIds, &nextId](Engine::DspChain& chain) {
        for(auto& dsp : chain) {
            const bool isLocalAddition = !baseline.contains(dsp.instanceId);
            if(isLocalAddition && external.contains(dsp.instanceId)) {
                dsp.instanceId = nextId();
            }
            else {
                usedIds.emplace(dsp.instanceId);
            }
        }
    };

    updateChain(draft.perTrackChain);
    updateChain(draft.masterChain);
}

Engine::DspChains mergeDspChainsThreeWay(const Engine::DspChains& baseline, const Engine::DspChains& external,
                                         Engine::DspChains draft)
{
    if(draft == baseline) {
        return external;
    }

    const auto baselineDsps = locateDsps(baseline);
    const auto externalDsps = locateDsps(external);

    avoidAdditionIdCollisions(baselineDsps, externalDsps, draft);

    const auto draftDsps = locateDsps(draft);

    std::unordered_map<uint64_t, std::optional<LocatedDsp>> mergedDsps;
    mergedDsps.reserve(baselineDsps.size() + externalDsps.size() + draftDsps.size());

    const auto mergeId = [&](uint64_t instanceId) {
        const auto baselineDsp = findDsp(baselineDsps, instanceId);
        const auto draftDsp    = findDsp(draftDsps, instanceId);
        mergedDsps.insert_or_assign(instanceId, draftDsp == baselineDsp ? findDsp(externalDsps, instanceId) : draftDsp);
    };

    for(const auto instanceId : baselineDsps | std::views::keys) {
        mergeId(instanceId);
    }
    for(const auto instanceId : externalDsps | std::views::keys) {
        if(!mergedDsps.contains(instanceId)) {
            mergeId(instanceId);
        }
    }
    for(const auto instanceId : draftDsps | std::views::keys) {
        if(!mergedDsps.contains(instanceId)) {
            mergeId(instanceId);
        }
    }

    Engine::DspChains merged;
    std::unordered_set<uint64_t> appended;
    appended.reserve(mergedDsps.size());

    const auto appendChain = [&mergedDsps, &appended](const Engine::DspChain& order, Engine::DspChainScope scope,
                                                      Engine::DspChain& target) {
        for(const auto& orderedDsp : order) {
            const auto mergedIt = mergedDsps.find(orderedDsp.instanceId);
            if(mergedIt == mergedDsps.end() || !mergedIt->second || mergedIt->second->scope != scope
               || appended.contains(orderedDsp.instanceId)) {
                continue;
            }

            target.push_back(mergedIt->second->dsp);
            appended.emplace(orderedDsp.instanceId);
        }
    };

    appendChain(draft.perTrackChain, Engine::DspChainScope::PerTrack, merged.perTrackChain);
    appendChain(external.perTrackChain, Engine::DspChainScope::PerTrack, merged.perTrackChain);
    appendChain(draft.masterChain, Engine::DspChainScope::Master, merged.masterChain);
    appendChain(external.masterChain, Engine::DspChainScope::Master, merged.masterChain);

    return merged;
}
} // namespace

class DspManagerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DspManagerPageWidget(DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                                  DspSettingsRegistry* settingsRegistry);

    void load() override;
    void apply() override;
    void finish() override;
    void reset() override;

private:
    struct DspDialogSession
    {
        QPointer<DspSettingsDialog> dialog;
        QByteArray originalSettings;
        QByteArray lastPreviewSettings;
        bool supportsLive{false};
    };

    [[nodiscard]] Engine::DspDefinition resolveDisplayName(Engine::DspDefinition dsp) const;
    void resolveDisplayNames(Engine::DspChain& dsps) const;

    void refreshAvailable();
    void refreshActive();
    void resolveActiveModelDisplayNames(DspModel* model);
    void refreshPresets();
    void updatePresetButtons();
    void syncChainFromActiveList();

    [[nodiscard]] Engine::DspChain& pendingRemovedForModel(DspModel* model);
    [[nodiscard]] const Engine::DspChain& pendingRemovedForModel(const DspModel* model) const;
    [[nodiscard]] uint64_t nextDialogInstanceId() const;
    [[nodiscard]] std::optional<std::pair<DspModel*, Engine::DspChainScope>>
    modelForInstance(uint64_t instanceId) const;
    [[nodiscard]] std::optional<Engine::DspChainScope> liveScopeForInstance(uint64_t instanceId) const;

    void configureActiveDsp(DspModel* model, const QModelIndex& index);
    void finishDspDialog(uint64_t instanceId, int result);
    void closeDspDialog(uint64_t instanceId);
    void closeDspDialogs(bool rollback = true);
    void updateLiveDspSettings(uint64_t instanceId, const QByteArray& settings, QObject* source);
    void syncActiveChain(const Engine::DspChains& chain);
    void syncLiveDspSettings(uint64_t instanceId, const QByteArray& settings, bool persisted, QObject* source);
    void syncDspEnabled(uint64_t instanceId, bool enabled);
    void moveActiveDsp(DspModel* model, const QModelIndex& index, int offset);
    void removeActiveDsp(DspModel* model, const QModelIndex& index);
    void addAvailableDsp(const QModelIndex& index, DspModel* targetModel);

    void showActiveContextMenu(DspModel* model, QAbstractItemView* view, const QPoint& pos);
    void showAvailableContextMenu(const QPoint& pos);

    void loadPreset();
    void savePreset();
    void deletePreset();

    DspChainStore* m_chainStore;
    DspPresetRegistry* m_presetRegistry;
    DspSettingsRegistry* m_settingsRegistry;

    QTableView* m_perTrackList;
    QTableView* m_masterList;
    QListView* m_availableList;

    DspModel* m_perTrackModel;
    DspModel* m_masterModel;
    DspModel* m_availableModel;

    DspDelegate* m_perTrackDelegate;
    DspDelegate* m_masterDelegate;

    QSortFilterProxyModel* m_availableProxy;

    QComboBox* m_presetBox;
    QPushButton* m_loadPreset;
    QPushButton* m_savePreset;
    QPushButton* m_deletePreset;

    Engine::DspChain m_pendingRemovedPerTrack;
    Engine::DspChain m_pendingRemovedMaster;
    Engine::DspChains m_chainBaseline;
    Engine::DspChains m_chain;
    std::unordered_map<uint64_t, DspDialogSession> m_dspDialogs;
    bool m_updating{false};
    bool m_applyingChain{false};
    bool m_changed{false};
};

DspManagerPageWidget::DspManagerPageWidget(DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                                           DspSettingsRegistry* settingsRegistry)
    : m_chainStore{chainStore}
    , m_presetRegistry{presetRegistry}
    , m_settingsRegistry{settingsRegistry}
    , m_perTrackList{new QTableView(this)}
    , m_masterList{new QTableView(this)}
    , m_availableList{new QListView(this)}
    , m_perTrackModel{new DspModel(this)}
    , m_masterModel{new DspModel(this)}
    , m_availableModel{new DspModel(this)}
    , m_perTrackDelegate{new DspDelegate(m_perTrackList, this)}
    , m_masterDelegate{new DspDelegate(m_masterList, this)}
    , m_availableProxy{new QSortFilterProxyModel(this)}
    , m_presetBox{new QComboBox(this)}
    , m_loadPreset{new QPushButton(tr("Load"), this)}
    , m_savePreset{new QPushButton(tr("Save"), this)}
    , m_deletePreset{new QPushButton(tr("Delete"), this)}
{
    m_availableModel->setAllowInternalMoves(false);
    m_availableModel->setCheckable(false);

    m_perTrackList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_perTrackList->setDragDropOverwriteMode(false);
    m_perTrackList->horizontalHeader()->hide();
    m_perTrackList->horizontalHeader()->setStretchLastSection(true);
    m_masterList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_masterList->setDragDropOverwriteMode(false);
    m_masterList->horizontalHeader()->hide();
    m_masterList->horizontalHeader()->setStretchLastSection(true);

    m_availableProxy->setSourceModel(m_availableModel);
    m_availableProxy->setSortRole(Qt::DisplayRole);
    m_availableProxy->sort(0);

    m_perTrackList->setMouseTracking(true);
    m_perTrackList->setModel(m_perTrackModel);
    m_perTrackList->setItemDelegate(m_perTrackDelegate);
    m_masterList->setMouseTracking(true);
    m_masterList->setModel(m_masterModel);
    m_masterList->setItemDelegate(m_masterDelegate);
    m_availableList->setModel(m_availableProxy);

    auto* layout = new QVBoxLayout(this);

    auto* listsLayout   = new QHBoxLayout();
    auto* perTrackGroup = new TitleToolTipGroupBox(tr("Per-Track DSPs"), this);
    auto* masterGroup   = new TitleToolTipGroupBox(tr("Master DSPs"), this);

    perTrackGroup->setTitleToolTip(tr("Per-track DSPs are applied to each stream separately before tracks are mixed.\n"
                                      "During crossfades, each track is processed independently."));
    masterGroup->setTitleToolTip(tr("Master DSPs are applied after all active tracks are mixed into one signal.\n"
                                    "Effects here process the final combined output."));

    auto* availGroup = new QGroupBox(tr("Available DSPs"), this);

    auto* perTrackLayout = new QVBoxLayout(perTrackGroup);
    perTrackLayout->addWidget(m_perTrackList);

    auto* masterLayout = new QVBoxLayout(masterGroup);
    masterLayout->addWidget(m_masterList);

    auto* availLayout = new QVBoxLayout(availGroup);
    availLayout->addWidget(m_availableList);

    auto* activeDSPLayout = new QVBoxLayout();
    activeDSPLayout->addWidget(perTrackGroup);
    activeDSPLayout->addWidget(masterGroup);

    listsLayout->addLayout(activeDSPLayout, 1);
    listsLayout->addWidget(availGroup, 1);
    layout->addLayout(listsLayout);

    auto* presetsGroup  = new QGroupBox(tr("DSP chain presets"), this);
    auto* presetsLayout = new QHBoxLayout(presetsGroup);
    presetsLayout->addWidget(m_presetBox, 1);
    presetsLayout->addWidget(m_loadPreset);
    presetsLayout->addWidget(m_savePreset);
    presetsLayout->addWidget(m_deletePreset);
    layout->addWidget(presetsGroup);

    const auto setupList = [](QAbstractItemView* list, bool acceptDrops = true) {
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setDragEnabled(true);
        list->setAcceptDrops(acceptDrops);
        list->setDropIndicatorShown(acceptDrops);
        list->setDragDropMode(acceptDrops ? QAbstractItemView::DragDrop : QAbstractItemView::DragOnly);
        list->setDefaultDropAction(acceptDrops ? Qt::MoveAction : Qt::CopyAction);
    };

    setupList(m_perTrackList);
    setupList(m_masterList);
    setupList(m_availableList, false);

    m_presetBox->setEditable(true);

    QObject::connect(m_loadPreset, &QPushButton::clicked, this, [this]() { loadPreset(); });
    QObject::connect(m_savePreset, &QPushButton::clicked, this, [this]() { savePreset(); });
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, [this]() { deletePreset(); });
    QObject::connect(m_presetBox, &QComboBox::currentTextChanged, this, [this]() { updatePresetButtons(); });

    QObject::connect(m_perTrackDelegate, &DspDelegate::removeClicked, this,
                     [this](const QModelIndex& index) { removeActiveDsp(m_perTrackModel, index); });
    QObject::connect(m_perTrackDelegate, &DspDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { configureActiveDsp(m_perTrackModel, index); });
    QObject::connect(m_masterDelegate, &DspDelegate::removeClicked, this,
                     [this](const QModelIndex& index) { removeActiveDsp(m_masterModel, index); });
    QObject::connect(m_masterDelegate, &DspDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { configureActiveDsp(m_masterModel, index); });
    QObject::connect(m_perTrackModel, &QAbstractItemModel::rowsInserted, this,
                     [this]() { resolveActiveModelDisplayNames(m_perTrackModel); });
    QObject::connect(m_masterModel, &QAbstractItemModel::rowsInserted, this,
                     [this]() { resolveActiveModelDisplayNames(m_masterModel); });
    QObject::connect(m_perTrackModel, &QAbstractItemModel::dataChanged, this, [this]() {
        if(!m_updating) {
            syncChainFromActiveList();
            refreshAvailable();
        }
    });
    QObject::connect(m_masterModel, &QAbstractItemModel::dataChanged, this, [this]() {
        if(!m_updating) {
            syncChainFromActiveList();
            refreshAvailable();
        }
    });

    m_perTrackList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_masterList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_availableList->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(m_perTrackList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showActiveContextMenu(m_perTrackModel, m_perTrackList, pos); });
    QObject::connect(m_masterList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showActiveContextMenu(m_masterModel, m_masterList, pos); });
    QObject::connect(m_availableList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showAvailableContextMenu(pos); });
    QObject::connect(m_chainStore, &DspChainStore::liveDspSettingsChanged, this,
                     [this](Engine::DspChainScope /*scope*/, uint64_t instanceId, const QByteArray& settings,
                            bool persisted,
                            QObject* source) { syncLiveDspSettings(instanceId, settings, persisted, source); });
    QObject::connect(m_chainStore, &DspChainStore::activeChainChanged, this, &DspManagerPageWidget::syncActiveChain);
    QObject::connect(m_chainStore, &DspChainStore::dspEnabledChanged, this,
                     [this](Engine::DspChainScope /*scope*/, uint64_t instanceId, bool enabled, QObject* /*source*/) {
                         syncDspEnabled(instanceId, enabled);
                     });

    updatePresetButtons();
}

void DspManagerPageWidget::load()
{
    closeDspDialogs();

    m_pendingRemovedPerTrack.clear();
    m_pendingRemovedMaster.clear();
    m_chainBaseline = m_chainStore->activeChain();
    m_chain         = m_chainBaseline;
    m_changed       = false;

    refreshAvailable();
    refreshActive();
    refreshPresets();
}

void DspManagerPageWidget::apply()
{
    syncChainFromActiveList();

    const bool hasPendingRemoved = !m_pendingRemovedPerTrack.empty() || !m_pendingRemovedMaster.empty();

    bool applied{false};

    if(m_changed) {
        m_applyingChain = true;
        m_chainStore->setActiveChain(m_chain);
        m_applyingChain = false;
        // Retrieve the normalised chain so live-setting updates can target newly added DSP instances
        m_chain         = m_chainStore->activeChain();
        m_chainBaseline = m_chain;
        applied         = true;
    }

    if(hasPendingRemoved) {
        auto finalChain = m_chain;
        removeEntriesFromChain(finalChain.perTrackChain, m_pendingRemovedPerTrack);
        removeEntriesFromChain(finalChain.masterChain, m_pendingRemovedMaster);

        m_pendingRemovedPerTrack.clear();
        m_pendingRemovedMaster.clear();

        if(finalChain != m_chain || !applied) {
            m_applyingChain = true;
            m_chainStore->setActiveChain(finalChain);
            m_applyingChain = false;
            m_chain         = m_chainStore->activeChain();
            m_chainBaseline = m_chain;
        }

        m_changed = false;
        refreshActive();
    }
    else if(applied) {
        m_changed = false;
        refreshActive();
    }
}

void DspManagerPageWidget::finish()
{
    closeDspDialogs();
}

void DspManagerPageWidget::reset()
{
    closeDspDialogs();

    m_pendingRemovedPerTrack.clear();
    m_pendingRemovedMaster.clear();
    m_chain.clear();
    refreshActive();
}

void DspManagerPageWidget::refreshAvailable()
{
    auto dsps = m_chainStore->availableDsps();
    for(auto& dsp : dsps) {
        dsp.hasSettings = m_settingsRegistry && m_settingsRegistry->hasProvider(dsp.id);
    }
    m_availableModel->setup(dsps);
}

void DspManagerPageWidget::refreshActive()
{
    m_updating = true;

    auto perTrack = visibleChain(m_chain.perTrackChain, m_pendingRemovedPerTrack);
    auto master   = visibleChain(m_chain.masterChain, m_pendingRemovedMaster);
    resolveDisplayNames(perTrack);
    resolveDisplayNames(master);

    m_perTrackModel->setup(perTrack);
    m_masterModel->setup(master);

    m_updating = false;

    updatePresetButtons();
}

void DspManagerPageWidget::resolveActiveModelDisplayNames(DspModel* model)
{
    if(m_updating || !model) {
        return;
    }

    auto dsps = model->dsps();
    for(auto& dsp : dsps) {
        if(dsp.instanceId == 0) {
            dsp.instanceId = nextDialogInstanceId();
        }
    }
    resolveDisplayNames(dsps);

    m_updating = true;
    model->setup(dsps);
    m_updating = false;

    syncChainFromActiveList();
    refreshAvailable();
}

void DspManagerPageWidget::refreshPresets()
{
    const QString currentText = m_presetBox->currentText();

    m_presetBox->clear();

    const auto presets = m_presetRegistry->presetsByName();
    for(const auto& preset : presets) {
        m_presetBox->addItem(preset.name, preset.id);
    }

    if(!currentText.isEmpty()) {
        const int idx = m_presetBox->findText(currentText);
        if(idx >= 0) {
            m_presetBox->setCurrentIndex(idx);
        }
        else {
            m_presetBox->setEditText(currentText);
        }
    }

    updatePresetButtons();
}

void DspManagerPageWidget::updatePresetButtons()
{
    const QString name = m_presetBox->currentText().trimmed();
    const int index    = m_presetBox->findText(name);
    const bool canLoad = index >= 0;
    bool canDelete{false};

    if(canLoad) {
        const int id = m_presetBox->itemData(index).toInt();
        if(const auto presetOpt = m_presetRegistry->itemById(id)) {
            canDelete = !presetOpt->isDefault;
        }
    }

    m_loadPreset->setEnabled(canLoad);
    m_deletePreset->setEnabled(canDelete);
    m_savePreset->setEnabled(!name.isEmpty());
}

void DspManagerPageWidget::configureActiveDsp(DspModel* model, const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    const auto dsps = model->dsps();
    const int row   = index.row();
    if(row < 0 || std::cmp_greater_equal(row, dsps.size())) {
        return;
    }

    const auto& dspEntry      = dsps.at(row);
    const QString dspId       = dspEntry.id;
    const uint64_t instanceId = dspEntry.instanceId;
    if(instanceId == 0) {
        return;
    }

    if(const auto session = m_dspDialogs.find(instanceId); session != m_dspDialogs.end() && session->second.dialog) {
        session->second.dialog->show();
        session->second.dialog->raise();
        session->second.dialog->activateWindow();
        return;
    }

    auto* provider = m_settingsRegistry ? m_settingsRegistry->providerFor(dspId) : nullptr;
    if(!provider) {
        QMessageBox::information(this, tr("DSP Settings"), tr("This DSP has no configurable settings."));
        return;
    }

    auto* settingsDialog = provider->createSettingsWidget(this);
    if(!settingsDialog) {
        QMessageBox::warning(this, tr("DSP Settings"),
                             tr("Unable to open settings for DSP \"%1\".").arg(dspEntry.name));
        return;
    }
    settingsDialog->setWindowTitle(dspEntry.name);
    settingsDialog->loadSettings(dspEntry.settings);

    bool supportsLive{false};
    if(auto node = m_chainStore->createDsp(dspId)) {
        supportsLive = node->supportsLiveSettings();
    }

    m_dspDialogs.insert_or_assign(instanceId, DspDialogSession{
                                                  .dialog              = settingsDialog,
                                                  .originalSettings    = dspEntry.settings,
                                                  .lastPreviewSettings = dspEntry.settings,
                                                  .supportsLive        = supportsLive,
                                              });

    QObject::connect(settingsDialog, &DspSettingsDialog::previewSettingsChanged, this,
                     [this, instanceId, settingsDialog](const QByteArray& settings) {
                         const auto session = m_dspDialogs.find(instanceId);
                         if(session == m_dspDialogs.end()) {
                             return;
                         }

                         session->second.lastPreviewSettings = settings;
                         if(session->second.supportsLive) {
                             updateLiveDspSettings(instanceId, settings, settingsDialog);
                         }
                     });
    QObject::connect(settingsDialog, &QDialog::finished, this,
                     [this, instanceId](int result) { finishDspDialog(instanceId, result); });
    QObject::connect(settingsDialog, &QObject::destroyed, this,
                     [this, instanceId]() { m_dspDialogs.erase(instanceId); });

    settingsDialog->setWindowModality(Qt::NonModal);
    settingsDialog->show();
}

void DspManagerPageWidget::finishDspDialog(uint64_t instanceId, int result)
{
    const auto sessionIt = m_dspDialogs.find(instanceId);
    if(sessionIt == m_dspDialogs.end()) {
        return;
    }

    const DspDialogSession session = sessionIt->second;

    if(result == QDialog::Accepted && session.dialog) {
        if(const auto target = modelForInstance(instanceId)) {
            auto* model      = target->first;
            auto dsps        = model->dsps();
            const auto dspIt = std::ranges::find(dsps, instanceId, &Engine::DspDefinition::instanceId);

            if(dspIt != dsps.end()) {
                dspIt->settings = session.dialog->saveSettings();
                *dspIt          = resolveDisplayName(std::move(*dspIt));
                model->setup(dsps);
                syncChainFromActiveList();
                refreshAvailable();

                if(session.supportsLive && dspIt->settings != session.lastPreviewSettings) {
                    updateLiveDspSettings(instanceId, dspIt->settings, session.dialog);
                }
            }
        }
    }
    else if(session.supportsLive && session.lastPreviewSettings != session.originalSettings) {
        updateLiveDspSettings(instanceId, session.originalSettings, session.dialog);
    }

    m_dspDialogs.erase(sessionIt);
    if(session.dialog) {
        session.dialog->deleteLater();
    }
}

void DspManagerPageWidget::closeDspDialog(uint64_t instanceId)
{
    if(const auto session = m_dspDialogs.find(instanceId); session != m_dspDialogs.end() && session->second.dialog) {
        session->second.dialog->reject();
    }
}

void DspManagerPageWidget::closeDspDialogs(bool rollback)
{
    std::vector<QPointer<DspSettingsDialog>> dialogs;
    dialogs.reserve(m_dspDialogs.size());

    for(auto& session : m_dspDialogs | std::views::values) {
        if(!rollback) {
            session.originalSettings = session.lastPreviewSettings;
        }
        dialogs.push_back(session.dialog);
    }

    for(const auto& dialog : dialogs) {
        if(dialog) {
            dialog->reject();
        }
    }
}

void DspManagerPageWidget::updateLiveDspSettings(uint64_t instanceId, const QByteArray& settings, QObject* source)
{
    if(const auto scope = liveScopeForInstance(instanceId)) {
        m_chainStore->updateLiveDspSettings(*scope, instanceId, settings, false, source);
    }
}

void DspManagerPageWidget::syncActiveChain(const Engine::DspChains& chain)
{
    if(m_applyingChain) {
        return;
    }

    syncChainFromActiveList();

    auto draft{m_chain};
    removeEntriesFromChain(draft.perTrackChain, m_pendingRemovedPerTrack);
    removeEntriesFromChain(draft.masterChain, m_pendingRemovedMaster);

    if(!equalIgnoringEnabled(m_chainBaseline, chain)) {
        closeDspDialogs(false);
    }

    const auto baseline = std::exchange(m_chainBaseline, chain);
    m_chain             = mergeDspChainsThreeWay(baseline, chain, std::move(draft));
    m_pendingRemovedPerTrack.clear();
    m_pendingRemovedMaster.clear();
    m_changed = m_chain != chain;

    refreshActive();
    refreshAvailable();
}

void DspManagerPageWidget::syncLiveDspSettings(uint64_t instanceId, const QByteArray& settings, bool persisted,
                                               QObject* source)
{
    if(persisted) {
        const auto updateChain = [instanceId, &settings](Engine::DspChains& chains) {
            for(auto* chain : {&chains.perTrackChain, &chains.masterChain}) {
                if(const auto it = std::ranges::find(*chain, instanceId, &Engine::DspDefinition::instanceId);
                   it != chain->end()) {
                    it->settings = settings;
                    break;
                }
            }
        };
        updateChain(m_chainBaseline);
        updateChain(m_chain);
    }

    const auto target = modelForInstance(instanceId);
    if(!target) {
        return;
    }

    auto* model      = target->first;
    auto dsps        = model->dsps();
    const auto dspIt = std::ranges::find(dsps, instanceId, &Engine::DspDefinition::instanceId);
    if(dspIt != dsps.end()) {
        auto displayDsp     = *dspIt;
        displayDsp.settings = settings;
        displayDsp          = resolveDisplayName(std::move(displayDsp));
        dspIt->name         = displayDsp.name;

        if(persisted) {
            dspIt->settings = settings;
        }

        m_updating = true;
        model->setup(dsps);
        m_updating = false;

        if(persisted) {
            auto& chain = target->second == Engine::DspChainScope::Master ? m_chain.masterChain : m_chain.perTrackChain;
            if(const auto chainIt = std::ranges::find(chain, instanceId, &Engine::DspDefinition::instanceId);
               chainIt != chain.end()) {
                chainIt->settings = settings;
                chainIt->name     = displayDsp.name;
            }
        }
    }

    const auto session = m_dspDialogs.find(instanceId);
    if(session == m_dspDialogs.end() || !session->second.dialog || source == session->second.dialog) {
        return;
    }

    session->second.lastPreviewSettings = settings;
    if(persisted) {
        session->second.originalSettings = settings;
    }
    session->second.dialog->loadSettings(settings);
}

void DspManagerPageWidget::syncDspEnabled(uint64_t instanceId, bool enabled)
{
    const auto target = modelForInstance(instanceId);
    if(!target) {
        return;
    }

    auto* model      = target->first;
    auto dsps        = model->dsps();
    const auto dspIt = std::ranges::find(dsps, instanceId, &Engine::DspDefinition::instanceId);
    if(dspIt != dsps.end()) {
        dspIt->enabled = enabled;

        m_updating = true;
        model->setup(dsps);
        m_updating = false;
    }

    auto& chain = target->second == Engine::DspChainScope::Master ? m_chain.masterChain : m_chain.perTrackChain;
    if(const auto chainIt = std::ranges::find(chain, instanceId, &Engine::DspDefinition::instanceId);
       chainIt != chain.end()) {
        chainIt->enabled = enabled;
    }
}

void DspManagerPageWidget::moveActiveDsp(DspModel* model, const QModelIndex& index, int offset)
{
    if(!model || !index.isValid() || offset == 0) {
        return;
    }

    auto dsps           = model->dsps();
    const int row       = index.row();
    const int targetRow = row + offset;

    if(row < 0 || targetRow < 0 || row >= static_cast<int>(dsps.size()) || targetRow >= static_cast<int>(dsps.size())) {
        return;
    }

    std::iter_swap(dsps.begin() + row, dsps.begin() + targetRow);
    model->setup(dsps);

    syncChainFromActiveList();
    refreshAvailable();
}

void DspManagerPageWidget::removeActiveDsp(DspModel* model, const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    auto dsps     = model->dsps();
    const int row = index.row();

    if(row < 0 || std::cmp_greater_equal(row, dsps.size())) {
        return;
    }

    auto entry    = dsps[static_cast<size_t>(row)];
    entry.enabled = false;

    closeDspDialog(entry.instanceId);

    auto& pendingRemoved = pendingRemovedForModel(model);
    if(std::ranges::find(pendingRemoved, entry) == pendingRemoved.end()) {
        pendingRemoved.push_back(entry);
    }

    model->removeRows(row, 1, {});

    syncChainFromActiveList();
    refreshAvailable();
}

void DspManagerPageWidget::addAvailableDsp(const QModelIndex& index, DspModel* targetModel)
{
    if(!index.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_availableProxy->mapToSource(index);

    auto dsp = sourceIndex.data(DspModel::Dsp).value<Engine::DspDefinition>();
    dsp      = resolveDisplayName(std::move(dsp));

    if(dsp.instanceId == 0) {
        dsp.instanceId = nextDialogInstanceId();
    }

    dsp.enabled = true;

    auto dsps = targetModel->dsps();
    dsps.push_back(dsp);
    targetModel->setup(dsps);

    syncChainFromActiveList();
    refreshAvailable();
}

void DspManagerPageWidget::showActiveContextMenu(DspModel* model, QAbstractItemView* view, const QPoint& pos)
{
    const QModelIndex index = view->indexAt(pos);
    if(!index.isValid()) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(index.data(DspModel::HasSettings).toBool()) {
        auto* configureAction = new QAction(tr("Configure…"), menu);
        menu->addAction(configureAction);
        QObject::connect(configureAction, &QAction::triggered, this,
                         [this, model, index]() { configureActiveDsp(model, index); });

        menu->addSeparator();
    }

    auto* moveUpAction = new QAction(tr("Move Up"), menu);
    moveUpAction->setEnabled(index.row() > 0);
    menu->addAction(moveUpAction);
    QObject::connect(moveUpAction, &QAction::triggered, this,
                     [this, model, index]() { moveActiveDsp(model, index, -1); });

    auto* moveDownAction = new QAction(tr("Move Down"), menu);
    moveDownAction->setEnabled(index.row() < model->rowCount({}) - 1);
    menu->addAction(moveDownAction);
    QObject::connect(moveDownAction, &QAction::triggered, this,
                     [this, model, index]() { moveActiveDsp(model, index, 1); });

    menu->addSeparator();

    const bool enabled        = index.data(DspModel::Enabled).toBool();
    auto* toggleEnabledAction = new QAction(enabled ? tr("Disable") : tr("Enable"), menu);
    menu->addAction(toggleEnabledAction);
    QObject::connect(toggleEnabledAction, &QAction::triggered, this, [this, model, index, enabled]() {
        model->setData(index, !enabled, DspModel::Enabled);
        syncChainFromActiveList();
        refreshAvailable();
    });

    auto* removeAction = new QAction(tr("Remove"), menu);
    menu->addAction(removeAction);
    QObject::connect(removeAction, &QAction::triggered, this,
                     [this, model, index]() { removeActiveDsp(model, index); });

    menu->popup(view->viewport()->mapToGlobal(pos));
}

void DspManagerPageWidget::showAvailableContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_availableList->indexAt(pos);
    if(!index.isValid()) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* addPerTrackAction = new QAction(tr("Add to Per-Track"), menu);
    auto* addMasterAction   = new QAction(tr("Add to Master"), menu);
    menu->addAction(addPerTrackAction);
    menu->addAction(addMasterAction);

    QObject::connect(addPerTrackAction, &QAction::triggered, this,
                     [this, index]() { addAvailableDsp(index, m_perTrackModel); });
    QObject::connect(addMasterAction, &QAction::triggered, this,
                     [this, index]() { addAvailableDsp(index, m_masterModel); });

    menu->popup(m_availableList->viewport()->mapToGlobal(pos));
}

void DspManagerPageWidget::syncChainFromActiveList()
{
    const auto mergedPerTrack
        = mergeVisibleAndPending(m_chain.perTrackChain, m_perTrackModel->dsps(), m_pendingRemovedPerTrack);
    if(std::exchange(m_chain.perTrackChain, mergedPerTrack) != m_chain.perTrackChain) {
        m_changed = true;
    }

    const auto mergedMaster
        = mergeVisibleAndPending(m_chain.masterChain, m_masterModel->dsps(), m_pendingRemovedMaster);
    if(std::exchange(m_chain.masterChain, mergedMaster) != m_chain.masterChain) {
        m_changed = true;
    }
}

Engine::DspChain& DspManagerPageWidget::pendingRemovedForModel(DspModel* model)
{
    return (model == m_masterModel) ? m_pendingRemovedMaster : m_pendingRemovedPerTrack;
}

const Engine::DspChain& DspManagerPageWidget::pendingRemovedForModel(const DspModel* model) const
{
    return (model == m_masterModel) ? m_pendingRemovedMaster : m_pendingRemovedPerTrack;
}

uint64_t DspManagerPageWidget::nextDialogInstanceId() const
{
    uint64_t maxId{0};

    const auto scanChain = [&maxId](const Engine::DspChain& chain) {
        for(const auto& entry : chain) {
            maxId = std::max(maxId, entry.instanceId);
        }
    };

    scanChain(m_chain.perTrackChain);
    scanChain(m_chain.masterChain);
    scanChain(m_perTrackModel->dsps());
    scanChain(m_masterModel->dsps());
    scanChain(m_pendingRemovedPerTrack);
    scanChain(m_pendingRemovedMaster);

    if(maxId >= std::numeric_limits<uint64_t>::max() - 1) {
        return 1;
    }

    return maxId + 1;
}

std::optional<std::pair<DspModel*, Engine::DspChainScope>>
DspManagerPageWidget::modelForInstance(uint64_t instanceId) const
{
    const auto containsInstance = [instanceId](const DspModel* model) {
        const auto dsps = model->dsps();
        return std::ranges::find(dsps, instanceId, &Engine::DspDefinition::instanceId) != dsps.end();
    };

    if(containsInstance(m_masterModel)) {
        return std::pair{m_masterModel, Engine::DspChainScope::Master};
    }
    if(containsInstance(m_perTrackModel)) {
        return std::pair{m_perTrackModel, Engine::DspChainScope::PerTrack};
    }
    return {};
}

std::optional<Engine::DspChainScope> DspManagerPageWidget::liveScopeForInstance(uint64_t instanceId) const
{
    const auto chain            = m_chainStore->activeChain();
    const auto containsInstance = [instanceId](const Engine::DspChain& dsps) {
        return std::ranges::find(dsps, instanceId, &Engine::DspDefinition::instanceId) != dsps.end();
    };

    if(containsInstance(chain.masterChain)) {
        return Engine::DspChainScope::Master;
    }
    if(containsInstance(chain.perTrackChain)) {
        return Engine::DspChainScope::PerTrack;
    }
    return {};
}

Engine::DspDefinition DspManagerPageWidget::resolveDisplayName(Engine::DspDefinition dsp) const
{
    dsp.hasSettings = m_settingsRegistry && m_settingsRegistry->hasProvider(dsp.id);

    auto node = m_chainStore->createDsp(dsp.id);
    if(node && !dsp.settings.isEmpty()) {
        node->loadSettings(dsp.settings);
    }

    if(node) {
        dsp.name = node->name();
    }
    return dsp;
}

void DspManagerPageWidget::resolveDisplayNames(Engine::DspChain& dsps) const
{
    for(auto& dsp : dsps) {
        dsp = resolveDisplayName(std::move(dsp));
    }
}

void DspManagerPageWidget::loadPreset()
{
    const QString name = m_presetBox->currentText().trimmed();
    if(name.isEmpty()) {
        return;
    }

    const int index = m_presetBox->findText(name);
    if(index < 0) {
        return;
    }

    const int id         = m_presetBox->itemData(index).toInt();
    const auto presetOpt = m_presetRegistry->itemById(id);
    if(!presetOpt) {
        return;
    }

    closeDspDialogs();

    if(std::exchange(m_chain, presetOpt->chain) != m_chain) {
        m_changed = true;
    }

    m_pendingRemovedPerTrack.clear();
    m_pendingRemovedMaster.clear();
    refreshActive();
    refreshAvailable();
    m_presetBox->setCurrentIndex(index);
}

void DspManagerPageWidget::savePreset()
{
    syncChainFromActiveList();

    auto presetChain{m_chain};
    removeEntriesFromChain(presetChain.perTrackChain, m_pendingRemovedPerTrack);
    removeEntriesFromChain(presetChain.masterChain, m_pendingRemovedMaster);

    const QString name = m_presetBox->currentText().trimmed();
    if(name.isEmpty()) {
        return;
    }

    auto existing = m_presetRegistry->itemByName(name);
    if(existing) {
        QMessageBox msg{QMessageBox::Question, tr("Preset already exists"),
                        tr("Preset \"%1\" already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() != QMessageBox::Yes) {
            return;
        }

        DspChainPreset preset = existing.value();

        preset.chain = presetChain;

        m_presetRegistry->changeItem(preset);

        refreshPresets();
        m_presetBox->setCurrentText(name);
        return;
    }

    DspChainPreset preset;
    preset.name  = name;
    preset.chain = presetChain;

    m_presetRegistry->addItem(preset);
    refreshPresets();
    m_presetBox->setCurrentText(name);
}

void DspManagerPageWidget::deletePreset()
{
    const QString name = m_presetBox->currentText().trimmed();
    if(name.isEmpty()) {
        return;
    }

    const int index = m_presetBox->findText(name);
    if(index < 0) {
        return;
    }

    const int id         = m_presetBox->itemData(index).toInt();
    const auto presetOpt = m_presetRegistry->itemById(id);
    if(!presetOpt || presetOpt->isDefault) {
        return;
    }

    m_presetRegistry->removeById(id);
    refreshPresets();
    updatePresetButtons();
}

DspManagerPage::DspManagerPage(DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                               DspSettingsRegistry* settingsRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::DspManager);
    setCategory({tr("Playback"), tr("DSP Manager")});
    setWidgetCreator([chainStore, presetRegistry, settingsRegistry]() {
        return new DspManagerPageWidget(chainStore, presetRegistry, settingsRegistry);
    });
}
} // namespace Fooyin

#include "dspmanagerpage.moc"
#include "moc_dspmanagerpage.cpp"
