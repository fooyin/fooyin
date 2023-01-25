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

#include "settingspages.h"

#include "gui/guisettings.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/librarymanager.h>
#include <core/plugins/pluginmanager.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

namespace Gui::Settings {
GeneralPage::GeneralPage(Core::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
{
    auto* mainLayout = new QVBoxLayout(this);
    //    mainLayout->addStretch();
    mainLayout->setAlignment(Qt::AlignTop);

    auto* splitterHandles = new QCheckBox("Show Splitter Handles", this);
    splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());

    mainLayout->addWidget(splitterHandles);

    connect(splitterHandles, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SplitterHandles>(checked);
    });
}

GeneralPage::~GeneralPage() = default;

LibraryPage::LibraryPage(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings,
                         QWidget* parent)
    : QWidget{parent}
    , m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_libraryList{0, 3, this}
{
    auto libraries = m_libraryManager->allLibraries();

    m_libraryList.setHorizontalHeaderLabels({"ID", "Name", "Path"});
    m_libraryList.verticalHeader()->hide();
    m_libraryList.horizontalHeader()->setStretchLastSection(true);
    m_libraryList.setSelectionBehavior(QAbstractItemView::SelectRows);

    for(const auto& lib : libraries) {
        addLibraryRow(lib);
    }
    m_libraryList.hideColumn(0);

    auto* libraryButtons      = new QWidget(this);
    auto* libraryButtonLayout = new QVBoxLayout(libraryButtons);
    auto* addLibrary          = new QPushButton("+", this);
    auto* removeLibrary       = new QPushButton("-", this);
    auto* autoRefresh         = new QCheckBox("Auto Refresh", this);
    autoRefresh->setToolTip(tr("Auto refresh libraries on startup"));

    autoRefresh->setChecked(m_settings->value<Core::Settings::AutoRefresh>());

    libraryButtonLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    libraryButtonLayout->addWidget(addLibrary);
    libraryButtonLayout->addWidget(removeLibrary);

    auto* mainLayout    = new QVBoxLayout(this);
    auto* libraryLayout = new QHBoxLayout();
    setLayout(mainLayout);
    mainLayout->addLayout(libraryLayout);
    libraryLayout->addWidget(&m_libraryList);
    libraryLayout->addWidget(libraryButtons);
    mainLayout->addWidget(autoRefresh);
    // mainLayout->addStretch();

    connect(addLibrary, &QPushButton::clicked, this, &LibraryPage::addLibrary);
    connect(removeLibrary, &QPushButton::clicked, this, &LibraryPage::removeLibrary);

    connect(autoRefresh, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Core::Settings::AutoRefresh>(checked);
    });
}

LibraryPage::~LibraryPage() = default;

void LibraryPage::addLibraryRow(const Core::Library::LibraryInfo& info)
{
    const int row = m_libraryList.rowCount();
    m_libraryList.setRowCount(row + 1);

    auto* libId   = new QTableWidgetItem{QString::number(info.id())};
    auto* libName = new QTableWidgetItem{info.name()};
    auto* libPath = new QTableWidgetItem{info.path()};

    m_libraryList.setItem(row, 0, libId);
    m_libraryList.setItem(row, 1, libName);
    m_libraryList.setItem(row, 2, libPath);
}

void LibraryPage::addLibrary()
{
    const QString newDir
        = QFileDialog::getExistingDirectory(this, tr("Directory"), QDir::homePath(), QFileDialog::ShowDirsOnly);

    if(newDir.isEmpty()) {
        return;
    }

    const QFileInfo info{newDir};
    QString name = info.fileName();

    bool success = false;
    const QString text
        = QInputDialog::getText(this, tr("Add Library"), tr("Library Name:"), QLineEdit::Normal, name, &success);

    if(success) {
        if(!text.isEmpty()) {
            name = text;
        }
        const auto id  = m_libraryManager->addLibrary(newDir, name);
        const auto lib = m_libraryManager->libraryInfo(id);
        addLibraryRow(lib);
    }
}

void LibraryPage::removeLibrary()
{
    const auto selItems = m_libraryList.selectionModel()->selectedRows();
    for(const auto& item : selItems) {
        const int row = item.row();
        const int id  = m_libraryList.item(row, 0)->text().toInt();
        m_libraryManager->removeLibrary(id);
        m_libraryList.removeRow(row);
    }
}

PlaylistPage::PlaylistPage(Core::SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
{
    auto* groupHeaders = new QCheckBox("Enable Disc Headers", this);
    groupHeaders->setChecked(m_settings->value<Settings::DiscHeaders>());

    auto* splitDiscs = new QCheckBox("Split Discs", this);
    splitDiscs->setChecked(m_settings->value<Settings::SplitDiscs>());
    splitDiscs->setEnabled(groupHeaders->isChecked());

    auto* simpleList = new QCheckBox("Simple Playlist", this);
    simpleList->setChecked(m_settings->value<Settings::SimplePlaylist>());

    auto* altColours = new QCheckBox("Alternate Row Colours", this);
    altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(groupHeaders);

    auto* indentWidget = Utils::Widgets::indentWidget(splitDiscs, this);
    mainLayout->addWidget(indentWidget);
    mainLayout->addWidget(simpleList);
    mainLayout->addWidget(altColours);
    mainLayout->addStretch();

    connect(groupHeaders, &QCheckBox::clicked, this, [this, splitDiscs](bool checked) {
        m_settings->set<Settings::DiscHeaders>(checked);
        if(checked) {
            splitDiscs->setEnabled(checked);
        }
        else {
            splitDiscs->setChecked(checked);
            splitDiscs->setEnabled(checked);
        }
    });
    connect(splitDiscs, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SplitDiscs>(checked);
    });
    connect(simpleList, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SimplePlaylist>(checked);
    });
    connect(altColours, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::PlaylistAltColours>(checked);
    });
}

PlaylistPage::~PlaylistPage() = default;
} // namespace Gui::Settings
