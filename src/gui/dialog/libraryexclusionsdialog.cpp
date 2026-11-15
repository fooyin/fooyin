/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "libraryexclusionsdialog.h"

#include "core/library/librarymanager.h"
#include "core/library/libraryscanutils.h"

#include <core/library/musiclibrary.h>
#include <gui/widgets/hintlabel.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QTreeWidget* createPathTree(QWidget* parent)
{
    auto* paths = new QTreeWidget(parent);
    paths->setColumnCount(2);
    paths->setSelectionMode(QAbstractItemView::ExtendedSelection);
    paths->setRootIsDecorated(false);
    paths->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    paths->header()->setStretchLastSection(true);
    paths->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto* copy = new QAction(LibraryExclusionsDialog::tr("Copy full path"), paths);
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    paths->addAction(copy);
    QObject::connect(copy, &QAction::triggered, paths, [paths]() {
        QStringList selectedPaths;
        const auto items = paths->selectedItems();
        for(const auto* item : items) {
            const QString path = item->data(1, Qt::UserRole).toString();
            if(!path.isEmpty()) {
                selectedPaths.push_back(QDir::toNativeSeparators(path));
            }
        }
        if(!selectedPaths.empty()) {
            QApplication::clipboard()->setText(selectedPaths.join(u'\n'));
        }
    });

    return paths;
}

void addPathItem(QTreeWidgetItem* item, const QString& path, const QString& root, bool folder)
{
    QString relativePath = QDir{root}.relativeFilePath(path);
    if(folder && relativePath == "."_L1) {
        relativePath = path;
    }
    if(folder && !relativePath.endsWith(u'/')) {
        relativePath += u'/';
    }
    item->setText(1, QDir::toNativeSeparators(relativePath));
    item->setData(1, Qt::UserRole, path);
    item->setToolTip(1, QDir::toNativeSeparators(path));
    item->setData(0, Qt::UserRole, folder);
}
} // namespace

LibraryExclusionsDialog::LibraryExclusionsDialog(const LibraryInfo& libraryInfo, MusicLibrary* library,
                                                 LibraryManager* libraryManager, QWidget* parent)
    : QDialog{parent}
    , m_libraryInfo{libraryInfo}
    , m_library{library}
    , m_libraryManager{libraryManager}
    , m_exclusions{}
    , m_paths{createPathTree(this)}
{
    setWindowTitle(tr("Exclusions — %1").arg(libraryInfo.name));
    resize(680, 420);

    m_paths->setHeaderLabels({tr("Type"), tr("Path")});

    auto* addFiles  = new QPushButton(tr("Add files…"), this);
    auto* addFolder = new QPushButton(tr("Add folder…"), this);
    auto* remove    = new QPushButton(tr("Remove exclusion"), this);

    remove->setEnabled(false);

    auto* tools = new QHBoxLayout();
    tools->addWidget(addFiles);
    tools->addWidget(addFolder);
    tools->addWidget(remove);
    tools->addStretch();

    auto* description = new QLabel(tr("Excluded files and folders are skipped when scanning. "
                                      "Folder exclusions include all files and subfolders."),
                                   this);
    description->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto* layout  = new QVBoxLayout(this);
    layout->addWidget(description);
    layout->addWidget(m_paths, 1);
    layout->addLayout(tools);
    layout->addWidget(buttons);

    QObject::connect(addFiles, &QPushButton::clicked, this, [this]() {
        addPaths(QFileDialog::getOpenFileNames(this, tr("Exclude Files"), m_libraryInfo.path, {}, nullptr,
                                               QFileDialog::DontResolveSymlinks),
                 false);
    });
    QObject::connect(addFolder, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Exclude Folder"), m_libraryInfo.path,
                                                                 QFileDialog::DontResolveSymlinks);
        if(!folder.isEmpty()) {
            addPaths({folder}, true);
        }
    });
    QObject::connect(m_paths, &QTreeWidget::itemSelectionChanged, remove,
                     [this, remove]() { remove->setEnabled(!m_paths->selectedItems().empty()); });
    QObject::connect(remove, &QPushButton::clicked, this, [this]() {
        const auto items = m_paths->selectedItems();
        for(const auto* item : items) {
            auto& paths = item->data(0, Qt::UserRole).toBool() ? m_exclusions.folders : m_exclusions.files;
            paths.erase(item->data(1, Qt::UserRole).toString());
        }
        populate();
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &LibraryExclusionsDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if(const auto exclusions = m_libraryManager->exclusions(libraryInfo.id)) {
        m_exclusions = *exclusions;
    }
    else {
        description->setText(tr("Could not load library exclusions. Close this dialog and try again."));
        addFiles->setEnabled(false);
        addFolder->setEnabled(false);
        m_paths->setEnabled(false);
        buttons->button(QDialogButtonBox::Save)->setEnabled(false);
    }

    populate();
}

void LibraryExclusionsDialog::excludeTracks(const TrackList& tracks, MusicLibrary* library,
                                            LibraryManager* libraryManager, QWidget* parent, bool folders)
{
    std::map<int, std::set<QString>> excludedPaths;
    bool sharedSources{false};

    for(const auto& track : tracks) {
        if(!track.isInLibrary() || !library->libraryInfo(track.libraryId())) {
            continue;
        }
        const QString path = physicalTrackPath(track);
        excludedPaths[track.libraryId()].emplace(folders ? QFileInfo{path}.absolutePath() : path);
        sharedSources = sharedSources || track.hasCue() || track.isInArchive() || track.subsong() > 0;
    }

    if(excludedPaths.empty()) {
        return;
    }

    QDialog dialog{parent};
    dialog.setWindowTitle(folders ? tr("Exclude Folders from Library") : tr("Exclude from Library"));
    dialog.resize(680, 380);

    auto* description = new QLabel(
        folders ? tr("All files in the listed folders and their subfolders will be excluded from the library.")
                : tr("Excluded files disappear from the library and stay excluded when scanning."),
        &dialog);
    description->setWordWrap(true);

    auto* paths = createPathTree(&dialog);
    paths->setHeaderLabels({tr("Library"), folders ? tr("Folder") : tr("File")});

    for(const auto& [id, libraryPaths] : excludedPaths) {
        const auto info = library->libraryInfo(id).value();
        for(const auto& path : libraryPaths) {
            auto* item = new QTreeWidgetItem(paths);
            item->setText(0, info.name);
            addPathItem(item, path, info.path, folders);
        }
    }

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(description);
    layout->addWidget(paths, 1);

    if(sharedSources && !folders) {
        auto* hint = new HintLabel(
            tr("Excluding a source file excludes all its tracks, including CUE tracks and archive entries."), &dialog);
        layout->addWidget(hint);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto* exclude = buttons->addButton(tr("Exclude"), QDialogButtonBox::AcceptRole);
    exclude->setDefault(true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if(dialog.exec() != QDialog::Accepted) {
        return;
    }

    for(const auto& [id, libraryPaths] : excludedPaths) {
        if(const auto info = library->libraryInfo(id)) {
            auto exclusions = libraryManager->exclusions(id);
            if(!exclusions) {
                QMessageBox::warning(parent, tr("Library Exclusions"),
                                     tr("Could not load exclusions for ‘%1’.").arg(info->name));
                continue;
            }

            auto& pathsToExclude = folders ? exclusions->folders : exclusions->files;
            pathsToExclude.insert(libraryPaths.cbegin(), libraryPaths.cend());

            if(libraryManager->setExclusions(id, *exclusions)) {
                library->refresh(*info);
            }
            else {
                QMessageBox::warning(parent, tr("Library Exclusions"),
                                     tr("Could not save exclusions for ‘%1’.").arg(info->name));
            }
        }
    }
}

void LibraryExclusionsDialog::accept()
{
    const auto info = m_library->libraryInfo(m_libraryInfo.id);
    if(!info) {
        QMessageBox::information(this, tr("Library Unavailable"), tr("This library has been removed."));
        return;
    }

    const auto exclusions = m_libraryManager->exclusions(info->id);
    if(!exclusions) {
        QMessageBox::warning(this, tr("Library Exclusions"), tr("Could not load library exclusions."));
        return;
    }
    if(m_exclusions != *exclusions) {
        if(!m_libraryManager->setExclusions(info->id, m_exclusions)) {
            QMessageBox::warning(this, tr("Library Exclusions"), tr("Could not save library exclusions."));
            return;
        }
        m_library->refresh(*info);
    }

    QDialog::accept();
}

void LibraryExclusionsDialog::addPaths(const QStringList& paths, bool folders)
{
    const QDir root{m_libraryInfo.path};
    for(const auto& path : paths) {
        const QString normalisedPath = normalisePath(path);
        const QString relativePath   = root.relativeFilePath(normalisedPath);
        if(QDir::isAbsolutePath(relativePath) || relativePath == ".."_L1 || relativePath.startsWith("../"_L1)
           || relativePath == "."_L1) {
            QMessageBox::information(
                this, tr("Path Outside Library"),
                tr("Select a file or subfolder within ‘%1’.").arg(QDir::toNativeSeparators(root.path())));
            continue;
        }

        auto& exclusions = folders ? m_exclusions.folders : m_exclusions.files;
        exclusions.emplace(normalisedPath);
    }

    populate();
}

void LibraryExclusionsDialog::populate()
{
    m_paths->clear();

    const auto append = [this](const auto& paths, bool folders) {
        for(const auto& path : paths) {
            auto* item = new QTreeWidgetItem(m_paths);
            item->setText(0, folders ? tr("Folder") : tr("File"));
            addPathItem(item, path, m_libraryInfo.path, folders);
        }
    };
    append(m_exclusions.folders, true);
    append(m_exclusions.files, false);
}
} // namespace Fooyin

#include "moc_libraryexclusionsdialog.cpp"
