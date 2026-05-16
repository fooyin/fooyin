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

#include "dspsettingslayoutwidget.h"

#include "dspsettingsregistry.h"

#include <gui/dsp/dsplayouteditor.h>
#include <gui/dsp/dspsettingsdialog.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QJsonObject>
#include <QMenu>
#include <QSignalBlocker>
#include <QVBoxLayout>

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
} // namespace

FyWidget* createDspSettingsLayoutWidget(DspSettingsController* controller, QString dspId, QString displayName,
                                        QWidget* parent)
{
    if(auto* layoutEditor = controller->createLayoutEditor(dspId)) {
        return new DspCompactLayoutWidget(controller, std::move(dspId), std::move(displayName), layoutEditor, parent);
    }

    return new DspSettingsDialogLayoutWidget(controller, std::move(dspId), std::move(displayName), parent);
}

DspLayoutWidgetBase::DspLayoutWidgetBase(DspSettingsController* controller, QString dspId, QString displayName,
                                         QWidget* parent)
    : FyWidget{parent}
    , m_controller{controller}
    , m_dspId{std::move(dspId)}
    , m_displayName{std::move(displayName)}
    , m_currentInstanceId{0}
{
    QObject::connect(m_controller, &DspSettingsController::dspInstancesChanged, this,
                     &DspLayoutWidgetBase::refreshInstances);
}

QString DspLayoutWidgetBase::name() const
{
    return m_displayName;
}

QString DspLayoutWidgetBase::layoutName() const
{
    return DspSettingsRegistry::layoutWidgetKey(m_dspId);
}

void DspLayoutWidgetBase::saveLayoutData(QJsonObject& layout)
{
    if(m_currentInstanceId != 0) {
        layout["InstanceID"_L1] = QString::number(m_currentInstanceId);
    }

    saveEditorLayoutData(layout);
}

void DspLayoutWidgetBase::loadLayoutData(const QJsonObject& layout)
{
    loadEditorLayoutData(layout);

    if(layout.contains("InstanceID"_L1)) {
        m_currentInstanceId = static_cast<uint64_t>(layout.value("InstanceID"_L1).toString().toULongLong());
        refreshInstances();
    }
}

void DspLayoutWidgetBase::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    addDspMenuActions(menu);
    menu->popup(event->globalPos());
}

DspSettingsController* DspLayoutWidgetBase::controller() const
{
    return m_controller;
}

QString DspLayoutWidgetBase::dspId() const
{
    return m_dspId;
}

uint64_t DspLayoutWidgetBase::currentInstanceId() const
{
    return m_currentInstanceId;
}

void DspLayoutWidgetBase::refreshInstances()
{
    const auto targets = m_controller->targetsFor(m_dspId);

    populateInstanceSelector(targets);

    uint64_t selectedInstanceId{0};
    for(const auto& target : targets) {
        if(selectedInstanceId == 0 || target.instanceId == m_currentInstanceId) {
            selectedInstanceId = target.instanceId;
        }
    }

    if(selectedInstanceId == 0) {
        clearInstance();
        return;
    }

    selectInstance(selectedInstanceId);
}

void DspLayoutWidgetBase::selectInstance(const uint64_t instanceId)
{
    if(instanceId == 0) {
        return;
    }

    const auto target = m_controller->targetForInstance(m_dspId, instanceId);
    if(!target) {
        return;
    }

    m_currentInstanceId = instanceId;
    updateSelectedInstance(instanceId);
    disconnectEditor();

    updateEnabledState(target->enabled);
    loadEditorSettings(target->settings);
    setEditorControlsEnabled(target->enabled);

    connectEditor(*target);
}

void DspLayoutWidgetBase::populateInstanceSelector(const std::vector<DspSettingsController::Target>& /*targets*/) { }

void DspLayoutWidgetBase::updateSelectedInstance(uint64_t /*instanceId*/) { }

void DspLayoutWidgetBase::updateEnabledState(bool /*enabled*/) { }

void DspLayoutWidgetBase::saveEditorLayoutData(QJsonObject& /*layout*/) { }

void DspLayoutWidgetBase::loadEditorLayoutData(const QJsonObject& /*layout*/) { }

void DspLayoutWidgetBase::addEditorActions(QMenu* /*menu*/) { }

void DspLayoutWidgetBase::disconnectEditor() { }

void DspLayoutWidgetBase::addDspMenuActions(QMenu* menu)
{
    if(!menu) {
        return;
    }

    addInstanceMenu(menu);
    addEnabledAction(menu);
    addEditorActions(menu);
}

void DspLayoutWidgetBase::addInstanceMenu(QMenu* menu)
{
    const auto targets = m_controller->targetsFor(m_dspId);
    if(targets.size() <= 1) {
        return;
    }

    auto* instanceMenu = new QMenu(tr("Instance"), menu);
    menu->addMenu(instanceMenu);

    for(const auto& target : targets) {
        auto* action = new QAction(target.label, instanceMenu);
        instanceMenu->addAction(action);
        action->setCheckable(true);
        action->setChecked(target.instanceId == m_currentInstanceId);
        QObject::connect(action, &QAction::triggered, this,
                         [this, instanceId = target.instanceId]() { selectInstance(instanceId); });
    }
}

void DspLayoutWidgetBase::addEnabledAction(QMenu* menu)
{
    const auto currentTarget = m_controller->targetForInstance(m_dspId, m_currentInstanceId);

    auto* enabled = new QAction(tr("Enabled"), menu);
    menu->addAction(enabled);
    enabled->setCheckable(true);
    enabled->setEnabled(currentTarget.has_value());
    enabled->setChecked(currentTarget ? currentTarget->enabled : false);

    QObject::connect(enabled, &QAction::triggered, this, [this](const bool checked) {
        if(const auto target = m_controller->targetForInstance(m_dspId, m_currentInstanceId)) {
            m_controller->setDspEnabled(target->scope, target->instanceId, checked);
            updateEnabledState(checked);
            setEditorControlsEnabled(checked);
        }
    });
}

void DspLayoutWidgetBase::clearInstance()
{
    disconnectEditor();

    m_currentInstanceId = 0;
    updateEnabledState(false);
    setEditorControlsEnabled(false);
}

DspSettingsDialogLayoutWidget::DspSettingsDialogLayoutWidget(DspSettingsController* controller, QString dspId,
                                                             QString displayName, QWidget* parent)
    : DspLayoutWidgetBase{controller, std::move(dspId), std::move(displayName), parent}
    , m_editor{controller->createEditor(this->dspId(), this)}
    , m_instanceSelector{addInstanceSelector(m_editor)}
    , m_enabledToggle{addEnabledToggle(m_editor, false)}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});

    m_editor->setButtonsVisible(false);
    m_editor->setWindowFlags(Qt::Widget);
    layout->addWidget(m_editor);

    QObject::connect(m_instanceSelector, &QComboBox::currentIndexChanged, m_editor, [this](int index) {
        if(index >= 0) {
            selectInstance(m_instanceSelector->itemData(index).toULongLong());
        }
    });

    refreshInstances();
}

void DspSettingsDialogLayoutWidget::populateInstanceSelector(const std::vector<DspSettingsController::Target>& targets)
{
    const QSignalBlocker blocker{m_instanceSelector};
    m_instanceSelector->clear();

    for(const auto& target : targets) {
        m_instanceSelector->addItem(target.label, QString::number(target.instanceId));
    }

    m_instanceSelector->setVisible(targets.size() > 1);
}

void DspSettingsDialogLayoutWidget::updateSelectedInstance(uint64_t instanceId)
{
    const int index = m_instanceSelector->findData(QString::number(instanceId));
    if(index >= 0 && m_instanceSelector->currentIndex() != index) {
        const QSignalBlocker blocker{m_instanceSelector};
        m_instanceSelector->setCurrentIndex(index);
    }
}

void DspSettingsDialogLayoutWidget::updateEnabledState(bool enabled)
{
    const QSignalBlocker blocker{m_enabledToggle};
    m_enabledToggle->setChecked(enabled);
}

void DspSettingsDialogLayoutWidget::connectEditor(const DspSettingsController::Target& target)
{
    m_previewConnection = QObject::connect(
        m_editor, &DspSettingsDialog::previewSettingsChanged, m_editor,
        [controller = controller(), scope = target.scope, instanceId = target.instanceId](const QByteArray& settings) {
            controller->updateDspSettings(scope, instanceId, settings, true);
        });

    m_enabledConnection = QObject::connect(
        m_enabledToggle, &QCheckBox::toggled, m_editor,
        [this, controller = controller(), scope = target.scope, instanceId = target.instanceId](const bool enabled) {
            controller->setDspEnabled(scope, instanceId, enabled);
            setEditorControlsEnabled(enabled);
        });
}

void DspSettingsDialogLayoutWidget::disconnectEditor()
{
    QObject::disconnect(m_previewConnection);
    QObject::disconnect(m_enabledConnection);
}

void DspSettingsDialogLayoutWidget::loadEditorSettings(const QByteArray& settings)
{
    m_editor->loadSettings(settings);
}

void DspSettingsDialogLayoutWidget::setEditorControlsEnabled(bool enabled)
{
    m_editor->setEnabled(enabled);
}

DspCompactLayoutWidget::DspCompactLayoutWidget(DspSettingsController* controller, QString dspId, QString displayName,
                                               DspLayoutEditor* editor, QWidget* parent)
    : DspLayoutWidgetBase{controller, std::move(dspId), std::move(displayName), parent}
    , m_editor{editor}
{
    m_editor->setParent(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    refreshInstances();
}

void DspCompactLayoutWidget::saveEditorLayoutData(QJsonObject& layout)
{
    QJsonObject editorData;
    m_editor->saveLayoutData(editorData);
    if(!editorData.empty()) {
        layout["Editor"_L1] = editorData;
    }
}

void DspCompactLayoutWidget::loadEditorLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Editor"_L1)) {
        m_editor->loadLayoutData(layout.value("Editor"_L1).toObject());
    }
}

void DspCompactLayoutWidget::addEditorActions(QMenu* menu)
{
    menu->addSeparator();
    m_editor->populateContextMenu(menu);
    menu->addSeparator();

    auto* restoreDefaults = new QAction(tr("Restore Defaults"), menu);
    menu->addAction(restoreDefaults);
    restoreDefaults->setEnabled(currentInstanceId() != 0);
    QObject::connect(restoreDefaults, &QAction::triggered, this, [this]() {
        if(currentInstanceId() == 0) {
            return;
        }

        m_editor->restoreDefaults();
        const auto target = controller()->targetForInstance(dspId(), currentInstanceId());
        if(target) {
            controller()->updateDspSettings(target->scope, target->instanceId, m_editor->saveSettings(), true);
        }
    });

    auto* configure = new QAction(tr("Configure…"), menu);
    menu->addAction(configure);
    configure->setEnabled(currentInstanceId() != 0);
    QObject::connect(configure, &QAction::triggered, this, [this]() { controller()->showDialog(dspId(), window()); });
}

void DspCompactLayoutWidget::connectEditor(const DspSettingsController::Target& target)
{
    m_previewConnection = QObject::connect(
        m_editor, &DspLayoutEditor::previewSettingsChanged, m_editor,
        [controller = controller(), scope = target.scope, instanceId = target.instanceId](const QByteArray& settings) {
            controller->updateDspSettings(scope, instanceId, settings, true);
        });
}

void DspCompactLayoutWidget::disconnectEditor()
{
    QObject::disconnect(m_previewConnection);
}

void DspCompactLayoutWidget::loadEditorSettings(const QByteArray& settings)
{
    m_editor->loadSettings(settings);
}

void DspCompactLayoutWidget::setEditorControlsEnabled(const bool enabled)
{
    m_editor->setControlsEnabled(enabled);
}
} // namespace Fooyin
