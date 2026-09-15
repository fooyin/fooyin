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

#include "playlisttabsconfigdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace Fooyin {
PlaylistTabsConfigDialog::PlaylistTabsConfigDialog(PlaylistTabs* playlistTabs, QWidget* parent)
    : WidgetConfigDialog{playlistTabs, tr("Playlist Tabs Settings"), parent}
    , m_position{new QComboBox(this)}
    , m_expand{new QCheckBox(tr("Expand tabs to fill empty space"), this)}
    , m_showAddButton{new QCheckBox(tr("Show add button"), this)}
    , m_showClearButton{new QCheckBox(tr("Show clear button"), this)}
    , m_showCloseButton{new QCheckBox(tr("Show delete button on tabs"), this)}
    , m_closeOnMiddleClick{new QCheckBox(tr("Delete playlists on middle click"), this)}
{
    m_position->addItem(tr("Top"), static_cast<int>(PlaylistTabPosition::Top));
    m_position->addItem(tr("Bottom"), static_cast<int>(PlaylistTabPosition::Bottom));

    auto* layout{contentLayout()};

    int row{0};
    layout->addWidget(new QLabel(tr("Position") + u":"_s, this), row, 0);
    layout->addWidget(m_position, row++, 1);
    layout->addWidget(m_expand, row++, 0, 1, 3);
    layout->addWidget(m_showAddButton, row++, 0, 1, 3);
    layout->addWidget(m_showClearButton, row++, 0, 1, 3);
    layout->addWidget(m_showCloseButton, row++, 0, 1, 3);
    layout->addWidget(m_closeOnMiddleClick, row++, 0, 1, 3);
    layout->setRowStretch(row, 1);
    layout->setColumnStretch(2, 1);

    QObject::connect(playlistTabs, &PlaylistTabs::configChanged, this, &PlaylistTabsConfigDialog::syncCurrentConfig);

    loadCurrentConfig();
}

PlaylistTabs::ConfigData PlaylistTabsConfigDialog::config() const
{
    return {
        .position           = static_cast<PlaylistTabPosition>(m_position->currentData().toInt()),
        .expand             = m_expand->isChecked(),
        .showAddButton      = m_showAddButton->isChecked(),
        .showClearButton    = m_showClearButton->isChecked(),
        .showCloseButton    = m_showCloseButton->isChecked(),
        .closeOnMiddleClick = m_closeOnMiddleClick->isChecked(),
    };
}

void PlaylistTabsConfigDialog::setConfig(const PlaylistTabs::ConfigData& config)
{
    m_position->setCurrentIndex(m_position->findData(static_cast<int>(config.position)));
    m_expand->setChecked(config.expand);
    m_showAddButton->setChecked(config.showAddButton);
    m_showClearButton->setChecked(config.showClearButton);
    m_showCloseButton->setChecked(config.showCloseButton);
    m_closeOnMiddleClick->setChecked(config.closeOnMiddleClick);
}

void PlaylistTabsConfigDialog::mergeExternalConfig(const PlaylistTabs::ConfigData& previous,
                                                   const PlaylistTabs::ConfigData& current)
{
    mergeExternalFields(previous, current, &PlaylistTabs::ConfigData::position, &PlaylistTabs::ConfigData::expand,
                        &PlaylistTabs::ConfigData::showAddButton, &PlaylistTabs::ConfigData::showClearButton,
                        &PlaylistTabs::ConfigData::showCloseButton, &PlaylistTabs::ConfigData::closeOnMiddleClick);
}
} // namespace Fooyin
