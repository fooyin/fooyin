/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcolumnpage.h"

#include "playlist/playlistcolumnregistry.h"
#include "playlistcolumnmodel.h"

#include <gui/guiconstants.h>
#include <utils/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QHeaderView>
#include <QVBoxLayout>

namespace Fooyin {
class PlaylistColumnPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistColumnPageWidget(ActionManager* actionManager, PlaylistColumnRegistry* columnRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    ActionManager* m_actionManager;
    PlaylistColumnRegistry* m_columnsRegistry;

    ExtendableTableView* m_columnList;
    PlaylistColumnModel* m_model;
};

PlaylistColumnPageWidget::PlaylistColumnPageWidget(ActionManager* actionManager, PlaylistColumnRegistry* columnRegistry)
    : m_actionManager{actionManager}
    , m_columnsRegistry{columnRegistry}
    , m_columnList{new ExtendableTableView(m_actionManager, this)}
    , m_model{new PlaylistColumnModel(m_columnsRegistry, this)}
{
    m_columnList->setExtendableModel(m_model);

    // Hide index column
    m_columnList->hideColumn(0);

    m_columnList->setExtendableColumn(1);
    m_columnList->verticalHeader()->hide();
    m_columnList->horizontalHeader()->setStretchLastSection(true);
    m_columnList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_columnList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto updateButtonState = [this]() {
        const auto selection = m_columnList->selectionModel()->selectedIndexes();
        if(auto* remove = m_columnList->removeAction()) {
            remove->setDisabled(std::ranges::all_of(selection, [](const QModelIndex& index) {
                return index.data(Qt::UserRole).value<PlaylistColumn>().isDefault;
            }));
        }
    };

    updateButtonState();

    QObject::connect(m_columnList->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtonState);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_columnList);
}

void PlaylistColumnPageWidget::load()
{
    m_model->populate();
}

void PlaylistColumnPageWidget::apply()
{
    m_model->processQueue();
}

void PlaylistColumnPageWidget::reset()
{
    m_columnsRegistry->reset();
}

PlaylistColumnPage::PlaylistColumnPage(ActionManager* actionManager, PlaylistColumnRegistry* columnRegistry,
                                       SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistColumns);
    setName(tr("Columns"));
    setCategory({tr("Playlist"), tr("Columns")});
    setWidgetCreator(
        [actionManager, columnRegistry] { return new PlaylistColumnPageWidget(actionManager, columnRegistry); });
}
} // namespace Fooyin

#include "moc_playlistcolumnpage.cpp"
#include "playlistcolumnpage.moc"
