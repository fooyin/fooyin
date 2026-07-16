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

#include <core/engine/conversion/conversionpathresolver.h>

#include <core/scripting/scriptparser.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <map>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString normaliseExtension(QString extension)
{
    extension = extension.trimmed();
    while(extension.startsWith(u'.')) {
        extension.remove(0, 1);
    }
    return extension.toLower();
}

QString sanitisePathSegment(QString segment)
{
    static const QRegularExpression invalidChars{uR"([<>:"|?*\x00-\x1f])"_s};

    segment = segment.trimmed();
    segment.replace(u'\\', u'/');
    segment.replace(invalidChars, u"_"_s);

    const QStringList parts = segment.split(u'/', Qt::SkipEmptyParts);
    QStringList cleanParts;
    cleanParts.reserve(parts.size());

    for(QString part : parts) {
        part = part.trimmed();
        while(part.endsWith(u'.') || part.endsWith(u' ')) {
            part.chop(1);
        }
        if(part.isEmpty() || part == u"."_s || part == u".."_s) {
            continue;
        }
        cleanParts.push_back(part);
    }

    return cleanParts.join(u'/');
}

QString fallbackFilename(const Track& track)
{
    QString filename = track.effectiveTitle().trimmed();
    if(filename.isEmpty()) {
        filename = track.filename().trimmed();
    }
    return filename;
}

QString appendExtension(QString path, const QString& extension)
{
    const QString cleanExtension = normaliseExtension(extension);
    if(cleanExtension.isEmpty()) {
        return path;
    }

    if(path.endsWith(u'.' + cleanExtension, Qt::CaseInsensitive)) {
        return path;
    }

    return path + u'.' + cleanExtension;
}

QString destinationFolder(const Track& track, const ConversionDestination& destination, const QString& askFolder)
{
    switch(destination.mode) {
        case DestinationMode::Ask:
            return QDir::cleanPath(askFolder);
        case DestinationMode::SourceFolder:
            return QDir::cleanPath(QFileInfo{track.filepath()}.absolutePath());
        case DestinationMode::FixedFolder:
            return QDir::cleanPath(destination.fixedFolder);
    }

    return {};
}
} // namespace

std::vector<ConversionPathResult> ConversionPathResolver::resolve(const Request& request)
{
    if(request.tracks.empty()) {
        return {};
    }

    std::vector<ConversionPathResult> results;

    if(request.destination.outputStyle == OutputStyle::MergeTracks) {
        const Track& track = request.tracks.front();
        results.push_back({
            .track      = track,
            .tracks     = request.tracks,
            .outputPath = previewPath(track, request.destination, request.extension, request.askFolder),
            .status     = ConversionPathStatus::Error,
            .error      = {},
        });
    }
    else if(request.destination.outputStyle == OutputStyle::MultiTrackFiles) {
        std::map<QString, size_t> groupIndices;
        for(const Track& track : request.tracks) {
            const QString outputPath = previewPath(track, request.destination, request.extension, request.askFolder);
            const QString key        = Utils::File::cleanPath(outputPath).toCaseFolded();
            if(const auto it = groupIndices.find(key); it != groupIndices.end()) {
                results[it->second].tracks.push_back(track);
                continue;
            }
            groupIndices.emplace(key, results.size());
            results.push_back({
                .track      = track,
                .tracks     = {track},
                .outputPath = outputPath,
                .status     = ConversionPathStatus::Error,
                .error      = {},
            });
        }
    }
    else {
        std::set<QString> seenPaths;
        results.reserve(request.tracks.size());
        for(const Track& track : request.tracks) {
            const QString outputPath = previewPath(track, request.destination, request.extension, request.askFolder);
            const QString key        = Utils::File::cleanPath(outputPath).toCaseFolded();
            ConversionPathResult result{
                .track      = track,
                .tracks     = {track},
                .outputPath = outputPath,
                .status     = ConversionPathStatus::Error,
                .error      = {},
            };
            if(seenPaths.contains(key)) {
                result.status = ConversionPathStatus::Error;
                result.error  = tr("Duplicate output path");
            }
            else {
                seenPaths.emplace(key);
            }
            results.push_back(std::move(result));
        }
    }

    for(ConversionPathResult& result : results) {
        if(result.status == ConversionPathStatus::Error && !result.error.isEmpty()) {
            continue;
        }
        if(result.outputPath.isEmpty()) {
            result.status = ConversionPathStatus::Error;
            result.error  = tr("Could not resolve output path");
            continue;
        }

        if(const QFileInfo info{result.outputPath}; !info.exists()) {
            result.status = ConversionPathStatus::Ready;
            continue;
        }

        switch(request.destination.existingFileMode) {
            case ExistingFileMode::Skip:
                result.status = ConversionPathStatus::Skipped;
                break;
            case ExistingFileMode::Overwrite:
                result.status = ConversionPathStatus::Ready;
                break;
            case ExistingFileMode::Ask:
                result.status = ConversionPathStatus::NeedsOverwriteDecision;
                break;
        }
    }

    return results;
}

QString ConversionPathResolver::previewPath(const Track& track, const ConversionDestination& destination,
                                            const QString& extension, const QString& askFolder)
{
    const QString folder = destinationFolder(track, destination, askFolder);
    if(folder.isEmpty()) {
        return {};
    }

    ScriptParser parser;
    QString filename = parser.evaluate(destination.filenamePattern, track).trimmed();
    filename         = sanitisePathSegment(filename);
    if(filename.isEmpty()) {
        filename = sanitisePathSegment(fallbackFilename(track));
    }

    return QDir::cleanPath(QDir{folder}.filePath(appendExtension(filename, extension)));
}
} // namespace Fooyin
