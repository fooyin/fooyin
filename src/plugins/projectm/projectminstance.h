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

#include "projectmsettings.h"

#include <QObject>
#include <QStringList>

#include <projectM-4/playlist.h>
#include <projectM-4/projectM.h>

namespace Fooyin::ProjectM {
class ProjectMInstance : public QObject
{
    Q_OBJECT

public:
    ProjectMInstance(QString dataDir, QStringList presetDirs, ProjectMSettings settings);
    ~ProjectMInstance() override;

    ProjectMInstance(const ProjectMInstance&)            = delete;
    ProjectMInstance& operator=(const ProjectMInstance&) = delete;

    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString errorMessage() const;

    void addPcm(const float* samples, unsigned int frameCount);

    void selectNext(bool hardCut);
    void selectPrevious(bool hardCut);
    void selectRandom(bool hardCut);
    void selectPreset(int index, bool hardCut);
    void selectPreset(const QString& path, bool hardCut);
    void setPresetLocked(bool locked);
    void setShuffle(bool shuffle);
    void applySettings(const ProjectMSettings& settings);

    [[nodiscard]] int playlistSize() const;
    [[nodiscard]] int selectedPresetIndex() const;
    [[nodiscard]] QString presetName(int index) const;
    [[nodiscard]] QString presetPath(int index) const;
    [[nodiscard]] QString selectedPresetPath() const;

    void resize(size_t width, size_t height);

    void renderFrame();

Q_SIGNALS:
    void presetChanged(const QString& path);
    void presetLoadFailed(int index, const QString& path, const QString& message);

private:
    template <typename Function>
    void withShuffle(bool shuffle, Function function)
    {
        const bool previousShuffle = projectm_playlist_get_shuffle(m_playlist);
        projectm_playlist_set_shuffle(m_playlist, shuffle);
        function();
        projectm_playlist_set_shuffle(m_playlist, previousShuffle);
    }

    [[nodiscard]] int presetIndexForPath(const QString& path) const;
    [[nodiscard]] int presetIndexForFailedPath(const QString& path) const;
    void notifyPresetChanged(int index);

    static void presetSwitched(bool isHardCut, unsigned int index, void* userData);
    static void presetSwitchFailed(const char* presetFilename, const char* message, void* userData);

    QString m_dataDir;
    QStringList m_presetDirs;
    QString m_errorMessage;
    ProjectMSettings m_settings;
    projectm_handle m_projectM;
    projectm_playlist_handle m_playlist;
};
} // namespace Fooyin::ProjectM
