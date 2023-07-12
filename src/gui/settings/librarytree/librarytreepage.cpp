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

#include "librarytreepage.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "gui/librarytree/librarytreegroupregistry.h"
#include "gui/settings/librarytree/librarytreegroupmodel.h"
#include "gui/trackselectioncontroller.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>

namespace Fy::Gui::Settings {
enum ClickType
{
    Middle,
    Double
};

class LibraryTreePageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryTreePageWidget(Widgets::LibraryTreeGroupRegistry* groupsRegistry, Utils::SettingsManager* settings);

    void apply() override;

private:
    void addGroup() const;
    void removeGroup() const;

    Widgets::LibraryTreeGroupRegistry* m_groupsRegistry;
    Utils::SettingsManager* m_settings;

    QTableView* m_groupList;
    LibraryTreeGroupModel* m_model;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QLineEdit* m_playlistName;
};

LibraryTreePageWidget::LibraryTreePageWidget(Widgets::LibraryTreeGroupRegistry* groupsRegistry,
                                             Utils::SettingsManager* settings)
    : m_groupsRegistry{groupsRegistry}
    , m_settings{settings}
    , m_groupList{new QTableView(this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playlistEnabled{new QCheckBox("Enabled", this)}
    , m_autoSwitch{new QCheckBox("Switch when changed", this)}
    , m_playlistName{new QLineEdit(this)}
{
    m_groupList->setModel(m_model);

    // Hide index column
    m_groupList->hideColumn(0);

    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttonsLayout = new QVBoxLayout();
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch();

    auto* clickBehaviour       = new QGroupBox("Click Behaviour", this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* doubleClickLabel = new QLabel("Double-click: ", this);
    auto* middleClickLabel = new QLabel("Middle-click: ", this);

    auto* selectionPlaylist       = new QGroupBox("Library Selection Playlist", this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    auto* playlistNameLabel = new QLabel("Name: ", this);

    clickBehaviourLayout->addWidget(doubleClickLabel, 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(middleClickLabel, 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(playlistNameLabel, 2, 0, 1, 1);
    selectionPlaylistLayout->addWidget(m_playlistName, 2, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_groupList, 0, 0, 1, 3);
    mainLayout->addLayout(buttonsLayout, 0, 3);
    mainLayout->addWidget(clickBehaviour, 1, 0);
    mainLayout->addWidget(selectionPlaylist, 1, 1);

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action) {
        box->addItem(text, QVariant::fromValue(action));
    };

    addTrackAction(m_doubleClick, "Expand/Collapse", TrackAction::Expand);
    addTrackAction(m_doubleClick, "Add to current playlist", TrackAction::AddCurrentPlaylist);
    addTrackAction(m_doubleClick, "Add to active playlist", TrackAction::AddActivePlaylist);
    addTrackAction(m_doubleClick, "Send to current playlist", TrackAction::SendCurrentPlaylist);
    addTrackAction(m_doubleClick, "Send to new playlist", TrackAction::SendNewPlaylist);

    addTrackAction(m_middleClick, "None", TrackAction::None);
    addTrackAction(m_middleClick, "Add to current playlist", TrackAction::AddCurrentPlaylist);
    addTrackAction(m_middleClick, "Add to active playlist", TrackAction::AddActivePlaylist);
    addTrackAction(m_middleClick, "Send to current playlist", TrackAction::SendCurrentPlaylist);
    addTrackAction(m_middleClick, "Send to new playlist", TrackAction::SendNewPlaylist);

    QObject::connect(addButton, &QPushButton::clicked, this, &LibraryTreePageWidget::addGroup);
    QObject::connect(removeButton, &QPushButton::clicked, this, &LibraryTreePageWidget::removeGroup);

    auto clickIndex = [](ClickType type, TrackAction action) -> int {
        switch(action) {
            case(TrackAction::None):
                return type == Middle ? 0 : -1;
            case TrackAction::Expand:
                return type == Double ? 0 : -1;
            case TrackAction::AddCurrentPlaylist:
                return 1;
            case TrackAction::AddActivePlaylist:
                return 2;
            case TrackAction::SendCurrentPlaylist:
                return 3;
            case TrackAction::SendNewPlaylist:
                return 4;
            default:
                return -1;
        }
    };

    auto doubleAction = static_cast<TrackAction>(m_settings->value<Settings::LibraryTreeDoubleClick>());
    m_doubleClick->setCurrentIndex(clickIndex(Double, doubleAction));

    auto middleAction = static_cast<TrackAction>(m_settings->value<Settings::LibraryTreeMiddleClick>());
    m_middleClick->setCurrentIndex(clickIndex(Middle, middleAction));

    QObject::connect(m_playlistEnabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
    });

    m_playlistEnabled->setChecked(m_settings->value<Settings::LibraryTreePlaylistEnabled>());
    m_autoSwitch->setChecked(m_settings->value<Settings::LibraryTreeAutoSwitch>());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());

    m_playlistName->setText(m_settings->value<Settings::LibraryTreeAutoPlaylist>());
}

void LibraryTreePageWidget::apply()
{
    m_model->processQueue();
    m_settings->set<Settings::LibraryTreeDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::LibraryTreeMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::LibraryTreePlaylistEnabled>(m_playlistEnabled->isChecked());
    m_settings->set<Settings::LibraryTreeAutoSwitch>(m_autoSwitch->isChecked());
    m_settings->set<Settings::LibraryTreeAutoPlaylist>(m_playlistName->text());
}

void LibraryTreePageWidget::addGroup() const
{
    m_model->addNewGroup();
}

void LibraryTreePageWidget::removeGroup() const
{
    const auto selectedItems = m_groupList->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = static_cast<LibraryTreeGroupItem*>(selected.internalPointer());
        m_model->markForRemoval(item->group());
    }
}

LibraryTreePage::LibraryTreePage(Widgets::LibraryTreeGroupRegistry* groupsRegistry, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WidgetsLibraryTree);
    setName(tr("Library Tree"));
    setCategory({"Widgets", "Library Tree"});
    setWidgetCreator([groupsRegistry, settings] {
        return new LibraryTreePageWidget(groupsRegistry, settings);
    });
}
} // namespace Fy::Gui::Settings
