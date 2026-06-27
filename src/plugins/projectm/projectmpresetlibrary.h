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

#include "projectmpreset.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace Fooyin::ProjectM {
class ProjectMPresetLibrary
{
public:
    ProjectMPresetLibrary();

    void setRootDir(const QString& rootDir);
    [[nodiscard]] QString rootDir() const;
    void setRecursive(bool recursive);
    [[nodiscard]] bool recursive() const;

    void scan();
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] int presetCount() const;

    [[nodiscard]] const std::vector<ProjectMPreset>& presets() const;

    [[nodiscard]] QString relativePath(const QString& fullPath) const;

    [[nodiscard]] static QString normalisePath(const QString& path);

private:
    [[nodiscard]] static QString normaliseRelativePath(QString path);

    QString m_rootDir;
    std::vector<ProjectMPreset> m_presets;
    bool m_recursive;
    bool m_ready;
};
} // namespace Fooyin::ProjectM
