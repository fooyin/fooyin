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

#pragma once

#include "radiodiscovery.h"
#include "radioguideconfig.h"
#include "radiostation.h"

#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <memory>
#include <vector>

namespace Fooyin::RadioBrowser {
class RadioBrowserController;

enum class ItemKind : uint8_t
{
    Section = 0,
    Action,
    Category,
    SavedSearch,
};

enum class Action : uint8_t
{
    NowListening = 0,
    Trending,
    Popular,
    Newest,
    Random,
    LatestSearch,
    MyStations,
};

struct RadioGuideItem : TreeItem<RadioGuideItem>
{
    using TreeItem::TreeItem;

    ItemKind kind{ItemKind::Section};
    QString name;
    QString iconName;
    int stationCount{0};
    uint8_t action{0};
    RadioCategory category;
    RadioSavedSearch savedSearch;
};

class RadioGuideModel : public TreeModel<RadioGuideItem>
{
    Q_OBJECT

public:
    enum Role : uint16_t
    {
        ItemKindRole = Qt::UserRole,
        ActionRole,
        CategoryTypeRole,
        CategoryNameRole,
        CategoryValueRole,
        SavedSearchIdRole,
        SavedSearchNameRole,
        SectionKeyRole,
    };

    explicit RadioGuideModel(RadioBrowserController* controller, QObject* parent = nullptr);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void initialiseDefaults();
    void setTagPresets(const RadioGuideTagSections& sections, bool showCountries);
    void refreshIcons();
    void setCategories(RadioCategoryType type, const RadioCategoryList& categories);
    void setSavedSearches(const RadioSavedSearchList& searches);

    [[nodiscard]] QModelIndex savedSearchesSection() const;
    [[nodiscard]] QModelIndex sectionForCategoryType(RadioCategoryType type) const;
    [[nodiscard]] RadioCategory categoryForIndex(const QModelIndex& index) const;

private:
    [[nodiscard]] RadioGuideItem* createNode(RadioGuideItem* parent);
    void clearChildren(RadioGuideItem* parent);
    void collectDescendants(RadioGuideItem* parent, std::vector<RadioGuideItem*>& descendants) const;
    void refreshIcons(const QModelIndex& parent);
    [[nodiscard]] RadioGuideItem* addSection(const QString& name);
    [[nodiscard]] RadioGuideItem* addSection(RadioGuideItem* parent, const QString& name);
    void addAction(RadioGuideItem* parent, const QString& name, Action action, const QString& iconName = {});
    void addTag(RadioGuideItem* parent, const QString& name, const QString& value);
    void addTagPresets(const RadioGuideTagSections& sections);
    void addCategory(RadioGuideItem* parent, const RadioCategory& category);
    void addSavedSearch(RadioGuideItem* parent, const RadioSavedSearch& search);
    [[nodiscard]] RadioGuideItem* ensureSavedSearchesSection();
    void removeSavedSearchesSection();
    [[nodiscard]] RadioGuideItem* sectionNodeForCategoryType(RadioCategoryType type) const;

    RadioBrowserController* m_controller;
    std::vector<std::unique_ptr<RadioGuideItem>> m_nodes;
    RadioGuideItem* m_libraryNode;
    RadioGuideItem* m_savedSearchesNode;
    RadioGuideItem* m_countriesNode;
};
} // namespace Fooyin::RadioBrowser
