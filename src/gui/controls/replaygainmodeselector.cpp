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

#include "replaygainmodeselector.h"

#include <core/coresettings.h>
#include <core/engine/enginedefs.h>
#include <core/internalcoresettings.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

constexpr auto DisabledMode = -1;

constexpr bool isActiveReplayGainMode(int mode)
{
    static constexpr int ApplyGainAndPreventClipping = Fooyin::Engine::ApplyGain | Fooyin::Engine::PreventClipping;

    return mode == Fooyin::Engine::ApplyGain || mode == Fooyin::Engine::PreventClipping
        || mode == ApplyGainAndPreventClipping;
}

namespace Fooyin {
ReplayGainModeSelector::ReplayGainModeSelector(SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_label{new QLabel(tr("ReplayGain") + u": "_s, this)}
    , m_combo{new ExpandingComboBox(this)}
    , m_showLabel{true}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_label);
    layout->addWidget(m_combo, 1);

    m_label->setContentsMargins(5, 0, 0, 0);
    m_label->setContextMenuPolicy(Qt::CustomContextMenu);

    m_combo->setResizeToCurrentEnabled(false);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_combo->setContextMenuPolicy(Qt::CustomContextMenu);

    m_combo->addItem(tr("Disabled"), DisabledMode);
    m_combo->addItem(tr("Prefer track gain"), static_cast<int>(ReplayGainType::Track));
    m_combo->addItem(tr("Prefer album gain"), static_cast<int>(ReplayGainType::Album));
    m_combo->addItem(tr("By playback order"), static_cast<int>(ReplayGainType::PlaybackOrder));

    m_combo->resizeDropDown();

    QObject::connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if(index < 0) {
            return;
        }

        const int mode = m_combo->itemData(index).toInt();
        if(mode == DisabledMode) {
            m_settings->set<Settings::Core::RGMode>(static_cast<int>(Engine::NoProcessing));
            return;
        }

        m_settings->set<Settings::Core::RGType>(mode);
        if(m_settings->value<Settings::Core::RGMode>() == Engine::NoProcessing) {
            int processingMode = m_settings->value<Settings::Core::Internal::ReplayGainLastActiveMode>();
            if(!isActiveReplayGainMode(processingMode)) {
                processingMode = Engine::ApplyGain;
            }
            m_settings->set<Settings::Core::RGMode>(processingMode);
        }
    });

    m_settings->subscribe<Settings::Core::RGMode>(this, &ReplayGainModeSelector::reload);
    m_settings->subscribe<Settings::Core::RGType>(this, &ReplayGainModeSelector::reload);

    QObject::connect(m_label, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_label->mapToGlobal(pos)); });
    QObject::connect(m_combo, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_combo->mapToGlobal(pos)); });

    setShowLabel(m_showLabel);
    reload();
}

QString ReplayGainModeSelector::name() const
{
    return tr("ReplayGain Mode");
}

QString ReplayGainModeSelector::layoutName() const
{
    return u"ReplayGainMode"_s;
}

void ReplayGainModeSelector::saveLayoutData(QJsonObject& layout)
{
    layout["ShowLabel"_L1] = m_showLabel;
}

void ReplayGainModeSelector::loadLayoutData(const QJsonObject& layout)
{
    setShowLabel(layout.value("ShowLabel"_L1).toBool());
}

void ReplayGainModeSelector::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void ReplayGainModeSelector::reload()
{
    const int mode = m_settings->value<Settings::Core::RGMode>() == Engine::NoProcessing
                       ? DisabledMode
                       : m_settings->value<Settings::Core::RGType>();

    const QSignalBlocker blocker{m_combo};
    m_combo->setCurrentIndex(m_combo->findData(mode));
}

void ReplayGainModeSelector::setShowLabel(bool showLabel)
{
    m_showLabel = showLabel;
    m_label->setVisible(m_showLabel);
    m_combo->setItemText(m_combo->findData(DisabledMode), m_showLabel ? tr("Disabled") : tr("ReplayGain disabled"));
    m_combo->resizeDropDown();
    m_combo->updateGeometry();
    updateGeometry();
}

void ReplayGainModeSelector::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabel = new QAction(tr("Show label"), menu);
    showLabel->setCheckable(true);
    showLabel->setChecked(m_showLabel);
    QObject::connect(showLabel, &QAction::triggered, this, &ReplayGainModeSelector::setShowLabel);
    menu->addAction(showLabel);

    menu->popup(globalPos);
}
} // namespace Fooyin

#include "moc_replaygainmodeselector.cpp"
