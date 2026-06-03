/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "settingsmodel.h"

#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>
#include <utils/stringcollator.h>

#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
[[nodiscard]] int positionIndex(SettingsPagePosition position)
{
    switch(position) {
        case SettingsPagePosition::First:
            return 0;
        case SettingsPagePosition::Default:
            return 1;
        case SettingsPagePosition::Last:
            return 2;
    }
    return 1;
}

[[nodiscard]] SettingsPagePosition combinePosition(SettingsPagePosition current, SettingsPagePosition next)
{
    if(current == SettingsPagePosition::Last || next == SettingsPagePosition::Last) {
        return SettingsPagePosition::Last;
    }
    if(current == SettingsPagePosition::First || next == SettingsPagePosition::First) {
        return SettingsPagePosition::First;
    }
    return SettingsPagePosition::Default;
}

[[nodiscard]] bool hasRelativePosition(const SettingsPage* page)
{
    return page->positionPage().isValid()
        && (page->relativePosition() == SettingsPageRelativePosition::Before
            || page->relativePosition() == SettingsPageRelativePosition::After);
}

void movePage(PageList& pages, PageList::iterator page, PageList::iterator anchor,
              SettingsPageRelativePosition position)
{
    auto* movedPage{*page};
    const auto pageIndex  = static_cast<size_t>(std::distance(pages.begin(), page));
    auto anchorIndex      = static_cast<size_t>(std::distance(pages.begin(), anchor));
    const bool placeAfter = (position == SettingsPageRelativePosition::After);

    pages.erase(page);

    if(pageIndex < anchorIndex) {
        --anchorIndex;
    }
    if(placeAfter) {
        ++anchorIndex;
    }

    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(anchorIndex), movedPage);
}

[[nodiscard]] bool categoryContainsPage(const SettingsItem* item, const Id& page)
{
    if(const auto* category = item->data()) {
        if(category->findPageById(page) >= 0) {
            return true;
        }
    }

    for(int i{0}; i < item->childCount(); ++i) {
        if(categoryContainsPage(item->child(i), page)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] const SettingsPage* relativePageForCategory(const SettingsItem* item)
{
    if(const auto* category = item->data()) {
        const auto it = std::ranges::find_if(category->pages, hasRelativePosition);
        if(it != category->pages.cend()) {
            return *it;
        }
    }

    for(int i{0}; i < item->childCount(); ++i) {
        if(const auto* page = relativePageForCategory(item->child(i))) {
            return page;
        }
    }

    return nullptr;
}

[[nodiscard]] QStringList categoryIdParts(const SettingsPage* page, int count)
{
    const QString pageId         = page->id().name();
    static constexpr auto Prefix = "Fooyin.Page."_L1;

    if(!pageId.startsWith(Prefix)) {
        return {};
    }

    QStringList parts = pageId.sliced(Prefix.length()).split('.'_L1, Qt::SkipEmptyParts);
    if(parts.size() > count) {
        parts.erase(parts.begin() + count, parts.end());
    }
    return parts;
}

[[nodiscard]] Id categoryIdForPage(const SettingsPage* page, const QStringList& categories, int depth)
{
    QString id                = u"Fooyin.Category"_s;
    const QStringList idParts = categoryIdParts(page, depth + 1);

    for(int i{0}; i <= depth; ++i) {
        id += u'.';
        id += i < idParts.size() ? idParts.at(i) : categories.at(i);
    }

    return Id{id};
}

void sortPages(PageList& pages)
{
    std::ranges::stable_sort(pages, [](const SettingsPage* lhs, const SettingsPage* rhs) {
        return positionIndex(lhs->position()) < positionIndex(rhs->position());
    });

    for(auto it = pages.begin(); it != pages.end(); ++it) {
        auto* page{*it};
        if(!hasRelativePosition(page)) {
            continue;
        }

        auto anchor = std::ranges::find(pages, page->positionPage(), &SettingsPage::id);
        if(anchor == pages.end() || anchor == it) {
            continue;
        }

        movePage(pages, it, anchor, page->relativePosition());
        it = std::ranges::find(pages, page);
    }
}

void sortRelativeCategories(std::vector<SettingsItem*>& categories)
{
    struct RelativeCategory
    {
        SettingsItem* item;
        SettingsItem* anchor;
        SettingsPageRelativePosition position;
    };

    std::vector<RelativeCategory> relativeCategories;

    for(auto* item : categories) {
        const auto* page = relativePageForCategory(item);
        if(!page || categoryContainsPage(item, page->positionPage())) {
            continue;
        }

        auto anchor = std::ranges::find_if(categories, [page](const SettingsItem* category) {
            return categoryContainsPage(category, page->positionPage());
        });
        if(anchor != categories.end() && *anchor != item) {
            relativeCategories.push_back({item, *anchor, page->relativePosition()});
        }
    }

    for(const auto& relative : relativeCategories) {
        categories.erase(std::ranges::find(categories, relative.item));
    }

    const auto anchors{categories};
    for(auto* anchor : anchors) {
        if(!Utils::contains(categories, anchor)) {
            continue;
        }

        auto before = std::views::filter(relativeCategories, [anchor](const RelativeCategory& relative) {
            return relative.anchor == anchor && relative.position == SettingsPageRelativePosition::Before;
        });
        auto after  = std::views::filter(relativeCategories, [anchor](const RelativeCategory& relative) {
            return relative.anchor == anchor && relative.position == SettingsPageRelativePosition::After;
        });

        for(const auto& relative : before) {
            categories.insert(std::ranges::find(categories, anchor), relative.item);
        }

        auto afterPosition = std::ranges::find(categories, anchor) + 1;
        for(const auto& relative : after) {
            afterPosition = categories.insert(afterPosition, relative.item) + 1;
        }
    }
}
} // namespace

SettingsItem::SettingsItem()
    : SettingsItem{nullptr, nullptr}
{ }

SettingsItem::SettingsItem(SettingsCategory* data, SettingsItem* parent)
    : TreeItem{parent}
    , m_data{data}
{ }

bool SettingsItem::operator<(const SettingsItem& other) const
{
    return m_data->index < other.m_data->index;
}

SettingsCategory* SettingsItem::data() const
{
    return m_data;
}

void SettingsItem::sort()
{
    for(SettingsItem* child : m_children) {
        child->sort();
        child->resetRow();
    }

    StringCollator collator;

    std::ranges::sort(m_children, [this, &collator](const SettingsItem* lhs, const SettingsItem* rhs) {
        const int lhsPosition = positionIndex(lhs->m_data->position);
        const int rhsPosition = positionIndex(rhs->m_data->position);
        if(lhsPosition != rhsPosition) {
            return lhsPosition < rhsPosition;
        }

        if(!m_data && lhs->m_data->order != rhs->m_data->order) {
            return lhs->m_data->order < rhs->m_data->order;
        }

        const auto cmp = collator.compare(lhs->m_data->name, rhs->m_data->name);
        if(cmp == 0) {
            return false;
        }
        return cmp < 0;
    });

    sortRelativeCategories(m_children);
    resetChildren();
}

SettingsModel::SettingsModel(QObject* parent)
    : TreeModel{parent}
{ }

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item       = static_cast<SettingsItem*>(index.internalPointer());
    SettingsCategory* data = item->data();

    switch(role) {
        case(Qt::DisplayRole):
            return data->name;
        case(SettingsItem::Data):
            return QVariant::fromValue(data);
        default:
            break;
    }

    return {};
}

void SettingsModel::setPages(const PageList& pages)
{
    beginResetModel();

    resetRoot();
    m_categories.clear();
    m_items.clear();
    m_pageIds.clear();

    int categoryOrder{0};

    for(const auto& page : pages) {
        if(m_pageIds.contains(page->id())) {
            qCWarning(SETTINGS) << "Duplicate settings page:" << page->id().name();
            continue;
        }

        m_pageIds.insert(page->id());
        SettingsItem* parent         = rootItem();
        const QStringList categories = page->category();
        Id categoryId;
        SettingsCategory* category = nullptr;

        for(int depth{0}; depth < categories.size(); ++depth) {
            const QString& categoryName = categories.at(depth);
            categoryId                  = categoryIdForPage(page, categories, depth);

            if(!m_items.contains(categoryId)) {
                m_categories.emplace(
                    categoryId, SettingsCategory{.id = categoryId, .name = categoryName, .order = categoryOrder++});
                category = &m_categories.at(categoryId);
                m_items.emplace(categoryId, SettingsItem{category, parent});
                auto* item = &m_items.at(categoryId);
                parent->appendChild(item);
            }

            category           = &m_categories.at(categoryId);
            category->position = combinePosition(category->position, page->position());
            auto* item         = &m_items.at(categoryId);
            parent             = item;
        }

        if(category) {
            category->pages.emplace_back(page);
        }
    }

    for(auto& category : m_categories | std::views::values) {
        sortPages(category.pages);
    }

    rootItem()->sort();
    endResetModel();
}

SettingsCategory* SettingsModel::categoryForPage(const Id& page)
{
    for(auto& [_, category] : m_categories) {
        const int pageIndex = category.findPageById(page);
        if(pageIndex >= 0) {
            return &category;
        }
    }
    return nullptr;
}

QModelIndex SettingsModel::indexForCategory(const Id& categoryId) const
{
    for(const auto& [_, category] : m_items) {
        if(category.data()->id == categoryId) {
            return createIndex(category.row(), 0, &category);
        }
    }
    return {};
}
} // namespace Fooyin
