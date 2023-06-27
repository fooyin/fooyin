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
#include "gui/librarytree/librarytreewidget.h"
#include "gui/settings/librarytree/librarytreegroupmodel.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

namespace Fy::Gui::Settings {
using ClickAction = Widgets::LibraryTreeWidget::ClickAction;

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

    QComboBox* m_middleClickBehaviour;
    QComboBox* m_doubleClickBehaviour;

    QCheckBox* m_autoplay;
    QCheckBox* m_autoSend;
};

LibraryTreePageWidget::LibraryTreePageWidget(Widgets::LibraryTreeGroupRegistry* groupsRegistry,
                                             Utils::SettingsManager* settings)
    : m_groupsRegistry{groupsRegistry}
    , m_settings{settings}
    , m_groupList{new QTableView(this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_middleClickBehaviour{new QComboBox(this)}
    , m_doubleClickBehaviour{new QComboBox(this)}
    , m_autoplay{new QCheckBox("Start playback when sending to playlist", this)}
    , m_autoSend{new QCheckBox("Auto-send selection to playlist", this)}
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

    auto* doubleClickLabel = new QLabel("Double-click behaviour", this);
    auto* middleClickLabel = new QLabel("Middle-click behaviour", this);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_groupList, 0, 0, 1, 3);
    mainLayout->addLayout(buttonsLayout, 0, 3);
    mainLayout->addWidget(doubleClickLabel, 2, 0);
    mainLayout->addWidget(m_doubleClickBehaviour, 2, 1);
    mainLayout->addWidget(middleClickLabel, 3, 0);
    mainLayout->addWidget(m_middleClickBehaviour, 3, 1);
    mainLayout->addWidget(m_autoplay, 4, 0);
    mainLayout->addWidget(m_autoSend, 5, 0);

    m_doubleClickBehaviour->addItem("Expand/Collapse", ClickAction::Expand);
    m_doubleClickBehaviour->addItem("Add to current playlist", ClickAction::AddCurrentPlaylist);
    m_doubleClickBehaviour->addItem("Send to current playlist", ClickAction::SendCurrentPlaylist);
    m_doubleClickBehaviour->addItem("Send to new playlist", ClickAction::SendNewPlaylist);

    m_middleClickBehaviour->addItem("None", ClickAction::None);
    m_middleClickBehaviour->addItem("Add to current playlist", ClickAction::AddCurrentPlaylist);
    m_middleClickBehaviour->addItem("Send to current playlist", ClickAction::SendCurrentPlaylist);
    m_middleClickBehaviour->addItem("Send to new playlist", ClickAction::SendNewPlaylist);

    mainLayout->setColumnStretch(2, 1);

    QObject::connect(addButton, &QPushButton::clicked, this, &LibraryTreePageWidget::addGroup);
    QObject::connect(removeButton, &QPushButton::clicked, this, &LibraryTreePageWidget::removeGroup);

    auto clickIndex = [](ClickType type, int action) -> int {
        switch(action) {
            case(ClickAction::None):
                return type == Middle ? 0 : -1;
            case ClickAction::Expand:
                return type == Double ? 0 : -1;
            case ClickAction::AddCurrentPlaylist:
                return 1;
            case ClickAction::SendCurrentPlaylist:
                return 2;
            case ClickAction::SendNewPlaylist:
                return 3;
            default:
                return -1;
        }
    };

    m_doubleClickBehaviour->setCurrentIndex(clickIndex(Double, m_settings->value<Settings::LibraryTreeDoubleClick>()));
    m_middleClickBehaviour->setCurrentIndex(clickIndex(Middle, m_settings->value<Settings::LibraryTreeMiddleClick>()));
    m_autoplay->setChecked(m_settings->value<Settings::LibraryTreeAutoplay>());
    m_autoSend->setChecked(m_settings->value<Settings::LibraryTreeAutoSend>());
}

void LibraryTreePageWidget::apply()
{
    m_model->processQueue();
    m_settings->set<Settings::LibraryTreeDoubleClick>(m_doubleClickBehaviour->currentData().toInt());
    m_settings->set<Settings::LibraryTreeMiddleClick>(m_middleClickBehaviour->currentData().toInt());
    m_settings->set<Settings::LibraryTreeAutoplay>(m_autoplay->isChecked());
    m_settings->set<Settings::LibraryTreeAutoSend>(m_autoSend->isChecked());
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
    setCategory("Category.Widgets");
    setCategoryName(tr("Widgets"));
    setWidgetCreator([groupsRegistry, settings] {
        return new LibraryTreePageWidget(groupsRegistry, settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Widgets);
}
} // namespace Fy::Gui::Settings
