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

#include "componentappearanceeditor.h"

#include <QPersistentModelIndex>

#include <array>
#include <memory>

class QAbstractItemView;
class QStackedWidget;
class QStyleOptionViewItem;

namespace Fooyin {
class FooyinStyle;

class ItemViewAppearanceEditor : public ComponentAppearanceEditor
{
    Q_OBJECT

public:
    explicit ItemViewAppearanceEditor(AppearancePropertyModel* propertyModel);
    ~ItemViewAppearanceEditor() override;

    void prepareItemOption(QStyleOptionViewItem& option) const;

    void selectPart(const QString& part, bool updatePreviewState) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void updatePreviewPalette(const QPalette& palette, const QFont& font) override;
    void updatePreview() override;
    void updatePreviewState(PreviewState state) override;

private:
    void applyPreviewStyle(QStyle* style);
    void refreshPreview();
    void updateActiveIndex();
    [[nodiscard]] QString activeStatePart() const;

    std::unique_ptr<FooyinStyle> m_previewStyle;
    QStackedWidget* m_views;
    std::array<QAbstractItemView*, 3> m_itemViews;
    QPersistentModelIndex m_activeIndex;
    QString m_surfaceState;
    bool m_selectedPreview;
    bool m_pickingItem;
};
} // namespace Fooyin
