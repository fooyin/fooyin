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

#include "radioguidemodel.h"

#include "radiogenres.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QFont>
#include <QIcon>

#include <algorithm>
#include <unordered_set>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioGuideModel::RadioGuideModel(QObject* parent)
    : TreeModel{parent}
    , m_libraryNode{nullptr}
    , m_savedSearchesNode{nullptr}
    , m_countriesNode{nullptr}
{
    initialiseDefaults();
}

QVariant RadioGuideModel::data(const QModelIndex& index, const int role) const
{
    if(!index.isValid()) {
        return {};
    }

    const RadioGuideItem* node = itemForIndex(index);
    if(!node) {
        return {};
    }

    switch(role) {
        case ItemKindRole:
            return static_cast<int>(node->kind);
        case ActionRole:
            return static_cast<int>(node->action);
        case CategoryTypeRole:
            return static_cast<int>(node->category.type);
        case CategoryNameRole:
            return node->category.name;
        case CategoryValueRole:
            return node->category.value;
        case SavedSearchIdRole:
            return node->savedSearch.id;
        case SavedSearchNameRole:
            return node->savedSearch.name;
        case SectionKeyRole:
            return static_cast<ItemKind>(node->kind) == ItemKind::Section ? node->name : QString{};
        default:
            break;
    }

    if(role == Qt::DisplayRole) {
        if(index.column() == 0) {
            return node->name;
        }
    }

    if(role == Qt::DecorationRole && index.column() == 0) {
        if(node->iconName.isEmpty()) {
            return {};
        }

        const QIcon icon = Gui::iconFromTheme(node->iconName);
        if(!icon.isNull()) {
            return icon;
        }

        return {};
    }

    if(role == Qt::FontRole && static_cast<ItemKind>(node->kind) == ItemKind::Section) {
        QFont font;
        font.setBold(true);
        return font;
    }

    return {};
}

QVariant RadioGuideModel::headerData(const int /*section*/, const Qt::Orientation /*orientation*/,
                                     const int /*role*/) const
{
    return {};
}

Qt::ItemFlags RadioGuideModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    const RadioGuideItem* node = itemForIndex(index);
    Qt::ItemFlags flags        = Qt::ItemIsEnabled;
    if(node && static_cast<ItemKind>(node->kind) != ItemKind::Section) {
        flags |= Qt::ItemIsSelectable;
    }
    return flags;
}

void RadioGuideModel::initialiseDefaults()
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();
    m_libraryNode       = nullptr;
    m_savedSearchesNode = nullptr;
    m_countriesNode     = nullptr;

    auto* selections = addSection(tr("Selections"));
    addAction(selections, tr("Popular"), Action::Popular, QString::fromLatin1(Constants::Icons::Favorite));
    addAction(selections, tr("Trending"), Action::Trending, QString::fromLatin1(Constants::Icons::Rss));
    addAction(selections, tr("Now Listening"), Action::NowListening, QString::fromLatin1(Constants::Icons::Users));
    addAction(selections, tr("Newest"), Action::Newest, QString::fromLatin1(Constants::Icons::PlaylistPlay));
    addAction(selections, tr("Random"), Action::Random, QString::fromLatin1(Constants::Icons::Randomize));

    m_libraryNode = addSection(tr("Library"));
    addAction(m_libraryNode, tr("My Stations"), Action::MyStations, QString::fromLatin1(Constants::Icons::Bookmarks));
    addAction(m_libraryNode, tr("Latest Search"), Action::LatestSearch, QString::fromLatin1(Constants::Icons::Search));

    auto* genres     = addSection(tr("Genres"));
    auto* moreGenres = addSection(tr("More Genres"));
    auto* eras       = addSection(tr("Eras"));
    auto* talk       = addSection(tr("Talk"));

    for(const RadioGenre& genre : RadioGenres::all()) {
        switch(genre.section) {
            case RadioGenreSection::Genre:
                addTag(genres, genre.name, genre.tag);
                break;
            case RadioGenreSection::MoreGenre:
                addTag(moreGenres, genre.name, genre.tag);
                break;
            case RadioGenreSection::Era:
                addTag(eras, genre.name, genre.tag);
                break;
            case RadioGenreSection::Talk:
                addTag(talk, genre.name, genre.tag);
                break;
        }
    }

    m_countriesNode = addSection(tr("Countries"));

    endResetModel();
}

void RadioGuideModel::refreshIcons()
{
    refreshIcons({});
}

void RadioGuideModel::setCategories(const RadioCategoryType type, const RadioCategoryList& categories)
{
    RadioGuideItem* section = sectionNodeForCategoryType(type);
    if(!section) {
        return;
    }

    const QModelIndex parentIndex = indexOfItem(section);
    if(section->childCount() > 0) {
        beginRemoveRows(parentIndex, 0, section->childCount() - 1);
        clearChildren(section);
        endRemoveRows();
    }

    RadioCategoryList sorted{categories};
    std::ranges::sort(sorted, [](const RadioCategory& lhs, const RadioCategory& rhs) {
        if(lhs.stationCount != rhs.stationCount) {
            return lhs.stationCount > rhs.stationCount;
        }
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    const int count = static_cast<int>(sorted.size());
    beginInsertRows(parentIndex, 0, count - 1);
    for(int i{0}; i < count; ++i) {
        addCategory(section, sorted.at(i));
    }
    endInsertRows();
}

void RadioGuideModel::setSavedSearches(const RadioSavedSearchList& searches)
{
    RadioSavedSearchList sorted = searches;
    std::ranges::sort(sorted, [](const RadioSavedSearch& lhs, const RadioSavedSearch& rhs) {
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    if(sorted.empty()) {
        removeSavedSearchesSection();
        return;
    }

    RadioGuideItem* savedSearches = ensureSavedSearchesSection();
    if(!savedSearches) {
        return;
    }

    const QModelIndex parentIndex = indexOfItem(savedSearches);
    if(savedSearches->childCount() > 0) {
        beginRemoveRows(parentIndex, 0, savedSearches->childCount() - 1);
        clearChildren(savedSearches);
        endRemoveRows();
    }

    beginInsertRows(parentIndex, 0, static_cast<int>(sorted.size()) - 1);
    for(const RadioSavedSearch& search : sorted) {
        addSavedSearch(savedSearches, search);
    }
    endInsertRows();
}

QModelIndex RadioGuideModel::sectionForCategoryType(const RadioCategoryType type) const
{
    return indexOfItem(sectionNodeForCategoryType(type));
}

QModelIndex RadioGuideModel::savedSearchesSection() const
{
    return indexOfItem(m_savedSearchesNode);
}

RadioCategory RadioGuideModel::categoryForIndex(const QModelIndex& index) const
{
    const RadioGuideItem* node = itemForIndex(index);
    return node && static_cast<ItemKind>(node->kind) == ItemKind::Category ? node->category : RadioCategory{};
}

RadioGuideItem* RadioGuideModel::createNode(RadioGuideItem* parent)
{
    auto node  = std::make_unique<RadioGuideItem>();
    auto* item = node.get();
    m_nodes.push_back(std::move(node));

    if(parent) {
        parent->appendChild(item);
    }
    return item;
}

void RadioGuideModel::clearChildren(RadioGuideItem* parent)
{
    std::vector<RadioGuideItem*> descendants;
    collectDescendants(parent, descendants);
    const std::unordered_set<RadioGuideItem*> descendantSet{descendants.cbegin(), descendants.cend()};

    parent->clearChildren();

    std::erase_if(m_nodes, [&descendantSet](const auto& node) { return descendantSet.contains(node.get()); });
}

void RadioGuideModel::collectDescendants(RadioGuideItem* parent, std::vector<RadioGuideItem*>& descendants) const
{
    if(!parent) {
        return;
    }

    for(RadioGuideItem* child : parent->children()) {
        descendants.push_back(child);
        collectDescendants(child, descendants);
    }
}

void RadioGuideModel::refreshIcons(const QModelIndex& parent)
{
    const int rows = rowCount(parent);
    if(rows <= 0) {
        return;
    }

    Q_EMIT dataChanged(index(0, 0, parent), index(rows - 1, 0, parent), {Qt::DecorationRole});

    for(int row{0}; row < rows; ++row) {
        refreshIcons(index(row, 0, parent));
    }
}

RadioGuideItem* RadioGuideModel::addSection(const QString& name)
{
    return addSection(rootItem(), name);
}

RadioGuideItem* RadioGuideModel::addSection(RadioGuideItem* parent, const QString& name)
{
    auto* node = createNode(parent);
    node->kind = ItemKind::Section;
    node->name = name;
    return node;
}

void RadioGuideModel::addAction(RadioGuideItem* parent, const QString& name, const Action action,
                                const QString& iconName)
{
    auto* node     = createNode(parent);
    node->kind     = ItemKind::Action;
    node->name     = name;
    node->iconName = iconName;
    node->action   = static_cast<uint8_t>(action);
}

void RadioGuideModel::addTag(RadioGuideItem* parent, const QString& name, const QString& value)
{
    RadioCategory category;
    category.type  = RadioCategoryType::Tag;
    category.name  = name;
    category.value = value;
    addCategory(parent, category);
}

void RadioGuideModel::addCategory(RadioGuideItem* parent, const RadioCategory& category)
{
    auto* node         = createNode(parent);
    node->kind         = ItemKind::Category;
    node->name         = category.name;
    node->iconName     = QString::fromLatin1(Constants::Icons::AudioFile);
    node->stationCount = category.stationCount;
    node->category     = category;
}

void RadioGuideModel::addSavedSearch(RadioGuideItem* parent, const RadioSavedSearch& search)
{
    auto* node        = createNode(parent);
    node->kind        = ItemKind::SavedSearch;
    node->name        = search.name;
    node->iconName    = QString::fromLatin1(Constants::Icons::Favorite);
    node->savedSearch = search;
}

RadioGuideItem* RadioGuideModel::ensureSavedSearchesSection()
{
    if(m_savedSearchesNode) {
        return m_savedSearchesNode;
    }
    if(!m_libraryNode) {
        return nullptr;
    }

    const QModelIndex libraryIndex = indexOfItem(m_libraryNode);
    const int row                  = m_libraryNode->childCount();
    beginInsertRows(libraryIndex, row, row);
    m_savedSearchesNode = addSection(m_libraryNode, tr("Saved Searches"));
    endInsertRows();
    return m_savedSearchesNode;
}

void RadioGuideModel::removeSavedSearchesSection()
{
    if(!m_savedSearchesNode) {
        return;
    }

    RadioGuideItem* parent = m_savedSearchesNode->parent();
    if(!parent) {
        return;
    }

    const int row = m_savedSearchesNode->row();
    beginRemoveRows(indexOfItem(parent), row, row);

    std::vector<RadioGuideItem*> nodesToRemove;
    nodesToRemove.push_back(m_savedSearchesNode);
    collectDescendants(m_savedSearchesNode, nodesToRemove);
    const std::unordered_set<RadioGuideItem*> nodeSet{nodesToRemove.cbegin(), nodesToRemove.cend()};

    parent->removeChild(row);
    m_savedSearchesNode = nullptr;
    std::erase_if(m_nodes, [&nodeSet](const auto& node) { return nodeSet.contains(node.get()); });

    endRemoveRows();
}

RadioGuideItem* RadioGuideModel::sectionNodeForCategoryType(const RadioCategoryType type) const
{
    switch(type) {
        case RadioCategoryType::Country:
            return m_countriesNode;
        case RadioCategoryType::Language:
        case RadioCategoryType::Tag:
        case RadioCategoryType::Codec:
            break;
    }

    return nullptr;
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioguidemodel.cpp"
