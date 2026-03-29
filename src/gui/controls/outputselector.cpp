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

#include "outputselector.h"

#include "output/outputprofilemanager.h"

#include <gui/guiconstants.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

namespace Fooyin {
OutputSelector::OutputSelector(OutputProfileManager* profileManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_profileManager{profileManager}
    , m_settings{settings}
    , m_label{new QLabel(tr("Output") + u": "_s, this)}
    , m_combo{new ExpandingComboBox(this)}
    , m_showLabel{true}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_label);
    layout->addWidget(m_combo, 1);

    m_combo->setResizeToCurrentEnabled(false);
    m_label->setContextMenuPolicy(Qt::CustomContextMenu);
    m_combo->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if(index < 0) {
            return;
        }

        const QString output = m_combo->currentData(Qt::UserRole).toString();
        const QString device = m_combo->currentData(Qt::UserRole + 1).toString();
        m_profileManager->applyProfile(output, device);
    });

    QObject::connect(m_profileManager, &OutputProfileManager::profilesChanged, this, &OutputSelector::reload);
    QObject::connect(m_profileManager, &OutputProfileManager::currentOutputChanged, this, &OutputSelector::reload);

    QObject::connect(m_label, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_label->mapToGlobal(pos)); });
    QObject::connect(m_combo, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_combo->mapToGlobal(pos)); });

    setShowLabel(m_showLabel);
    reload();
}

QString OutputSelector::name() const
{
    return tr("Output Selector");
}

QString OutputSelector::layoutName() const
{
    return u"OutputSelector"_s;
}

void OutputSelector::saveLayoutData(QJsonObject& layout)
{
    layout["ShowLabel"_L1] = m_showLabel;
}

void OutputSelector::loadLayoutData(const QJsonObject& layout)
{
    setShowLabel(layout.value("ShowLabel"_L1).toBool());
}

void OutputSelector::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void OutputSelector::reload()
{
    const QString currentOutput = m_profileManager->currentOutput();
    const QString currentDevice = m_profileManager->currentDevice();
    const auto entries          = m_profileManager->deviceEntries(currentOutput);

    const QSignalBlocker blocker{m_combo};
    m_combo->clear();

    for(const auto& entry : entries) {
        if(!entry.enabled && entry.device != currentDevice) {
            continue;
        }

        m_combo->addItem(entry.description, currentOutput);
        m_combo->setItemData(m_combo->count() - 1, entry.device, Qt::UserRole + 1);
        if(entry.device == currentDevice) {
            m_combo->setCurrentIndex(m_combo->count() - 1);
        }
    }

    m_combo->setEnabled(m_combo->count() > 0);
    m_combo->resizeDropDown();
}

void OutputSelector::setShowLabel(bool showLabel)
{
    m_showLabel = showLabel;
    m_label->setVisible(m_showLabel);
    updateGeometry();
}

void OutputSelector::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabel = new QAction(tr("Show label"), menu);
    showLabel->setCheckable(true);
    showLabel->setChecked(m_showLabel);
    QObject::connect(showLabel, &QAction::triggered, this, &OutputSelector::setShowLabel);
    menu->addAction(showLabel);

    auto* configureDevices = new QAction(tr("Configure listed devices…"), menu);
    QObject::connect(configureDevices, &QAction::triggered, this, [this]() {
        if(m_settings && m_settings->settingsDialog()) {
            m_settings->settingsDialog()->openAtPage(Id{Constants::Page::OutputDevices});
        }
    });
    menu->addAction(configureDevices);

    menu->popup(globalPos);
}
} // namespace Fooyin

#include "moc_outputselector.cpp"
