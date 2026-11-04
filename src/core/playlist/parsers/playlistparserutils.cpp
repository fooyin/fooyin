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

#include "playlistparserutils.h"

#include <core/track.h>

#include <QDir>
#include <QRegularExpression>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::Utils {
QString resolveEntryPath(const QString& playlistPath, const QString& entry, const QDir& dir)
{
    if(Track::isArchivePath(entry) || Track::isRemotePath(entry) || Track::isVirtualPath(entry)) {
        return entry;
    }

    const QUrl playlistUrl{playlistPath};

    if(Track::isRemotePath(playlistUrl.toString())) {
        const QUrl resolved = playlistUrl.resolved(QUrl{entry});
        if(resolved.isValid() && !resolved.scheme().isEmpty()) {
            return resolved.toString();
        }
    }

    QString localEntry = QDir::fromNativeSeparators(entry);
    if(dir.exists() && !QDir::isAbsolutePath(localEntry)) {
        return QDir::cleanPath(dir.absoluteFilePath(localEntry));
    }

    return localEntry;
}

int takeSubsongSuffix(QString& filepath)
{
    static const QRegularExpression regex{uR"(#(\d+)$)"_s};
    const QRegularExpressionMatch match = regex.match(filepath);
    if(!match.hasMatch()) {
        return -1;
    }

    const int subsong = match.captured(1).toInt();
    filepath.remove(match.capturedStart(), match.capturedLength());
    return subsong;
}
} // namespace Fooyin::Utils
