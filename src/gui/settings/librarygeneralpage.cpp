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

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

// TODO: Create model and use QTableView
namespace Gui::Settings {
struct LibraryGeneralPageWidget::Private
{
    Core::Library::LibraryManager* libraryManager;
    Core::SettingsManager* settings;

    QVBoxLayout* mainLayout;
    QHBoxLayout* libraryLayout;

    QTableView* libraryList;
    LibraryModel* model;

    QWidget* buttons;

    QVBoxLayout* buttonsLayout;
    QPushButton* addButton;
    QPushButton* removeButton;
    QPushButton* renameButton;

    QCheckBox* autoRefresh;

    struct LibraryToAdd
    {
        LibraryToAdd(QString dir, QString name)
            : dir{std::move(dir)}
            , name{std::move(name)}
        { }
        QString dir;
        QString name;
    };

    explicit Private(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings)
        : libraryManager{libraryManager}
        , settings{settings}
    { }

    void addLibrary() const
    {
        const QString newDir = QFileDialog::getExistingDirectory(libraryList, tr("Directory"), QDir::homePath(),
                                                                 QFileDialog::ShowDirsOnly);

        if(newDir.isEmpty()) {
            return;
        }

        const QFileInfo info{newDir};
        const QString name = info.fileName();

        bool success       = false;
        const QString text = QInputDialog::getText(libraryList, tr("Add Library"), tr("Library Name:"),
                                                   QLineEdit::Normal, name, &success);

        if(success && !text.isEmpty()) {
            Core::Library::LibraryInfo const info{name, newDir};
            model->markForAddition(info);
        }
    }

    void removeLibrary() const
    {
        const auto selectedItems = libraryList->selectionModel()->selectedRows();
        for(const auto& selected : selectedItems) {
            const auto* item = static_cast<LibraryItem*>(selected.internalPointer());
            model->markForRemoval(item->info());
        }
    }

    void renameLibrary() const
    {
        const auto selectedItems = libraryList->selectionModel()->selectedRows();
        for(const auto& selected : selectedItems) {
            const auto* item = static_cast<LibraryItem*>(selected.internalPointer());
            auto* info       = item->info();

            bool success       = false;
            const QString text = QInputDialog::getText(libraryList, tr("Rename Library"), tr("Library Name:"),
                                                       QLineEdit::Normal, info->name, &success);

            if(success && !text.isEmpty()) {
                info->name = text;
                model->markForRename(info);
            }
        }
    }
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager,
                                                   Core::SettingsManager* settings)
    : p{std::make_unique<Private>(libraryManager, settings)}
{
    p->libraryList = new QTableView(this);
    p->model       = new LibraryModel(p->libraryManager, this);
    p->libraryList->setModel(p->model);

    p->libraryList->verticalHeader()->hide();
    p->libraryList->horizontalHeader()->setStretchLastSection(true);
    p->libraryList->setSelectionBehavior(QAbstractItemView::SelectRows);

    p->libraryList->hideColumn(0);

    p->buttons       = new QWidget(this);
    p->buttonsLayout = new QVBoxLayout(p->buttons);
    p->addButton     = new QPushButton("Add", this);
    p->removeButton  = new QPushButton("Remove", this);
    p->renameButton  = new QPushButton("Rename", this);
    p->autoRefresh   = new QCheckBox("Auto Refresh", this);
    p->autoRefresh->setToolTip(tr("Auto refresh libraries on startup"));

    p->autoRefresh->setChecked(p->settings->value<Core::Settings::AutoRefresh>());

    p->buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    p->buttonsLayout->addWidget(p->addButton);
    p->buttonsLayout->addWidget(p->removeButton);
    p->buttonsLayout->addWidget(p->renameButton);

    p->mainLayout    = new QVBoxLayout(this);
    p->libraryLayout = new QHBoxLayout();
    setLayout(p->mainLayout);
    p->mainLayout->addLayout(p->libraryLayout);
    p->libraryLayout->addWidget(p->libraryList);
    p->libraryLayout->addWidget(p->buttons);
    p->mainLayout->addWidget(p->autoRefresh);

    connect(p->addButton, &QPushButton::clicked, this, [this]() {
        p->addLibrary();
    });
    connect(p->removeButton, &QPushButton::clicked, this, [this]() {
        p->removeLibrary();
    });
    connect(p->renameButton, &QPushButton::clicked, this, [this]() {
        p->renameLibrary();
    });
}

void LibraryGeneralPageWidget::apply()
{
    p->settings->set<Core::Settings::AutoRefresh>(p->autoRefresh->isChecked());
    p->model->processQueue();
}

LibraryGeneralPage::LibraryGeneralPage(Utils::SettingsDialogController* controller,
                                       Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings)
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
