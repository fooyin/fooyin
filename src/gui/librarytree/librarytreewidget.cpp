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

#include "librarytreewidget.h"

#include "internalguisettings.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"

#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <core/library/tracksort.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/tooltipfilter.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <stack>

namespace {
QModelIndexList filterAncestors(const QModelIndexList& indexes)
{
    QModelIndexList filteredIndexes;
    QModelIndexList indexesToRemove;

    for(const QModelIndex& index : indexes) {
        const QModelIndex parentIndex = index.parent();
        if(indexes.contains(parentIndex) || filteredIndexes.contains(parentIndex)) {
            for(const QModelIndex& childIndex : filteredIndexes) {
                if(childIndex.parent() == parentIndex) {
                    indexesToRemove.emplace_back(childIndex);
                }
            }
        }
        else {
            filteredIndexes.emplace_back(index);
        }
    }

    for(const QModelIndex& index : std::as_const(indexesToRemove)) {
        filteredIndexes.removeAll(index);
    }

    return filteredIndexes;
}

Fooyin::TrackList getSelectedTracks(const QTreeView* treeView, Fooyin::MusicLibrary* library)
{
    const QModelIndexList selectedIndexes = treeView->selectionModel()->selectedRows();
    if(selectedIndexes.empty()) {
        return {};
    }

    const QModelIndexList filteredIndexes = filterAncestors(selectedIndexes);

    Fooyin::TrackList tracks;
    QModelIndexList trackIndexes;

    for(const QModelIndex& index : filteredIndexes) {
        const int level = index.data(Fooyin::LibraryTreeItem::Level).toInt();
        if(level < 0) {
            trackIndexes.clear();
            tracks = library->tracks();
            break;
        }
        const auto indexTracks = index.data(Fooyin::LibraryTreeItem::Tracks).value<Fooyin::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    return tracks;
}
} // namespace

namespace Fooyin {
using namespace Settings::Gui::Internal;

struct LibraryTreeWidget::Private
{
    LibraryTreeWidget* self;

    MusicLibrary* library;
    LibraryTreeGroupRegistry groupsRegistry;
    TrackSelectionController* trackSelection;
    SettingsManager* settings;

    LibraryTreeGrouping grouping;

    QVBoxLayout* layout;
    LibraryTreeView* libraryTree;
    LibraryTreeModel* model;

    WidgetContext* widgetContext;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    QString prevSearch;
    TrackList prevSearchTracks;

    bool updating{false};

    Private(LibraryTreeWidget* self_, MusicLibrary* library_, TrackSelectionController* trackSelection_,
            SettingsManager* settings_)
        : self{self_}
        , library{library_}
        , groupsRegistry{settings_}
        , trackSelection{trackSelection_}
        , settings{settings_}
        , layout{new QVBoxLayout(self)}
        , libraryTree{new LibraryTreeView(self)}
        , model{new LibraryTreeModel(self)}
        , widgetContext{new WidgetContext(self, Context{Id{"Fooyin.Context.LibraryTree."}.append(self->id())}, self)}
        , doubleClickAction{static_cast<TrackAction>(settings->value<LibTreeDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<LibTreeMiddleClick>())}
    {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(libraryTree);

        libraryTree->setModel(model);
        libraryTree->viewport()->installEventFilter(new ToolTipFilter(self));

        libraryTree->setExpandsOnDoubleClick(doubleClickAction == TrackAction::Play);
        libraryTree->setAnimated(true);

        setScrollbarEnabled(settings->value<LibTreeScrollBar>());
        libraryTree->setAlternatingRowColors(settings->value<LibTreeAltColours>());

        changeGrouping(groupsRegistry.itemByName(QStringLiteral("")));

        model->setFont(settings->value<LibTreeFont>());
        model->setColour(settings->value<LibTreeColour>());
        model->setRowHeight(settings->value<LibTreeRowHeight>());
    }

    void reset() const
    {
        model->reset(library->tracks());
    }

    void changeGrouping(const LibraryTreeGrouping& newGrouping)
    {
        if(std::exchange(grouping, newGrouping) != newGrouping) {
            model->changeGrouping(grouping);
            reset();
        }
    }

    void addGroupMenu(QMenu* parent)
    {
        auto* groupMenu = new QMenu(QStringLiteral("Grouping"), parent);

        auto* treeGroups = new QActionGroup(groupMenu);

        const auto groups = groupsRegistry.items();
        for(const auto& group : groups) {
            auto* switchGroup = new QAction(group.name, groupMenu);
            QObject::connect(switchGroup, &QAction::triggered, self, [this, group]() { changeGrouping(group); });
            switchGroup->setCheckable(true);
            switchGroup->setChecked(grouping.id == group.id);
            groupMenu->addAction(switchGroup);
            treeGroups->addAction(switchGroup);
        }

        parent->addMenu(groupMenu);
    }

    void setScrollbarEnabled(bool enabled) const
    {
        libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    }

    void setupHeaderContextMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        addGroupMenu(menu);
        menu->popup(self->mapToGlobal(pos));
    }

    void selectionChanged() const
    {
        if(updating) {
            return;
        }

        const TrackList tracks = getSelectedTracks(libraryTree, library);
        trackSelection->changeSelectedTracks(widgetContext, tracks, playlistNameFromSelection());

        if(settings->value<LibTreePlaylistEnabled>()) {
            const QString playlistName = settings->value<LibTreeAutoPlaylist>();
            const bool autoSwitch      = settings->value<LibTreeAutoSwitch>();

            trackSelection->executeAction(TrackAction::SendNewPlaylist,
                                          autoSwitch ? PlaylistAction::Switch : PlaylistAction::None, playlistName);
        }
    }

    void searchChanged(const QString& search)
    {
        const bool reset = prevSearch.length() > search.length();
        prevSearch       = search;

        if(search.isEmpty()) {
            prevSearchTracks.clear();
            model->reset(library->tracks());
            return;
        }

        const TrackList tracksToFilter = !reset && !prevSearchTracks.empty() ? prevSearchTracks : library->tracks();

        const auto tracks = Filter::filterTracks(tracksToFilter, search);

        prevSearchTracks = tracks;
        model->reset(tracks);
    }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QString title;
        const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
        for(const auto& index : selectedIndexes) {
            if(!title.isEmpty()) {
                title.append(QStringLiteral(", "));
            }
            title.append(index.data().toString());
        }
        return title;
    }

    void handleDoubleClick() const
    {
        if(doubleClickAction != TrackAction::Play) {
            PlaylistAction::ActionOptions options;

            if(settings->value<LibTreeAutoSwitch>()) {
                options |= PlaylistAction::Switch;
            }
            if(settings->value<LibTreeSendPlayback>()) {
                options |= PlaylistAction::StartPlayback;
            }

            trackSelection->executeAction(doubleClickAction, options, playlistNameFromSelection());
        }
    }

    void handleMiddleClick() const
    {
        PlaylistAction::ActionOptions options;

        if(settings->value<LibTreeAutoSwitch>()) {
            options |= PlaylistAction::Switch;
        }
        if(settings->value<LibTreeSendPlayback>()) {
            options |= PlaylistAction::StartPlayback;
        }

        trackSelection->executeAction(middleClickAction, options, playlistNameFromSelection());
    }

    void handleTracksAdded(const TrackList& tracks) const
    {
        if(tracks.empty()) {
            return;
        }

        if(!prevSearch.isEmpty()) {
            const auto filteredTracks = Filter::filterTracks(tracks, prevSearch);
            model->addTracks(filteredTracks);
        }
        else {
            model->addTracks(tracks);
        }
    }

    void handleTracksUpdated(const TrackList& tracks)
    {
        if(tracks.empty()) {
            return;
        }

        updating = true;
        libraryTree->setUpdatesEnabled(false);

        const QModelIndexList selectedRows = libraryTree->selectionModel()->selectedRows();
        const QModelIndexList trackParents = model->indexesForTracks(tracks);

        QStringList selectedTitles;
        for(const QModelIndex& index : selectedRows) {
            selectedTitles.emplace_back(index.data(Qt::DisplayRole).toString());
        }

        std::stack<QModelIndex> checkExpanded;
        for(const QModelIndex& index : trackParents) {
            if(index.isValid()) {
                checkExpanded.emplace(index);
            }
        }

        QStringList expandedTitles;
        while(!checkExpanded.empty()) {
            const QModelIndex index = checkExpanded.top();
            checkExpanded.pop();
            if(!index.isValid()) {
                continue;
            }

            if(libraryTree->isExpanded(index)) {
                const QString title = index.data(Qt::DisplayRole).toString();
                if(!expandedTitles.contains(title)) {
                    expandedTitles.emplace_back(title);
                }
                const int rowCount = model->rowCount(index);
                for(int row{0}; row < rowCount; ++row) {
                    checkExpanded.emplace(model->index(row, 0, index));
                }
            }
        }

        model->updateTracks(tracks);

        QObject::connect(
            model, &LibraryTreeModel::modelUpdated, self,
            [this, expandedTitles, selectedTitles]() { restoreSelection(expandedTitles, selectedTitles); },
            Qt::SingleShotConnection);
    }

    void restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles)
    {
        const QModelIndexList expandedIndexes = model->findIndexes(expandedTitles);
        const QModelIndexList selectedIndexes = model->findIndexes(selectedTitles);

        for(const QModelIndex& index : expandedIndexes) {
            if(index.isValid()) {
                libraryTree->setExpanded(index, true);
            }
        }

        QItemSelection indexesToSelect;
        indexesToSelect.reserve(selectedIndexes.size());

        for(const QModelIndex& index : selectedIndexes) {
            if(index.isValid()) {
                indexesToSelect.append({index, index});
            }
        }

        libraryTree->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
        libraryTree->setUpdatesEnabled(true);
        updating = false;
    }
};

LibraryTreeWidget::LibraryTreeWidget(MusicLibrary* library, TrackSelectionController* trackSelection,
                                     SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, library, trackSelection, settings)}
{
    setObjectName(LibraryTreeWidget::name());

    setFeature(FyWidget::Search);

    QObject::connect(p->libraryTree, &LibraryTreeView::doubleClicked, this, [this]() { p->handleDoubleClick(); });
    QObject::connect(p->libraryTree, &LibraryTreeView::middleClicked, this, [this]() { p->handleMiddleClick(); });
    QObject::connect(p->libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });
    QObject::connect(p->libraryTree->header(), &QHeaderView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->setupHeaderContextMenu(pos); });
    QObject::connect(&p->groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, this,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(p->grouping.id == changedGrouping.id) {
                             p->changeGrouping(changedGrouping);
                         }
                     });

    QObject::connect(library, &MusicLibrary::tracksLoaded, this, [this]() { p->reset(); });
    QObject::connect(library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(library, &MusicLibrary::tracksScanned, p->model,
                     [this](int /*id*/, const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(
        library, &MusicLibrary::tracksUpdated, this,
        [this](const TrackList& /*oldTracks*/, const TrackList& tracks) { p->handleTracksUpdated(tracks); });
    QObject::connect(library, &MusicLibrary::tracksDeleted, p->model, &LibraryTreeModel::removeTracks);
    QObject::connect(library, &MusicLibrary::tracksSorted, this, [this]() { p->reset(); });

    settings->subscribe<LibTreeDoubleClick>(this, [this](int action) {
        p->doubleClickAction = static_cast<TrackAction>(action);
        p->libraryTree->setExpandsOnDoubleClick(p->doubleClickAction == TrackAction::Play);
    });
    settings->subscribe<LibTreeMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<LibTreeScrollBar>(this, [this](bool show) { p->setScrollbarEnabled(show); });
    settings->subscribe<LibTreeAltColours>(this,
                                           [this](bool enable) { p->libraryTree->setAlternatingRowColors(enable); });
    settings->subscribe<LibTreeFont>(this, [this](const QString& font) { p->model->setFont(font); });
    settings->subscribe<LibTreeColour>(this, [this](const QString& colour) { p->model->setColour(colour); });
    settings->subscribe<LibTreeRowHeight>(this, [this](const int height) {
        p->model->setRowHeight(height);
        QMetaObject::invokeMethod(p->libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    });
}

QString LibraryTreeWidget::name() const
{
    return QStringLiteral("Library Tree");
}

QString LibraryTreeWidget::layoutName() const
{
    return QStringLiteral("LibraryTree");
}

void LibraryTreeWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("Grouping")] = p->grouping.name;
}

void LibraryTreeWidget::loadLayoutData(const QJsonObject& layout)
{
    const LibraryTreeGrouping grouping = p->groupsRegistry.itemByName(layout[QStringLiteral("Grouping")].toString());
    if(grouping.isValid()) {
        p->changeGrouping(grouping);
    }
}

void LibraryTreeWidget::searchEvent(const QString& search)
{
    p->searchChanged(search);
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const bool hasSelection = !p->libraryTree->selectionModel()->selectedRows().empty();

    if(hasSelection) {
        p->trackSelection->addTrackPlaylistContextMenu(menu);
    }

    p->addGroupMenu(menu);

    if(hasSelection) {
        p->trackSelection->addTrackContextMenu(menu);
    }

    menu->popup(mapToGlobal(event->pos()));
}
} // namespace Fooyin

#include "moc_librarytreewidget.cpp"
