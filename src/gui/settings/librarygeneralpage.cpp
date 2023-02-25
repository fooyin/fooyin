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
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
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

    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_libraryLayout;

    QTableView* m_libraryList;
    LibraryModel* m_model;

    QWidget* m_buttons;

    QVBoxLayout* m_buttonsLayout;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_renameButton;

    QCheckBox* m_autoRefresh;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager,
                                                   Utils::SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
    , m_libraryLayout{new QHBoxLayout()}
    , m_libraryList{new QTableView(this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_buttons{new QWidget(this)}
    , m_buttonsLayout{new QVBoxLayout(m_buttons)}
    , m_addButton{new QPushButton("Add", this)}
    , m_removeButton{new QPushButton("Remove", this)}
    , m_renameButton{new QPushButton("Rename", this)}
    , m_autoRefresh{new QCheckBox("Auto refresh on startup", this)}
{
    m_libraryList->setModel(m_model);

    m_libraryList->verticalHeader()->hide();
    m_libraryList->horizontalHeader()->setStretchLastSection(true);
    m_libraryList->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Hide Id column
    m_libraryList->hideColumn(0);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup"));
    m_autoRefresh->setChecked(m_settings->value<Core::Settings::AutoRefresh>());

    m_buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_buttonsLayout->addWidget(m_addButton);
    m_buttonsLayout->addWidget(m_removeButton);
    m_buttonsLayout->addWidget(m_renameButton);

    m_libraryLayout->addWidget(m_libraryList);
    m_libraryLayout->addWidget(m_buttons);
    m_mainLayout->addLayout(m_libraryLayout);
    m_mainLayout->addWidget(m_autoRefresh);

    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        addLibrary();
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        removeLibrary();
    });
    connect(m_renameButton, &QPushButton::clicked, this, [this]() {
        renameLibrary();
    });
}

void LibraryGeneralPageWidget::apply()
{
    m_settings->set<Core::Settings::AutoRefresh>(m_autoRefresh->isChecked());
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

} // namespace Fy::Gui::Settings
