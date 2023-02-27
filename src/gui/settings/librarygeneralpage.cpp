/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "gui/guiconstants.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/librarymanager.h>

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

namespace Gui::Settings {
class LibraryGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager, Utils::SettingsManager* settings);

    void apply() override;

private:
    void addLibrary() const;
    void removeLibrary() const;
    void renameLibrary() const;

    Core::Library::LibraryManager* m_libraryManager;
    Utils::SettingsManager* m_settings;

    QTableView* m_libraryList;
    LibraryModel* m_model;

    QCheckBox* m_autoRefresh;
    QSpinBox* m_lazyTracksBox;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager,
                                                   Utils::SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_libraryList{new QTableView(this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_autoRefresh{new QCheckBox("Auto refresh on startup", this)}
    , m_lazyTracksBox{new QSpinBox(this)}
{
    m_libraryList->setModel(m_model);

    m_libraryList->verticalHeader()->hide();
    m_libraryList->horizontalHeader()->setStretchLastSection(true);
    m_libraryList->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Hide Id column
    m_libraryList->hideColumn(0);

    auto* buttons       = new QWidget(this);
    auto* buttonsLayout = new QVBoxLayout(buttons);
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);
    auto* renameButton = new QPushButton("Rename", this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addWidget(renameButton);

    auto* libraryLayout = new QHBoxLayout();
    libraryLayout->addWidget(m_libraryList);
    libraryLayout->addWidget(buttons);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup"));
    m_autoRefresh->setChecked(m_settings->value<Core::Settings::AutoRefresh>());

    auto* lazyTracksLabel  = new QLabel("Lazy Tracks:", this);
    auto* lazyTracksLayout = new QHBoxLayout();
    m_lazyTracksBox->setToolTip(
        tr("Load tracks from the database in groups of the number specified. \nThis can improve the startup speed "
           "of the playlist for large libraries. \nSet to 0 to turn off. \n(Default: 2500)"));
    m_lazyTracksBox->setMinimum(0);
    m_lazyTracksBox->setMaximum(100000);
    m_lazyTracksBox->setSingleStep(250);
    m_lazyTracksBox->setValue(m_settings->value<Core::Settings::LazyTracks>());
    lazyTracksLayout->addWidget(m_lazyTracksBox);
    lazyTracksLayout->addStretch();

    auto* lazySettingsLayout = new QGridLayout();
    lazySettingsLayout->addWidget(lazyTracksLabel, 1, 0);
    lazySettingsLayout->addLayout(lazyTracksLayout, 1, 1);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(libraryLayout);
    mainLayout->addWidget(m_autoRefresh);
    mainLayout->addLayout(lazySettingsLayout);

    connect(addButton, &QPushButton::clicked, this, [this]() {
        addLibrary();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        removeLibrary();
    });
    connect(renameButton, &QPushButton::clicked, this, [this]() {
        renameLibrary();
    });
}

void LibraryGeneralPageWidget::apply()
{
    m_settings->set<Core::Settings::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Core::Settings::LazyTracks>(m_lazyTracksBox->value());
    m_model->processQueue();
}

void LibraryGeneralPageWidget::addLibrary() const
{
    const QString newDir = QFileDialog::getExistingDirectory(m_libraryList, tr("Directory"), QDir::homePath(),
                                                             QFileDialog::ShowDirsOnly);

    if(newDir.isEmpty()) {
        return;
    }

    const QFileInfo info{newDir};
    const QString name = info.fileName();

    bool success       = false;
    const QString text = QInputDialog::getText(m_libraryList, tr("Add Library"), tr("Library Name:"), QLineEdit::Normal,
                                               name, &success);

    if(success && !text.isEmpty()) {
        Core::Library::LibraryInfo const info{name, newDir};
        m_model->markForAddition(info);
    }
}

void LibraryGeneralPageWidget::removeLibrary() const
{
    const auto selectedItems = m_libraryList->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = static_cast<LibraryItem*>(selected.internalPointer());
        m_model->markForRemoval(item->info());
    }
}

void LibraryGeneralPageWidget::renameLibrary() const
{
    const auto selectedItems = m_libraryList->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = static_cast<LibraryItem*>(selected.internalPointer());
        auto* info       = item->info();

        bool success       = false;
        const QString text = QInputDialog::getText(m_libraryList, tr("Rename Library"), tr("Library Name:"),
                                                   QLineEdit::Normal, info->name, &success);

        if(success && !text.isEmpty()) {
            info->name = text;
            m_model->markForRename(info);
        }
    }
}

LibraryGeneralPage::LibraryGeneralPage(Utils::SettingsDialogController* controller,
                                       Core::Library::LibraryManager* libraryManager, Utils::SettingsManager* settings)
    : Utils::SettingsPage{controller}
{
    setId(Constants::Page::LibraryGeneral);
    setName(tr("General"));
    setCategory("Category.Library");
    setCategoryName(tr("Library"));
    setWidgetCreator([libraryManager, settings] {
        return new LibraryGeneralPageWidget(libraryManager, settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Library);
}

} // namespace Gui::Settings
