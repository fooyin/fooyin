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

#include "scanprogresstext.h"

#include <core/library/musiclibrary.h>

#include <QCoreApplication>

namespace Fooyin::ScanProgressText {
namespace {
QString translate(const char* context, const char* sourceText, const char* disambiguation = nullptr, int n = -1)
{
    return QCoreApplication::translate(context, sourceText, disambiguation, n);
}
} // namespace

bool isCheckingForChanges(const ScanProgress& progress)
{
    return progress.type == ScanRequest::Library && progress.onlyModified
        && progress.phase == ScanProgress::Phase::ReadingMetadata;
}

QString libraryStatusPrefix(const ScanProgress& progress, const char* context)
{
    return isCheckingForChanges(progress) ? translate(context, "Checking for changes") : translate(context, "Scanning");
}

QString requestText(const ScanProgress& progress, const char* context)
{
    switch(progress.type) {
        case ScanRequest::Files:
            return translate(context, "Scanning files");
        case ScanRequest::Tracks:
            return translate(context, "Scanning tracks");
        case ScanRequest::Library:
            if(isCheckingForChanges(progress)) {
                //: %1 refers to the name of a music library.
                return translate(context, "Checking %1 for changes").arg(progress.info.name);
            }
            //: %1 refers to the name of a music library.
            return translate(context, "Scanning %1").arg(progress.info.name);
        case ScanRequest::Playlist:
            return translate(context, "Loading playlist");
    }

    return {};
}

QString phaseText(const ScanProgress& progress, const char* context)
{
    switch(progress.phase) {
        case ScanProgress::Phase::Enumerating:
            return translate(context, "discovering files");
        case ScanProgress::Phase::ReadingMetadata:
            if(isCheckingForChanges(progress)) {
                return {};
            }
            return translate(context, "reading metadata");
        case ScanProgress::Phase::WritingDatabase:
            return translate(context, "saving changes");
        case ScanProgress::Phase::Finalising:
            return translate(context, "finalising");
        case ScanProgress::Phase::Finished:
            return {};
    }

    return {};
}

QString discoveredText(int discovered, const char* context)
{
    return translate(context, "%Ln file(s)", nullptr, discovered);
}
} // namespace Fooyin::ScanProgressText
