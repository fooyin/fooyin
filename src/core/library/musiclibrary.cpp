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

#include <core/library/musiclibrary.h>

#include <core/scripting/trackqueryfilter.h>
#include <utils/async.h>

#include <QStringList>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString combinedExpression(const LibraryFilterList& filters)
{
    QStringList expressions;
    expressions.reserve(static_cast<qsizetype>(filters.size()));

    for(const LibraryFilter& filter : filters) {
        expressions.push_back(u"("_s + filter.expression + u")"_s);
    }

    return expressions.join(u" AND "_s);
}
} // namespace

class MusicLibraryPrivate
{
public:
    explicit MusicLibraryPrivate(MusicLibrary* self)
        : m_self{self}
    { }

    void refreshVisibleLibraryTracks()
    {
        const uint64_t revision  = ++m_libraryFilterRevision;
        const QString expression = combinedExpression(m_activeLibraryFilters);

        Utils::asyncExec([expression, tracks = m_self->libraryTracks()]() {
            TrackQueryFilter filter;
            return filter.filter(expression, tracks);
        }).then(m_self, [this, revision](TrackList tracks) {
            if(revision != m_libraryFilterRevision) {
                return;
            }

            m_visibleLibraryTracks = std::move(tracks);
            Q_EMIT m_self->visibleLibraryTracksChanged();
        });
    }

    MusicLibrary* m_self;
    LibraryFilterList m_activeLibraryFilters;
    TrackList m_visibleLibraryTracks;
    uint64_t m_libraryFilterRevision{0};
};

MusicLibrary::MusicLibrary(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<MusicLibraryPrivate>(this)}
{
    const auto refreshActiveFilter = [this]() {
        if(hasActiveLibraryFilters()) {
            p->refreshVisibleLibraryTracks();
        }
    };

    QObject::connect(this, &MusicLibrary::tracksLoaded, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksAdded, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksMetadataChanged, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksUpdated, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksStatsChanged, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksDeleted, this, refreshActiveFilter);
    QObject::connect(this, &MusicLibrary::tracksSorted, this, refreshActiveFilter);
}

MusicLibrary::~MusicLibrary() = default;

TrackList MusicLibrary::visibleLibraryTracks() const
{
    return hasActiveLibraryFilters() ? p->m_visibleLibraryTracks : libraryTracks();
}

bool MusicLibrary::hasActiveLibraryFilters() const
{
    return !p->m_activeLibraryFilters.empty();
}

LibraryFilterList MusicLibrary::activeLibraryFilters() const
{
    return p->m_activeLibraryFilters;
}

void MusicLibrary::setActiveLibraryFilters(LibraryFilterList filters)
{
    for(LibraryFilter& filter : filters) {
        filter.expression = filter.expression.trimmed();
    }
    std::erase_if(filters, [](const LibraryFilter& filter) { return filter.expression.isEmpty(); });

    if(p->m_activeLibraryFilters == filters) {
        return;
    }

    p->m_activeLibraryFilters = std::move(filters);
    Q_EMIT activeLibraryFiltersChanged();

    if(hasActiveLibraryFilters()) {
        p->refreshVisibleLibraryTracks();
    }
    else {
        ++p->m_libraryFilterRevision;
        p->m_visibleLibraryTracks.clear();
        Q_EMIT visibleLibraryTracksChanged();
    }
}

void MusicLibrary::clearActiveLibraryFilters()
{
    setActiveLibraryFilters({});
}
} // namespace Fooyin

#include "core/library/moc_musiclibrary.cpp"
