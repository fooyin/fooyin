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

#include "librarysortingpage.h"

#include "core/library/sortingregistry.h"
#include "gui/guiconstants.h"
#include "sortingmodel.h"

#include <core/coresettings.h>
#include <utils/multilinedelegate.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableView>
#include <QVBoxLayout>

namespace Fooyin {
class LibrarySortingPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibrarySortingPageWidget(ActionManager* actionManager, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SortingRegistry m_sortRegistry;
    SettingsManager* m_settings;

    ExtendableTableView* m_sortList;
    SortingModel* m_model;

    QPlainTextEdit* m_sortScript;
};

LibrarySortingPageWidget::LibrarySortingPageWidget(ActionManager* actionManager, SettingsManager* settings)
    : m_sortRegistry{settings}
    , m_settings{settings}
    , m_sortList{new ExtendableTableView(actionManager, this)}
    , m_model{new SortingModel(&m_sortRegistry, this)}
    , m_sortScript{new QPlainTextEdit(this)}
{
    m_sortList->setExtendableModel(m_model);
    m_sortList->setItemDelegateForColumn(2, new MultiLineEditDelegate(this));

    // Hide index column
    m_sortList->hideColumn(0);

    m_sortList->setExtendableColumn(1);
    m_sortList->verticalHeader()->hide();
    m_sortList->horizontalHeader()->setStretchLastSection(true);
    m_sortList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_sortList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* sortScriptLabel = new QLabel(tr("Sort tracks in library by") + QStringLiteral(":"), this);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_sortList, 0, 0, 1, 2);
    mainLayout->addWidget(sortScriptLabel, 1, 0);
    mainLayout->addWidget(m_sortScript, 2, 0, 1, 2);
    mainLayout->setRowStretch(0, 1);

    auto updateButtonState = [this]() {
        const auto selection = m_sortList->selectionModel()->selectedIndexes();
        if(auto* remove = m_sortList->removeAction()) {
            remove->setDisabled(std::ranges::all_of(selection, [](const QModelIndex& index) {
                return index.data(Qt::UserRole).value<SortScript>().isDefault;
            }));
        }
    };

    updateButtonState();

    QObject::connect(m_sortList->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtonState);
}

void LibrarySortingPageWidget::load()
{
    m_model->populate();
    m_sortScript->setPlainText(m_settings->value<Settings::Core::LibrarySortScript>());
}

void LibrarySortingPageWidget::apply()
{
    m_model->processQueue();
    m_settings->set<Settings::Core::LibrarySortScript>(m_sortScript->toPlainText());
}

void LibrarySortingPageWidget::reset()
{
    m_sortRegistry.reset();
    m_settings->reset<Settings::Core::LibrarySortScript>();
}

LibrarySortingPage::LibrarySortingPage(ActionManager* actionManager, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibrarySorting);
    setName(tr("Sorting"));
    setCategory({tr("Library"), tr("Sorting")});
    setWidgetCreator([actionManager, settings] { return new LibrarySortingPageWidget(actionManager, settings); });
}
} // namespace Fooyin

#include "moc_librarysortingpage.cpp"
