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
 */

#pragma once

#include "tageditorfield.h"
#include "tageditoritem.h"

#include <core/stringpool.h>
#include <core/track.h>
#include <utils/worker.h>

#include <QStringList>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Fooyin::TagEditor {
constexpr auto TagEditorCompletionDomainCount = static_cast<size_t>(StringPool::Domain::ExtraTagKey) + 1;
using TagEditorCompletionValues               = std::array<QStringList, TagEditorCompletionDomainCount>;
using TagEditorTagMap                         = std::unordered_map<QString, TagEditorItem>;

struct TagEditorData
{
    uint64_t requestId{0};
    TrackList tracks;
    std::vector<TagEditorField> fields;
    TagEditorTagMap tags;
    std::vector<QString> tagOrder;
    TagEditorCompletionValues completionValues;
};

using TagEditorDataPtr = std::shared_ptr<TagEditorData>;

class TagEditorPopulator : public Worker
{
    Q_OBJECT

public:
    explicit TagEditorPopulator(QObject* parent = nullptr);

    void request(uint64_t requestId);
    void run(uint64_t requestId, const TrackList& tracks, const std::vector<TagEditorField>& fields,
             const QStringList& multiValueSeparators);

Q_SIGNALS:
    void populated(Fooyin::TagEditor::TagEditorDataPtr data);

private:
    [[nodiscard]] bool mayRunRequest(uint64_t requestId) const;

    std::atomic<uint64_t> m_latestRequest;
};
} // namespace Fooyin::TagEditor

Q_DECLARE_METATYPE(Fooyin::TagEditor::TagEditorDataPtr)
