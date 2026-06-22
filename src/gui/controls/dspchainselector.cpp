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

#include "dspchainselector.h"

#include "dsp/dsppresetregistry.h"

#include <core/engine/dsp/dspchainstore.h>
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

constexpr auto ConfigureId = -1;

namespace Fooyin {
DspChainSelector::DspChainSelector(DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                                   SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_chainStore{chainStore}
    , m_presetRegistry{presetRegistry}
    , m_settings{settings}
    , m_label{new QLabel(tr("DSP") + u": "_s, this)}
    , m_combo{new ExpandingComboBox(this)}
    , m_showLabel{true}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_label);
    layout->addWidget(m_combo, 1);

    m_combo->setResizeToCurrentEnabled(false);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_label->setContentsMargins(5, 0, 0, 0);
    m_label->setContextMenuPolicy(Qt::CustomContextMenu);
    m_combo->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if(index < 0) {
            return;
        }

        const int presetId = m_combo->itemData(index).toInt();
        if(presetId == ConfigureId) {
            reload();
            if(auto* settingsDlg = m_settings->settingsDialog()) {
                settingsDlg->openAtPage(Id{Constants::Page::DspManager});
            }
            return;
        }
        if(const auto preset = m_presetRegistry->itemById(presetId)) {
            m_chainStore->setActiveChain(preset->chain);
        }
    });

    QObject::connect(m_chainStore, &DspChainStore::activeChainChanged, this, &DspChainSelector::reload);
    QObject::connect(m_presetRegistry, &RegistryBase::itemAdded, this, &DspChainSelector::reload);
    QObject::connect(m_presetRegistry, &RegistryBase::itemChanged, this, &DspChainSelector::reload);
    QObject::connect(m_presetRegistry, &RegistryBase::itemRemoved, this, &DspChainSelector::reload);

    QObject::connect(m_label, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_label->mapToGlobal(pos)); });
    QObject::connect(m_combo, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_combo->mapToGlobal(pos)); });

    setShowLabel(m_showLabel);
    reload();
}

QString DspChainSelector::name() const
{
    return tr("DSP Selector");
}

QString DspChainSelector::layoutName() const
{
    return u"DspSelector"_s;
}

void DspChainSelector::saveLayoutData(QJsonObject& layout)
{
    layout["ShowLabel"_L1] = m_showLabel;
}

void DspChainSelector::loadLayoutData(const QJsonObject& layout)
{
    setShowLabel(layout.value("ShowLabel"_L1).toBool());
}

void DspChainSelector::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void DspChainSelector::reload()
{
    const auto activeChain = m_chainStore->activeChain();
    const auto presets     = m_presetRegistry->items();

    const QSignalBlocker blocker{m_combo};
    m_combo->clear();

    int currentIndex{-1};
    for(const auto& preset : presets) {
        m_combo->addItem(preset.name, preset.id);
        if(preset.chain == activeChain) {
            currentIndex = m_combo->count() - 1;
        }
    }

    if(!presets.empty()) {
        m_combo->insertSeparator(m_combo->count());
    }
    m_combo->addItem(tr("Configure…"), ConfigureId);

    m_combo->setCurrentIndex(currentIndex);
    m_combo->resizeDropDown();
}

void DspChainSelector::setShowLabel(bool showLabel)
{
    m_showLabel = showLabel;
    m_label->setVisible(m_showLabel);
    updateGeometry();
}

void DspChainSelector::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabel = new QAction(tr("Show label"), menu);
    showLabel->setCheckable(true);
    showLabel->setChecked(m_showLabel);
    QObject::connect(showLabel, &QAction::triggered, this, &DspChainSelector::setShowLabel);
    menu->addAction(showLabel);

    menu->popup(globalPos);
}
} // namespace Fooyin

#include "moc_dspchainselector.cpp"
