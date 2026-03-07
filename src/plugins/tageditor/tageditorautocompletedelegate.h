/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/stringpool.h>
#include <core/track.h>

#include <QStyledItemDelegate>

#include <array>

class QStringListModel;

namespace Fooyin::TagEditor {
class TagEditorAutocompleteDelegate : public QStyledItemDelegate
{
public:
    explicit TagEditorAutocompleteDelegate(QObject* parent = nullptr);

    void setTracks(const TrackList& tracks);

    [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

private:
    struct CompletionConfig
    {
        bool enabled{false};
        bool multivalue{false};
        StringPool::Domain domain{StringPool::Domain::Artist};
    };

    [[nodiscard]] static CompletionConfig completionConfig(const QModelIndex& index);
    [[nodiscard]] QStringListModel* modelForDomain(StringPool::Domain domain) const;

    static constexpr auto DomainCount = static_cast<size_t>(StringPool::Domain::ExtraTagKey) + 1;
    std::array<QStringListModel*, DomainCount> m_models{};
};
} // namespace Fooyin::TagEditor
