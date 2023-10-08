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

#include "librarygeneralpage.h"

#include "librarymodel.h"
#include "libraryview.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/librarymanager.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class LibraryGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager, Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void addLibrary() const;
    void removeLibrary() const;

    Core::Library::LibraryManager* m_libraryManager;
    Utils::SettingsManager* m_settings;

    LibraryView* m_libraryView;
    LibraryModel* m_model;

    QCheckBox* m_autoRefresh;

    QLineEdit* m_sortScript;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager,
                                                   Utils::SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_libraryView{new LibraryView(this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_autoRefresh{new QCheckBox("Auto refresh on startup", this)}
    , m_sortScript{new QLineEdit(this)}
{
    m_model->populate();
    m_libraryView->setModel(m_model);

    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setStretchLastSection(true);
    m_libraryView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Hide Id column
    m_libraryView->hideColumn(0);

    auto* buttons       = new QWidget(this);
    auto* buttonsLayout = new QVBoxLayout(buttons);
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);

    auto* libraryLayout = new QHBoxLayout();
    libraryLayout->addWidget(m_libraryView);
    libraryLayout->addWidget(buttons);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup."));
    m_autoRefresh->setChecked(m_settings->value<Core::Settings::AutoRefresh>());

    auto* sortScriptLabel  = new QLabel("Sort tracks by:", this);
    auto* sortScriptLayout = new QHBoxLayout();
    sortScriptLayout->addWidget(sortScriptLabel);
    sortScriptLayout->addWidget(m_sortScript);
    m_sortScript->setText(m_settings->value<Core::Settings::LibrarySortScript>());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(libraryLayout);
    mainLayout->addWidget(m_autoRefresh);
    mainLayout->addLayout(sortScriptLayout);

    connect(addButton, &QPushButton::clicked, this, &LibraryGeneralPageWidget::addLibrary);
    connect(removeButton, &QPushButton::clicked, this, &LibraryGeneralPageWidget::removeLibrary);
}

void LibraryGeneralPageWidget::apply()
{
    m_settings->set<Core::Settings::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Core::Settings::LibrarySortScript>(m_sortScript->text());

    m_model->processQueue();
}

void LibraryGeneralPageWidget::reset()
{
    m_settings->reset<Core::Settings::AutoRefresh>();
    m_settings->reset<Core::Settings::LibrarySortScript>();
}

void LibraryGeneralPageWidget::addLibrary() const
{
    const QString newDir = QFileDialog::getExistingDirectory(m_libraryView, tr("Directory"), QDir::homePath(),
                                                             QFileDialog::ShowDirsOnly);

    if(newDir.isEmpty()) {
        return;
    }

    const QFileInfo info{newDir};
    const QString name = info.fileName();

    bool success       = false;
    const QString text = QInputDialog::getText(m_libraryView, tr("Add Library"), tr("Library Name:"), QLineEdit::Normal,
                                               name, &success);

    if(success && !text.isEmpty()) {
        m_model->markForAddition({text, newDir});
    }
}

void LibraryGeneralPageWidget::removeLibrary() const
{
    const auto selectedItems = m_libraryView->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = m_model->itemForIndex(selected);
        m_model->markForRemoval(item->info());
    }
    //    m_libraryList->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

LibraryGeneralPage::LibraryGeneralPage(Core::Library::LibraryManager* libraryManager, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryGeneral);
    setName(tr("General"));
    setCategory({"Library"});
    setWidgetCreator([libraryManager, settings] {
        return new LibraryGeneralPageWidget(libraryManager, settings);
    });
}

} // namespace Fy::Gui::Settings
