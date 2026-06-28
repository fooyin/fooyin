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

#include "projectmpresetlibrary.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Fooyin::ProjectM {
ProjectMPresetLibrary::ProjectMPresetLibrary()
    : m_recursive{true}
    , m_ready{false}
{ }

void ProjectMPresetLibrary::setRootDir(const QString& rootDir)
{
    const QString path = normalisePath(rootDir);
    if(path == m_rootDir) {
        return;
    }

    m_rootDir.clear();
    m_presets.clear();
    m_ready = false;

    if(!path.isEmpty()) {
        m_rootDir = path;
    }
}

QString ProjectMPresetLibrary::rootDir() const
{
    return m_rootDir;
}

void ProjectMPresetLibrary::setRecursive(bool recursive)
{
    if(recursive == m_recursive) {
        return;
    }

    m_recursive = recursive;
    m_presets.clear();
    m_ready = false;
}

bool ProjectMPresetLibrary::recursive() const
{
    return m_recursive;
}

void ProjectMPresetLibrary::scan()
{
    m_presets.clear();
    m_ready = false;

    if(m_rootDir.isEmpty() || !QDir{m_rootDir}.exists()) {
        return;
    }

    const auto flags = m_recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator iter{m_rootDir, {u"*.milk"_s}, QDir::Files, flags};
    while(iter.hasNext()) {
        const QString path = normalisePath(iter.next());
        const QString rel  = relativePath(path);

        m_presets.push_back({.index          = -1,
                             .name           = QFileInfo{path}.completeBaseName(),
                             .path           = path,
                             .relativePath   = rel,
                             .failureMessage = {}});
    }

    std::ranges::sort(m_presets, [](const ProjectMPreset& lhs, const ProjectMPreset& rhs) {
        return QString::compare(lhs.relativePath, rhs.relativePath, Qt::CaseInsensitive) < 0;
    });

    m_ready = true;
}

bool ProjectMPresetLibrary::isReady() const
{
    return m_ready;
}

int ProjectMPresetLibrary::presetCount() const
{
    return static_cast<int>(m_presets.size());
}

const std::vector<ProjectMPreset>& ProjectMPresetLibrary::presets() const
{
    return m_presets;
}

QString ProjectMPresetLibrary::relativePath(const QString& fullPath) const
{
    QString path = normalisePath(fullPath);
    if(!m_rootDir.isEmpty() && path.startsWith(m_rootDir + '/'_L1, Qt::CaseInsensitive)) {
        path = path.mid(m_rootDir.size() + 1);
    }
    return normaliseRelativePath(path);
}

QString ProjectMPresetLibrary::normalisePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    return trimmed.isEmpty() ? QString{} : QDir::cleanPath(trimmed);
}

QString ProjectMPresetLibrary::normaliseRelativePath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    while(path.startsWith('/'_L1)) {
        path.remove(0, 1);
    }
    if(path.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(path);
}
} // namespace Fooyin::ProjectM
