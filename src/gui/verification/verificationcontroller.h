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

#include <core/track.h>

#include <QObject>

#include <memory>

class QWidget;

namespace Fooyin {
class AudioLoader;
class NetworkAccessManager;

class VerificationController : public QObject
{
    Q_OBJECT

public:
    VerificationController(std::shared_ptr<AudioLoader> audioLoader,
                           std::shared_ptr<NetworkAccessManager> networkAccess, QWidget* parentWindow,
                           QObject* parent = nullptr);

    void verifyIntegrity(const TrackList& tracks);
    void verifyAccurateRip(const TrackList& tracks);

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    QWidget* m_parentWindow;
};
} // namespace Fooyin
