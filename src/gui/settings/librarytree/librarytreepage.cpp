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

#include "librarytree/librarytreegroupregistry.h"
#include "librarytreegroupmodel.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/multilinedelegate.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>

namespace Fooyin {
class LibraryTreePageWidget : public SettingsPageWidget
{
public:
    explicit LibraryTreePageWidget(LibraryTreeGroupRegistry* groupsRegistry, SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void addGroup() const;
    void removeGroup() const;

    LibraryTreeGroupRegistry* m_groupsRegistry;
    SettingsManager* m_settings;

    QTableView* m_groupList;
    LibraryTreeGroupModel* m_model;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QLineEdit* m_playlistName;
};

LibraryTreePageWidget::LibraryTreePageWidget(LibraryTreeGroupRegistry* groupsRegistry, SettingsManager* settings)
    : m_groupsRegistry{groupsRegistry}
    , m_settings{settings}
    , m_groupList{new QTableView(this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_playlistName{new QLineEdit(this)}
{
    m_model->populate();
    m_groupList->setModel(m_model);

    auto* delegate = new MultiLineEditDelegate(this);
    m_groupList->setItemDelegateForColumn(2, delegate);

    // Hide index column
    m_groupList->hideColumn(0);

    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttonsLayout = new QVBoxLayout();
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton(tr("Add"), this);
    auto* removeButton = new QPushButton(tr("Remove"), this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch();

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* doubleClickLabel = new QLabel(tr("Double-click: "), this);
    auto* middleClickLabel = new QLabel(tr("Middle-click: "), this);

    auto* selectionPlaylist       = new QGroupBox(tr("Library Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    auto* playlistNameLabel = new QLabel(tr("Name: "), this);

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

    using ActionIndexMap = std::map<int, int>;
    ActionIndexMap doubleActions;
    ActionIndexMap middleActions;

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action, ActionIndexMap& actionMap) {
        const int actionValue = static_cast<int>(action);
        actionMap.emplace(actionValue, box->count());
        box->addItem(text, actionValue);
    };

    addTrackAction(m_doubleClick, tr("Expand/Collapse"), TrackAction::Expand, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, doubleActions);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None, middleActions);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, middleActions);

    QObject::connect(addButton, &QPushButton::clicked, this, &LibraryTreePageWidget::addGroup);
    QObject::connect(removeButton, &QPushButton::clicked, this, &LibraryTreePageWidget::removeGroup);

    auto doubleAction = m_settings->value<Settings::Gui::LibraryTreeDoubleClick>();
    if(doubleActions.contains(doubleAction)) {
        m_doubleClick->setCurrentIndex(doubleActions.at(doubleAction));
    }

    auto middleAction = m_settings->value<Settings::Gui::LibraryTreeMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    QObject::connect(m_playlistEnabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
    });

    m_playlistEnabled->setChecked(m_settings->value<Settings::Gui::LibraryTreePlaylistEnabled>());
    m_autoSwitch->setChecked(m_settings->value<Settings::Gui::LibraryTreeAutoSwitch>());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());

    m_playlistName->setText(m_settings->value<Settings::Gui::LibraryTreeAutoPlaylist>());
}

void LibraryTreePageWidget::apply()
{
    m_model->processQueue();
    m_settings->set<Settings::Gui::LibraryTreeDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::Gui::LibraryTreeMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::Gui::LibraryTreePlaylistEnabled>(m_playlistEnabled->isChecked());
    m_settings->set<Settings::Gui::LibraryTreeAutoSwitch>(m_autoSwitch->isChecked());
    m_settings->set<Settings::Gui::LibraryTreeAutoPlaylist>(m_playlistName->text());
}

void LibraryTreePageWidget::reset()
{
    m_settings->reset<Settings::Gui::LibraryTreeGrouping>();
    m_settings->reset<Settings::Gui::LibraryTreeDoubleClick>();
    m_settings->reset<Settings::Gui::LibraryTreeMiddleClick>();
    m_settings->reset<Settings::Gui::LibraryTreePlaylistEnabled>();
    m_settings->reset<Settings::Gui::LibraryTreeAutoSwitch>();
    m_settings->reset<Settings::Gui::LibraryTreeAutoPlaylist>();

    m_groupsRegistry->loadItems();
    m_model->populate();
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

LibraryTreePage::LibraryTreePage(LibraryTreeGroupRegistry* groupsRegistry, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryTreeGeneral);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Library Tree")});
    setWidgetCreator([groupsRegistry, settings] { return new LibraryTreePageWidget(groupsRegistry, settings); });
}
} // namespace Fooyin
