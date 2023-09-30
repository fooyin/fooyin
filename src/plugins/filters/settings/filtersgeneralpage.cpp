/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtersgeneralpage.h"

#include "constants.h"
#include "filtersettings.h"

#include <gui/trackselectioncontroller.h>

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace Fy::Filters::Settings {
enum ClickType
{
    Middle,
    Double
};

class FiltersGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit FiltersGeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QLineEdit* m_playlistName;
};

FiltersGeneralPageWidget::FiltersGeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playlistEnabled{new QCheckBox("Enabled", this)}
    , m_autoSwitch{new QCheckBox("Switch when changed", this)}
    , m_playlistName{new QLineEdit(this)}
{
    auto* clickBehaviour       = new QGroupBox("Click Behaviour", this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* doubleClickLabel = new QLabel("Double-click: ", this);
    auto* middleClickLabel = new QLabel("Middle-click: ", this);

    clickBehaviourLayout->addWidget(doubleClickLabel, 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(middleClickLabel, 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* selectionPlaylist       = new QGroupBox("Filter Selection Playlist", this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    auto* playlistNameLabel = new QLabel("Name: ", this);

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(playlistNameLabel, 2, 0, 1, 1);
    selectionPlaylistLayout->addWidget(m_playlistName, 2, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(clickBehaviour, 0, 0);
    mainLayout->addWidget(selectionPlaylist, 0, 1);
    mainLayout->setRowStretch(1, 1);

    using ActionIndexMap = std::map<int, int>;
    ActionIndexMap doubleActions;
    ActionIndexMap middleActions;

    auto addTrackAction = [](QComboBox* box, const QString& text, Gui::TrackAction action, ActionIndexMap& actionMap) {
        const int actionValue = static_cast<int>(action);
        actionMap.emplace(actionValue, box->count());
        box->addItem(text, actionValue);
    };

    addTrackAction(m_doubleClick, "None", Gui::TrackAction::None, doubleActions);
    addTrackAction(m_doubleClick, "Add to current playlist", Gui::TrackAction::AddCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, "Add to active playlist", Gui::TrackAction::AddActivePlaylist, doubleActions);
    addTrackAction(m_doubleClick, "Send to current playlist", Gui::TrackAction::SendCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, "Send to new playlist", Gui::TrackAction::SendNewPlaylist, doubleActions);

    addTrackAction(m_middleClick, "None", Gui::TrackAction::None, middleActions);
    addTrackAction(m_middleClick, "Add to current playlist", Gui::TrackAction::AddCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, "Add to active playlist", Gui::TrackAction::AddActivePlaylist, middleActions);
    addTrackAction(m_middleClick, "Send to current playlist", Gui::TrackAction::SendCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, "Send to new playlist", Gui::TrackAction::SendNewPlaylist, middleActions);

    auto doubleAction = m_settings->value<Settings::FilterDoubleClick>();
    if(doubleActions.contains(doubleAction)) {
        m_doubleClick->setCurrentIndex(doubleActions.at(doubleAction));
    }

    auto middleAction = m_settings->value<Settings::FilterMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    QObject::connect(m_playlistEnabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
    });

    m_playlistEnabled->setChecked(m_settings->value<Settings::FilterPlaylistEnabled>());
    m_autoSwitch->setChecked(m_settings->value<Settings::FilterAutoSwitch>());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());

    m_playlistName->setText(m_settings->value<Settings::FilterAutoPlaylist>());
}

void FiltersGeneralPageWidget::apply()
{
    m_settings->set<Settings::FilterDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::FilterMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::FilterPlaylistEnabled>(m_playlistEnabled->isChecked());
    m_settings->set<Settings::FilterAutoSwitch>(m_autoSwitch->isChecked());
    m_settings->set<Settings::FilterAutoPlaylist>(m_playlistName->text());
}

FiltersGeneralPage::FiltersGeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersGeneral);
    setName(tr("General"));
    setCategory({"Plugins", "Filters"});
    setWidgetCreator([settings] {
        return new FiltersGeneralPageWidget(settings);
    });
}
} // namespace Fy::Filters::Settings
