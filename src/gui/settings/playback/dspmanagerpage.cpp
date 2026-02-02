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

#include <core/engine/dsp/dspchainstore.h>
#include <core/engine/dsp/dspnode.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
class DspManagerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DspManagerPageWidget(DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                                  DspSettingsRegistry* settingsRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    [[nodiscard]] Engine::DspDefinition resolveDisplayName(Engine::DspDefinition dsp) const;
    void resolveDisplayNames(Engine::DspChain& dsps) const;

    void refreshAvailable();
    void refreshActive();
    void resolveActiveModelDisplayNames(DspModel* model);
    void refreshPresets();
    void updatePresetButtons();
    void syncChainFromActiveList();

    void configureActiveDsp(DspModel* model, const QModelIndex& index);
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

    Engine::DspChains m_chain;
    bool m_updating{false};
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
    auto* perTrackGroup = new QGroupBox(tr("Per-Track DSPs"), this);
    auto* masterGroup   = new QGroupBox(tr("Master DSPs"), this);

    perTrackGroup->setToolTip(tr("Per-track DSPs are applied to each stream separately before tracks are mixed. "
                                 "During crossfades, each track is processed independently."));
    masterGroup->setToolTip(tr("Master DSPs are applied after all active tracks are mixed into one signal. "
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

    QObject::connect(m_perTrackDelegate, &DspDelegate::removeClicked, this, [this](const QModelIndex& index) {
        m_perTrackModel->removeRows(index.row(), 1, {});
        syncChainFromActiveList();
        refreshAvailable();
    });
    QObject::connect(m_perTrackDelegate, &DspDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { configureActiveDsp(m_perTrackModel, index); });
    QObject::connect(m_masterDelegate, &DspDelegate::removeClicked, this, [this](const QModelIndex& index) {
        m_masterModel->removeRows(index.row(), 1, {});
        syncChainFromActiveList();
        refreshAvailable();
    });
    QObject::connect(m_masterDelegate, &DspDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { configureActiveDsp(m_masterModel, index); });
    QObject::connect(m_perTrackModel, &QAbstractItemModel::rowsInserted, this,
                     [this](const QModelIndex&, int, int) { resolveActiveModelDisplayNames(m_perTrackModel); });
    QObject::connect(m_masterModel, &QAbstractItemModel::rowsInserted, this,
                     [this](const QModelIndex&, int, int) { resolveActiveModelDisplayNames(m_masterModel); });

    m_perTrackList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_masterList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_availableList->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(m_perTrackList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showActiveContextMenu(m_perTrackModel, m_perTrackList, pos); });
    QObject::connect(m_masterList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showActiveContextMenu(m_masterModel, m_masterList, pos); });
    QObject::connect(m_availableList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showAvailableContextMenu(pos); });

    updatePresetButtons();
}

void DspManagerPageWidget::load()
{
    m_chain = m_chainStore->activeChain();

    refreshAvailable();
    refreshActive();
    refreshPresets();
}

void DspManagerPageWidget::apply()
{
    syncChainFromActiveList();

    if(m_changed) {
        m_chainStore->setActiveChain(m_chain);
    }
}

void DspManagerPageWidget::reset()
{
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

    auto perTrack = m_chain.perTrackChain;
    auto master   = m_chain.masterChain;
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
    resolveDisplayNames(dsps);

    m_updating = true;
    model->setup(dsps);
    m_updating = false;
}

void DspManagerPageWidget::refreshPresets()
{
    const QString currentText = m_presetBox->currentText();

    m_presetBox->clear();

    const auto presets = m_presetRegistry->items();
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

    const QString dspId   = index.data(DspModel::Id).toString();
    const QString dspName = index.data(Qt::DisplayRole).toString();

    auto dsps      = model->dsps();
    const int row  = index.row();
    auto& dspEntry = dsps.at(row);

    auto* provider = m_settingsRegistry ? m_settingsRegistry->providerFor(dspId) : nullptr;
    if(!provider) {
        QMessageBox::information(this, tr("DSP Settings"), tr("This DSP has no configurable settings."));
        return;
    }

    auto settingsDialog = std::unique_ptr<DspSettingsDialog>{provider->createSettingsWidget(this)};
    if(!settingsDialog) {
        QMessageBox::warning(this, tr("DSP Settings"), tr("Unable to open settings for DSP \"%1\".").arg(dspName));
        return;
    }
    settingsDialog->setWindowTitle(dspName);
    settingsDialog->loadSettings(dspEntry.settings);

    if(settingsDialog->exec() != QDialog::Accepted) {
        return;
    }

    dsps[row].settings = settingsDialog->saveSettings();
    dsps[row]          = resolveDisplayName(std::move(dsps[row]));
    model->setup(dsps);
}

void DspManagerPageWidget::removeActiveDsp(DspModel* model, const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    model->removeRows(index.row(), 1, {});
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
        QObject::connect(configureAction, &QAction::triggered, this,
                         [this, model, index]() { configureActiveDsp(model, index); });
    }

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

    QObject::connect(addPerTrackAction, &QAction::triggered, this,
                     [this, index]() { addAvailableDsp(index, m_perTrackModel); });
    QObject::connect(addMasterAction, &QAction::triggered, this,
                     [this, index]() { addAvailableDsp(index, m_masterModel); });

    menu->popup(m_availableList->viewport()->mapToGlobal(pos));
}

void DspManagerPageWidget::syncChainFromActiveList()
{
    if(std::exchange(m_chain.perTrackChain, m_perTrackModel->dsps()) != m_chain.perTrackChain) {
        m_changed = true;
    }
    if(std::exchange(m_chain.masterChain, m_masterModel->dsps()) != m_chain.masterChain) {
        m_changed = true;
    }
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

    if(std::exchange(m_chain, presetOpt->chain) != m_chain) {
        m_changed = true;
    }

    refreshActive();
    refreshAvailable();
    m_presetBox->setCurrentIndex(index);
}

void DspManagerPageWidget::savePreset()
{
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

        preset.chain = m_chain;

        m_presetRegistry->changeItem(preset);

        refreshPresets();
        m_presetBox->setCurrentText(name);
        return;
    }

    DspChainPreset preset;
    preset.name  = name;
    preset.chain = m_chain;

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
