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

#include <gui/settings/context/staticcontextmenu.h>

#include <QApplication>
#include <QMenu>
#include <QStringList>

#include <utility>

namespace Fooyin::ContextMenuUtils {
inline bool showAllActions(Qt::KeyboardModifiers modifiers)
{
    return modifiers.testFlag(Qt::ShiftModifier);
}

inline bool showAllActions()
{
    return showAllActions(QApplication::keyboardModifiers());
}

inline bool isSectionEnabled(const QStringList& disabledSections, const char* sectionId,
                             Qt::KeyboardModifiers modifiers)
{
    return showAllActions(modifiers) || !disabledSections.contains(QLatin1StringView{sectionId});
}

template <typename Callback>
bool appendMenuSection(QMenu* menu, Callback&& callback)
{
    if(!menu) {
        return false;
    }

    const auto countBefore = static_cast<int>(menu->actions().size());
    callback();
    return menu->actions().size() != countBefore;
}

inline bool isSeparatorLayoutId(const QString& id)
{
    return id.startsWith(QStringLiteral("separator:"));
}

inline QStringList effectiveContextMenuLayout(const QStringList& defaultLayout, const QStringList& savedLayout)
{
    QStringList layout;
    layout.reserve(defaultLayout.size() + savedLayout.size());

    for(const auto& id : savedLayout) {
        if(isSeparatorLayoutId(id)) {
            layout.emplace_back(id);
        }
        else if(defaultLayout.contains(id) && !layout.contains(id)) {
            layout.emplace_back(id);
        }
    }

    for(qsizetype defaultIndex{0}; std::cmp_less(defaultIndex, defaultLayout.size()); ++defaultIndex) {
        const QString& id = defaultLayout.at(defaultIndex);
        if(layout.contains(id)) {
            continue;
        }

        qsizetype insertIndex = layout.size();

        for(qsizetype previousIndex = defaultIndex; previousIndex-- > 0;) {
            const qsizetype layoutIndex = layout.indexOf(defaultLayout.at(previousIndex));
            if(layoutIndex >= 0) {
                insertIndex = layoutIndex + 1;
                break;
            }
        }

        if(insertIndex == layout.size()) {
            for(qsizetype nextIndex = defaultIndex + 1; nextIndex < defaultLayout.size(); ++nextIndex) {
                const qsizetype layoutIndex = layout.indexOf(defaultLayout.at(nextIndex));
                if(layoutIndex >= 0) {
                    insertIndex = layoutIndex;
                    break;
                }
            }
        }

        layout.insert(insertIndex, id);
    }

    return layout;
}

template <size_t N, typename RenderSection>
void renderStaticContextMenu(QMenu* menu, const std::array<StaticContextMenu::Item, N>& items,
                             const QStringList& savedLayout, const QStringList& disabledSections,
                             RenderSection&& renderSection)
{
    if(!menu) {
        return;
    }

    const auto modifiers      = QApplication::keyboardModifiers();
    const auto sectionEnabled = [&disabledSections, modifiers](const char* sectionId) {
        return isSectionEnabled(disabledSections, sectionId, modifiers);
    };

    const QStringList orderedIds = effectiveContextMenuLayout(StaticContextMenu::defaultLayoutIds(items), savedLayout);

    renderGroupedMenu(
        menu, orderedIds,
        [&items](const auto& id) {
            return isSeparatorLayoutId(id) || StaticContextMenu::isBuiltInSeparatorId(items, id);
        },
        [&](const auto& id, QMenu* targetMenu) {
            return appendMenuSection(targetMenu, [&] { renderSection(id, targetMenu, sectionEnabled); });
        });
}

template <typename Range, typename IsBoundary, typename RenderItem>
void renderGroupedMenu(QMenu* menu, const Range& items, IsBoundary&& isBoundary, RenderItem&& renderItem)
{
    if(!menu) {
        return;
    }

    bool hasRenderedContent{false};
    bool pendingBoundary{false};

    for(const auto& item : items) {
        if(std::forward<IsBoundary>(isBoundary)(item)) {
            if(hasRenderedContent) {
                pendingBoundary = true;
            }
            continue;
        }

        if(pendingBoundary) {
            QAction* separator = menu->addSeparator();
            if(!std::forward<RenderItem>(renderItem)(item, menu)) {
                menu->removeAction(separator);
                separator->deleteLater();
                continue;
            }

            pendingBoundary    = false;
            hasRenderedContent = true;
            continue;
        }

        if(std::forward<RenderItem>(renderItem)(item, menu)) {
            hasRenderedContent = true;
        }
    }
}
} // namespace Fooyin::ContextMenuUtils
