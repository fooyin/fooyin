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

#include "settingspages.h"

#include "core/coresettings.h"
#include "core/library/libraryinfo.h"
#include "core/library/librarymanager.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <pluginsystem/pluginmanager.h>
#include <utils/utils.h>

namespace Core::Widgets {
GeneralPage::GeneralPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    //    mainLayout->addStretch();
    mainLayout->setAlignment(Qt::AlignTop);

    auto* settings = PluginSystem::object<SettingsManager>();

    auto* splitterHandles = new QCheckBox("Show Splitter Handles", this);
    splitterHandles->setChecked(settings->value(Settings::SplitterHandles).toBool());

    mainLayout->addWidget(splitterHandles);

    connect(splitterHandles, &QCheckBox::clicked, this, [settings](bool checked) {
        settings->set(Settings::SplitterHandles, checked);
    });
}

GeneralPage::~GeneralPage() = default;

LibraryPage::LibraryPage(Library::LibraryManager* libraryManager, QWidget* parent)
    : QWidget(parent)
    , m_libraryManager(libraryManager)
    , m_libraryList(0, 3, this)
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

    libraryButtonLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    libraryButtonLayout->addWidget(addLibrary);
    libraryButtonLayout->addWidget(removeLibrary);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(&m_libraryList);
    mainLayout->addWidget(libraryButtons);
    // mainLayout->addStretch();

    connect(addLibrary, &QPushButton::clicked, this, &LibraryPage::addLibrary);
    connect(removeLibrary, &QPushButton::clicked, this, &LibraryPage::removeLibrary);
}

LibraryPage::~LibraryPage() = default;

void LibraryPage::addLibraryRow(const Library::LibraryInfo& info)
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
    QString newDir
        = QFileDialog::getExistingDirectory(this, tr("Directory"), QDir::homePath(), QFileDialog::ShowDirsOnly);

    if(newDir.isEmpty()) {
        return;
    }

    QFileInfo info{newDir};
    QString name = info.fileName();

    bool success = false;
    QString text
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

PlaylistPage::PlaylistPage(QWidget* parent)
    : QWidget(parent)
{
    auto* settings = PluginSystem::object<SettingsManager>();

    auto* groupHeaders = new QCheckBox("Enable Disc Headers", this);
    groupHeaders->setChecked(settings->value(Settings::DiscHeaders).toBool());

    auto* splitDiscs = new QCheckBox("Split Discs", this);
    splitDiscs->setChecked(settings->value(Settings::SplitDiscs).toBool());
    splitDiscs->setEnabled(groupHeaders->isChecked());

    auto* simpleList = new QCheckBox("Simple Playlist", this);
    simpleList->setChecked(settings->value(Settings::SimplePlaylist).toBool());

    auto* altColours = new QCheckBox("Alternate Row Colours", this);
    altColours->setChecked(settings->value(Settings::PlaylistAltColours).toBool());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(groupHeaders);

    auto* indentWidget = Utils::Widgets::indentWidget(splitDiscs, this);
    mainLayout->addWidget(indentWidget);
    mainLayout->addWidget(simpleList);
    mainLayout->addWidget(altColours);
    mainLayout->addStretch();

    connect(groupHeaders, &QCheckBox::clicked, this, [settings, splitDiscs](bool checked) {
        settings->set(Settings::DiscHeaders, checked);
        if(checked) {
            splitDiscs->setEnabled(checked);
        }
        else {
            splitDiscs->setChecked(checked);
            splitDiscs->setEnabled(checked);
        }
    });
    connect(splitDiscs, &QCheckBox::clicked, this, [settings](bool checked) {
        settings->set(Settings::SplitDiscs, checked);
    });
    connect(simpleList, &QCheckBox::clicked, this, [settings](bool checked) {
        settings->set(Settings::SimplePlaylist, checked);
    });
    connect(altColours, &QCheckBox::clicked, this, [settings](bool checked) {
        settings->set(Settings::PlaylistAltColours, checked);
    });
}

PlaylistPage::~PlaylistPage() = default;
}; // namespace Core::Widgets
