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

#include "dspsettingscontroller.h"

#include "dspsettingslayoutwidget.h"
#include "dspsettingsregistry.h"

#include <core/engine/dsp/dspchainstore.h>
#include <core/engine/dsp/dspnode.h>
#include <gui/dsp/dspsettingsdialog.h>
#include <gui/dsp/dspsettingsprovider.h>

#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QPointer>
#include <QSignalBlocker>

#include <algorithm>
#include <functional>
#include <unordered_map>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QCheckBox* addEnabledToggle(DspSettingsDialog* editor, bool enabled)
{
    auto* toggle = new QCheckBox(DspSettingsController::tr("Enabled"), editor);
    toggle->setChecked(enabled);
    editor->addButtonRowWidget(toggle);

    return toggle;
}

QComboBox* addInstanceSelector(DspSettingsDialog* editor)
{
    auto* selector = new QComboBox(editor);
    selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    editor->addButtonRowWidget(selector);
    return selector;
}

QString scopeLabel(const Engine::DspChainScope scope)
{
    return scope == Engine::DspChainScope::Master ? DspSettingsController::tr("Master")
                                                  : DspSettingsController::tr("Per-track");
}

class DspSettingsDialogSession : public QObject
{
    Q_OBJECT

public:
    DspSettingsDialogSession(DspSettingsController* controller, QString dspId, DspSettingsDialog* editor,
                             std::vector<DspSettingsController::Target> targets, std::function<void()> rollback);

    void open();
    void finish(int result);

private:
    struct PendingState
    {
        QByteArray settings;
        bool enabled{true};
    };

    void setupControls();
    void populateSelector(const std::vector<DspSettingsController::Target>& targets);
    void refreshInstances();

    PendingState& pendingFor(const DspSettingsController::Target& target);

    void saveCurrent();
    void selectIndex(int index);
    void loadTarget(uint64_t instanceId);

    void commit();

    DspSettingsController* m_controller;
    QString m_dspId;
    QPointer<DspSettingsDialog> m_editor;
    std::vector<DspSettingsController::Target> m_targets;
    std::function<void()> m_rollback;

    QComboBox* m_instanceSelector;
    QCheckBox* m_enabledToggle;

    std::unordered_map<uint64_t, PendingState> m_pending;
    uint64_t m_currentInstanceId{0};

    QMetaObject::Connection m_previewConnection;
    QMetaObject::Connection m_enabledConnection;
};

DspSettingsDialogSession::DspSettingsDialogSession(DspSettingsController* controller, QString dspId,
                                                   DspSettingsDialog* editor,
                                                   std::vector<DspSettingsController::Target> targets,
                                                   std::function<void()> rollback)
    : QObject{controller}
    , m_controller{controller}
    , m_dspId{std::move(dspId)}
    , m_editor{editor}
    , m_targets{std::move(targets)}
    , m_rollback{std::move(rollback)}
    , m_instanceSelector{addInstanceSelector(m_editor)}
    , m_enabledToggle{addEnabledToggle(m_editor, m_targets.front().enabled)}
{
    QObject::connect(m_editor, &QObject::destroyed, this, &QObject::deleteLater);
}

void DspSettingsDialogSession::open()
{
    setupControls();
    loadTarget(m_targets.front().instanceId);

    QObject::connect(m_editor, &QDialog::finished, this, &DspSettingsDialogSession::finish);
    m_editor->open();
}

void DspSettingsDialogSession::finish(int result)
{
    if(result != QDialog::Accepted) {
        m_rollback();
    }
    else {
        commit();
    }

    m_editor->deleteLater();
}

void DspSettingsDialogSession::setupControls()
{
    m_editor->setWindowTitle(m_controller->displayName(m_dspId));
    populateSelector(m_targets);

    QObject::connect(m_instanceSelector, &QComboBox::currentIndexChanged, this, &DspSettingsDialogSession::selectIndex);
    QObject::connect(m_controller, &DspSettingsController::dspInstancesChanged, this,
                     &DspSettingsDialogSession::refreshInstances);
}

void DspSettingsDialogSession::populateSelector(const std::vector<DspSettingsController::Target>& targets)
{
    const QSignalBlocker blocker{m_instanceSelector};
    m_instanceSelector->clear();

    for(const auto& target : targets) {
        m_instanceSelector->addItem(target.label, QString::number(target.instanceId));
    }

    m_instanceSelector->setVisible(targets.size() > 1);

    if(m_currentInstanceId != 0) {
        const int index = m_instanceSelector->findData(QString::number(m_currentInstanceId));
        if(index >= 0) {
            m_instanceSelector->setCurrentIndex(index);
        }
    }
}

void DspSettingsDialogSession::refreshInstances()
{
    const auto targets = m_controller->targetsFor(m_dspId);

    const bool currentExists
        = m_currentInstanceId == 0 || std::ranges::any_of(targets, [this](const DspSettingsController::Target& target) {
              return target.instanceId == m_currentInstanceId;
          });

    if(!currentExists) {
        m_editor->reject();
        return;
    }

    populateSelector(targets);
}

void DspSettingsDialogSession::saveCurrent()
{
    if(m_currentInstanceId != 0 && m_editor) {
        m_pending[m_currentInstanceId].settings = m_editor->saveSettings();
    }
}

void DspSettingsDialogSession::selectIndex(const int index)
{
    if(index >= 0) {
        loadTarget(m_instanceSelector->itemData(index).toString().toULongLong());
    }
}

void DspSettingsDialogSession::loadTarget(const uint64_t instanceId)
{
    const auto target = m_controller->targetForInstance(m_dspId, instanceId);
    if(!target) {
        return;
    }

    if(m_currentInstanceId != 0 && m_currentInstanceId != instanceId) {
        saveCurrent();
    }

    m_currentInstanceId = instanceId;
    auto& pendingState  = pendingFor(*target);

    const int selectorIndex = m_instanceSelector->findData(QString::number(instanceId));
    if(selectorIndex >= 0 && m_instanceSelector->currentIndex() != selectorIndex) {
        const QSignalBlocker blocker{m_instanceSelector};
        m_instanceSelector->setCurrentIndex(selectorIndex);
    }

    QObject::disconnect(m_previewConnection);
    QObject::disconnect(m_enabledConnection);

    {
        const QSignalBlocker blocker{m_enabledToggle};
        m_enabledToggle->setChecked(pendingState.enabled);
    }
    m_editor->loadSettings(pendingState.settings);

    m_previewConnection = QObject::connect(
        m_editor, &DspSettingsDialog::previewSettingsChanged, this, [this, target](const QByteArray& settings) {
            m_pending[target->instanceId].settings = settings;
            m_controller->updateDspSettings(target->scope, target->instanceId, settings, false);
        });
    m_enabledConnection
        = QObject::connect(m_enabledToggle, &QCheckBox::toggled, this, [this, target](const bool enabled) {
              m_pending[target->instanceId].enabled = enabled;
              m_controller->setDspEnabled(target->scope, target->instanceId, enabled);
          });
}

void DspSettingsDialogSession::commit()
{
    saveCurrent();

    for(const auto& [instanceId, state] : m_pending) {
        if(const auto target = m_controller->targetForInstance(m_dspId, instanceId)) {
            m_controller->setDspEnabled(target->scope, target->instanceId, state.enabled);
            m_controller->updateDspSettings(target->scope, target->instanceId, state.settings, true);
        }
    }
}

DspSettingsDialogSession::PendingState&
DspSettingsDialogSession::pendingFor(const DspSettingsController::Target& target)
{
    auto [it, _]
        = m_pending.emplace(target.instanceId, PendingState{.settings = target.settings, .enabled = target.enabled});
    return it->second;
}
} // namespace

DspSettingsController::DspSettingsController(DspChainStore* chainStore, DspSettingsRegistry* registry, QObject* parent)
    : QObject{parent}
    , m_chainStore{chainStore}
    , m_registry{registry}
{
    QObject::connect(m_chainStore, &DspChainStore::activeChainChanged, this,
                     &DspSettingsController::dspInstancesChanged);
}

QString DspSettingsController::displayName(const QString& dspId) const
{
    if(auto* provider = providerFor(dspId)) {
        const QString name = provider->displayName().trimmed();
        return name.isEmpty() ? dspId : name;
    }
    return dspId;
}

bool DspSettingsController::hasDsp(const QString& dspId) const
{
    return !targetsFor(dspId).empty();
}

FyWidget* DspSettingsController::createLayoutWidget(const QString& dspId, QWidget* parent)
{
    return createDspSettingsLayoutWidget(this, dspId, displayName(dspId), parent);
}

DspLayoutEditor* DspSettingsController::createLayoutEditor(const QString& dspId, QWidget* parent) const
{
    auto* provider = providerFor(dspId);
    return provider ? provider->createLayoutEditor(parent) : nullptr;
}

void DspSettingsController::showDialog(const QString& dspId, QWidget* parent)
{
    auto* editor = createEditor(dspId, parent);
    if(!editor) {
        QMessageBox::warning(parent, tr("DSP Settings"), tr("Unable to open settings for DSP \"%1\".").arg(dspId));
        return;
    }

    const auto targets = targetsFor(dspId);
    if(targets.empty()) {
        editor->deleteLater();
        QMessageBox::warning(parent, tr("DSP Settings"), tr("Unable to find DSP \"%1\".").arg(dspId));
        return;
    }

    const auto originalChain = m_chainStore->activeChain();

    const auto rollback = [this, originalChain]() {
        m_chainStore->setActiveChain(originalChain);
    };

    auto* session = new DspSettingsDialogSession(this, dspId, editor, targets, rollback);
    session->open();
}

DspSettingsProvider* DspSettingsController::providerFor(const QString& dspId) const
{
    return m_registry->providerFor(dspId);
}

std::vector<DspSettingsController::Target> DspSettingsController::targetsFor(const QString& dspId) const
{
    std::vector<Target> targets;

    const auto chain = m_chainStore->activeChain();

    const auto appendTargets = [this, &dspId, &targets, &chain](const Engine::DspChain& subChain,
                                                                const Engine::DspChainScope scope) {
        int matchIndex{0};

        for(const auto& entry : subChain) {
            if(entry.id != dspId) {
                continue;
            }

            ++matchIndex;

            bool supportsLive{false};
            if(auto node = m_chainStore->createDsp(dspId)) {
                supportsLive = node->supportsLiveSettings();
            }

            const QString name  = entry.name.isEmpty() ? dspId : entry.name;
            const QString label = matchIndex == 1 ? tr("%1: %2").arg(scopeLabel(scope), name)
                                                  : tr("%1: %2 (%3)").arg(scopeLabel(scope), name).arg(matchIndex);

            targets.push_back(Target{
                .scope         = scope,
                .instanceId    = entry.instanceId,
                .name          = name,
                .label         = label,
                .settings      = entry.settings,
                .enabled       = entry.enabled,
                .supportsLive  = supportsLive,
                .originalChain = chain,
            });
        }
    };

    appendTargets(chain.masterChain, Engine::DspChainScope::Master);
    appendTargets(chain.perTrackChain, Engine::DspChainScope::PerTrack);

    return targets;
}

std::optional<DspSettingsController::Target> DspSettingsController::targetFor(const QString& dspId) const
{
    const auto targets = targetsFor(dspId);
    return targets.empty() ? std::nullopt : std::optional{targets.front()};
}

std::optional<DspSettingsController::Target> DspSettingsController::targetForInstance(const QString& dspId,
                                                                                      const uint64_t instanceId) const
{
    const auto targets = targetsFor(dspId);

    const auto it = std::ranges::find(targets, instanceId, &Target::instanceId);
    if(it == targets.cend()) {
        return {};
    }

    return *it;
}

DspSettingsDialog* DspSettingsController::createEditor(const QString& dspId, QWidget* parent) const
{
    auto* provider = providerFor(dspId);
    return provider ? provider->createSettingsWidget(parent) : nullptr;
}

void DspSettingsController::bindEditor(DspSettingsDialog* editor, const Target& target, const bool persistPreview)
{
    if(target.instanceId == 0) {
        return;
    }

    QObject::connect(
        editor, &DspSettingsDialog::previewSettingsChanged, this,
        [this, scope = target.scope, instanceId = target.instanceId, persistPreview](const QByteArray& settings) {
            updateDspSettings(scope, instanceId, settings, persistPreview);
        });
}

bool DspSettingsController::updateDspSettings(const Engine::DspChainScope scope, const uint64_t instanceId,
                                              const QByteArray& settings, const bool persist)
{
    return m_chainStore->updateLiveDspSettings(scope, instanceId, settings, persist);
}

bool DspSettingsController::setDspEnabled(const Engine::DspChainScope scope, const uint64_t instanceId,
                                          const bool enabled)
{
    if(instanceId == 0) {
        return false;
    }

    auto chain        = m_chainStore->activeChain();
    auto& targetChain = (scope == Engine::DspChainScope::Master) ? chain.masterChain : chain.perTrackChain;

    for(auto& entry : targetChain) {
        if(entry.instanceId == instanceId) {
            if(entry.enabled == enabled) {
                return true;
            }

            entry.enabled = enabled;
            m_chainStore->setActiveChain(chain);
            return true;
        }
    }

    return false;
}
} // namespace Fooyin

#include "dspsettingscontroller.moc"
#include "moc_dspsettingscontroller.cpp"
