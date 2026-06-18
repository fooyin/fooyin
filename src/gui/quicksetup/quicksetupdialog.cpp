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

#include "quicksetupdialog.h"

#include "playlist/playlistwidget.h"
#include "playlist/presetregistry.h"

#include <gui/editablelayout.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/theme/themeregistry.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace Fooyin {
QuickSetupDialog::QuickSetupDialog(LayoutProvider* layoutProvider, ThemeRegistry* themeRegistry,
                                   PresetRegistry* presetRegistry, EditableLayout* editableLayout,
                                   SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_layoutProvider{layoutProvider}
    , m_themeRegistry{themeRegistry}
    , m_presetRegistry{presetRegistry}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_layoutList{new QListWidget(this)}
    , m_themeList{new QListWidget(this)}
    , m_playlistPresetList{new QListWidget(this)}
    , m_playlistPresetGroup{new QGroupBox(tr("Playlist Layout"), this)}
    , m_accept{new QPushButton(tr("OK"), this)}
{
    setObjectName(u"Quick Setup"_s);
    setWindowTitle(tr("Quick Setup"));
    setModal(true);

    auto* layout = new QGridLayout(this);

    auto* layoutGroup       = new QGroupBox(tr("Layout"), this);
    auto* layoutGroupLayout = new QGridLayout(layoutGroup);
    layoutGroupLayout->addWidget(m_layoutList, 0, 0);

    auto* themeGroup       = new QGroupBox(tr("Colours"), this);
    auto* themeGroupLayout = new QGridLayout(themeGroup);
    themeGroupLayout->addWidget(m_themeList, 0, 0);

    auto* presetGroupLayout = new QGridLayout(m_playlistPresetGroup);
    presetGroupLayout->addWidget(m_playlistPresetList, 0, 0);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(m_accept, QDialogButtonBox::AcceptRole);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QuickSetupDialog::close);

    int row{0};
    layout->addWidget(layoutGroup, row, 0, 2, 1);
    layout->addWidget(themeGroup, row++, 1);
    layout->addWidget(m_playlistPresetGroup, row++, 1);
    layout->addWidget(buttons, row++, 0, 1, 2);

    m_layoutList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_themeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistPresetList->setSelectionMode(QAbstractItemView::SingleSelection);

    populateLayouts();
    populateThemes();
    populatePlaylistPresets();

    QObject::connect(m_layoutList, &QListWidget::currentItemChanged, this, &QuickSetupDialog::changeLayout);
    QObject::connect(m_themeList, &QListWidget::currentItemChanged, this, &QuickSetupDialog::changeTheme);
    QObject::connect(m_playlistPresetList, &QListWidget::currentItemChanged, this,
                     &QuickSetupDialog::changePlaylistPreset);

    const auto playlists = m_editableLayout->findWidgetsByType<PlaylistWidget>();
    m_playlistPresetGroup->setEnabled(!playlists.empty());
}

QSize QuickSetupDialog::sizeHint() const
{
    return {620, 360};
}

void QuickSetupDialog::populateLayouts() const
{
    const auto current = m_layoutProvider->currentLayout();

    const auto layouts = m_layoutProvider->layouts();
    for(const auto& layout : layouts) {
        auto* item = new QListWidgetItem(layout.name(), m_layoutList);
        item->setData(Id, layout.name());
        if(layout.name() == current.name()) {
            m_layoutList->setCurrentItem(item);
        }
    }
}

void QuickSetupDialog::populateThemes() const
{
    auto* systemDefaults = new QListWidgetItem(tr("System defaults"), m_themeList);
    systemDefaults->setData(Id, -1);

    const auto current = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();

    QListWidgetItem* currentThemeItem{nullptr};

    const auto themes = m_themeRegistry->items();
    for(const auto& theme : themes) {
        auto* item = new QListWidgetItem(theme.name, m_themeList);
        item->setData(Id, theme.id);
        if(theme.id == current.id && theme.name == current.name) {
            currentThemeItem = item;
        }
        else if(!currentThemeItem && theme.name == current.name) {
            currentThemeItem = item;
        }
    }

    if(currentThemeItem) {
        m_themeList->setCurrentItem(currentThemeItem);
    }
}

void QuickSetupDialog::populatePlaylistPresets() const
{
    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        auto* item = new QListWidgetItem(preset.name, m_playlistPresetList);
        item->setData(Id, preset.id);
    }
}

void QuickSetupDialog::changeLayout()
{
    if(!m_layoutList->currentItem()) {
        return;
    }

    const QString name = m_layoutList->currentItem()->data(Id).toString();
    const auto layout  = m_layoutProvider->layoutByName(name);
    if(!layout.isValid()) {
        return;
    }

    m_editableLayout->changeLayout(layout);

    const auto playlists = m_editableLayout->findWidgetsByType<PlaylistWidget>();
    m_playlistPresetGroup->setEnabled(!playlists.empty());
}

void QuickSetupDialog::changeTheme()
{
    if(!m_themeList->currentItem()) {
        return;
    }

    const int id = m_themeList->currentItem()->data(Id).toInt();
    if(id < 0) {
        m_settings->reset<Settings::Gui::CustomTheme>();
        return;
    }

    if(const auto theme = m_themeRegistry->itemById(id)) {
        m_settings->set<Settings::Gui::CustomTheme>(QVariant::fromValue(theme.value()));
    }
}

void QuickSetupDialog::changePlaylistPreset()
{
    if(!m_playlistPresetList->currentItem()) {
        return;
    }

    const int id = m_playlistPresetList->currentItem()->data(Id).toInt();
    if(const auto preset = m_presetRegistry->itemById(id)) {
        const auto playlists = m_editableLayout->findWidgetsByType<PlaylistWidget>();
        for(auto* playlist : playlists) {
            if(playlist) {
                playlist->changePreset(preset.value());
            }
        }
    }
}
} // namespace Fooyin

#include "moc_quicksetupdialog.cpp"
