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

#include "fycore_export.h"

#include <core/player/playbackhistorydefs.h>
#include <core/track.h>

#include <QObject>

#include <optional>

namespace Fooyin {
struct ListeningSession
{
    uint64_t itemId{0};
    PlaybackSource source;
    Track track;
    qint64 startedAt{0};
    uint64_t listeningTimeMs{0};
};

/** Live playback observations, independent of history storage and scrobbling policy. */
class FYCORE_EXPORT PlaybackListeningTracker : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackListeningTracker(QObject* parent = nullptr);

    [[nodiscard]] std::optional<ListeningSession> currentSession() const;
    static uint64_t listeningThreshold(uint64_t durationMs, double fraction, uint64_t timeMs);

Q_SIGNALS:
    void sessionStarted(const Fooyin::ListeningSession& session);
    void metadataChanged(const Fooyin::ListeningSession& session);
    void listeningTimeChanged(uint64_t itemId, uint64_t deltaMs);
    void sessionEnded(const Fooyin::ListeningSession& session);

private:
    friend class PlayerController;
    friend class PlayerControllerPrivate;

    void start(uint64_t itemId, const Track& track);
    void updateMetadata(const Track& track);
    void addListeningTime(uint64_t deltaMs);
    void finish();

    std::optional<ListeningSession> m_session;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::ListeningSession)
