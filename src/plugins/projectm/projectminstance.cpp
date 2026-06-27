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

#include "projectminstance.h"

#include <QDir>
#include <QFileInfo>

#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::ProjectM {
namespace {
void appendExistingDir(QStringList& dirs, const QString& dir)
{
    const QString trimmed = dir.trimmed();
    if(trimmed.isEmpty()) {
        return;
    }

    const QString path = QDir::cleanPath(trimmed);
    if(!path.isEmpty() && !dirs.contains(path) && QDir{path}.exists()) {
        dirs.append(path);
    }
}
} // namespace

ProjectMInstance::ProjectMInstance(QString dataDir, QStringList presetDirs, ProjectMSettings settings)
    : m_dataDir{std::move(dataDir)}
    , m_presetDirs{std::move(presetDirs)}
    , m_settings{settings}
    , m_projectM{projectm_create()}
    , m_playlist{nullptr}
{
    if(!m_projectM) {
        m_errorMessage = u"projectM could not create an OpenGL renderer"_s;
        return;
    }

    applySettings(m_settings);

    QStringList textureDirs;
    if(!m_dataDir.isEmpty()) {
        appendExistingDir(textureDirs, m_dataDir + u"/textures"_s);
        appendExistingDir(textureDirs, m_dataDir);
    }
    for(const QString& presetDir : std::as_const(m_presetDirs)) {
        appendExistingDir(textureDirs, presetDir + u"/Textures"_s);
        appendExistingDir(textureDirs, QFileInfo{presetDir}.dir().filePath(u"Textures"_s));
    }

    std::vector<std::string> texturePathStrings;
    texturePathStrings.reserve(textureDirs.size());
    std::vector<const char*> texturePaths;
    texturePaths.reserve(textureDirs.size());

    for(const QString& textureDir : std::as_const(textureDirs)) {
        texturePathStrings.push_back(textureDir.toStdString());
        texturePaths.push_back(texturePathStrings.back().c_str());
    }

    if(!texturePaths.empty()) {
        projectm_set_texture_search_paths(m_projectM, texturePaths.data(), texturePaths.size());
    }

    m_playlist = projectm_playlist_create(m_projectM);
    if(!m_playlist) {
        m_errorMessage = u"projectM could not create a playlist"_s;
        return;
    }

    projectm_playlist_set_retry_count(m_playlist, 0);
    projectm_playlist_set_shuffle(m_playlist, true);
    projectm_playlist_set_preset_switched_event_callback(m_playlist, &ProjectMInstance::presetSwitched, this);
    projectm_playlist_set_preset_switch_failed_event_callback(m_playlist, &ProjectMInstance::presetSwitchFailed, this);

    int existingPresetDirs{0};
    for(const QString& presetDir : std::as_const(m_presetDirs)) {
        if(!QDir{presetDir}.exists()) {
            continue;
        }

        const std::string presetDirPath = presetDir.toStdString();
        projectm_playlist_add_path(m_playlist, presetDirPath.c_str(), m_settings.scanRecursive, false);
        ++existingPresetDirs;
    }

    if(existingPresetDirs == 0) {
        m_errorMessage = u"No projectM preset folders exist"_s;
        return;
    }

    if(projectm_playlist_size(m_playlist) == 0) {
        m_errorMessage = u"No projectM presets were found in the configured folders"_s;
        return;
    }

    projectm_playlist_set_position(m_playlist, 0, true);
}

ProjectMInstance::~ProjectMInstance()
{
    if(m_playlist) {
        projectm_playlist_destroy(m_playlist);
    }
    if(m_projectM) {
        projectm_destroy(m_projectM);
    }
}

void ProjectMInstance::addPcm(const float* samples, unsigned int frameCount)
{
    if(!isReady()) {
        return;
    }

    projectm_pcm_add_float(m_projectM, samples, frameCount, PROJECTM_STEREO);
}

void ProjectMInstance::selectNext(bool hardCut)
{
    if(!isReady()) {
        return;
    }

    withShuffle(false, [this, hardCut] { projectm_playlist_play_next(m_playlist, hardCut); });
}

void ProjectMInstance::selectPrevious(bool hardCut)
{
    if(!isReady()) {
        return;
    }

    withShuffle(false, [this, hardCut] { projectm_playlist_play_previous(m_playlist, hardCut); });
}

void ProjectMInstance::selectRandom(bool hardCut)
{
    if(!isReady()) {
        return;
    }

    withShuffle(true, [this, hardCut] { projectm_playlist_play_next(m_playlist, hardCut); });
}

void ProjectMInstance::selectPreset(int index, bool hardCut)
{
    if(!isReady() || index < 0 || index >= playlistSize()) {
        return;
    }

    projectm_playlist_set_position(m_playlist, static_cast<unsigned int>(index), hardCut);
}

void ProjectMInstance::selectPreset(const QString& path, bool hardCut)
{
    if(!isReady() || path.isEmpty()) {
        return;
    }

    const int index = presetIndexForPath(path);
    if(index >= 0) {
        selectPreset(index, hardCut);
    }
}

void ProjectMInstance::setPresetLocked(bool locked)
{
    if(!isReady()) {
        return;
    }

    projectm_set_preset_locked(m_projectM, locked);
}

void ProjectMInstance::setShuffle(bool shuffle)
{
    if(!isReady()) {
        return;
    }

    projectm_playlist_set_shuffle(m_playlist, shuffle);
}

void ProjectMInstance::applySettings(const ProjectMSettings& settings)
{
    if(!m_projectM) {
        return;
    }

    m_settings = settings;
    projectm_set_mesh_size(m_projectM, static_cast<size_t>(m_settings.meshWidth),
                           static_cast<size_t>(m_settings.meshHeight));
    projectm_set_fps(m_projectM, m_settings.maxFps);
    projectm_set_aspect_correction(m_projectM, m_settings.aspectCorrection);
    projectm_set_preset_duration(m_projectM, m_settings.presetDuration);
    projectm_set_soft_cut_duration(m_projectM, m_settings.softCutDuration);
    projectm_set_hard_cut_enabled(m_projectM, m_settings.hardCutsEnabled);
    projectm_set_hard_cut_duration(m_projectM, m_settings.hardCutDuration);
    projectm_set_hard_cut_sensitivity(m_projectM, m_settings.hardCutSensitivity);
    projectm_set_beat_sensitivity(m_projectM, m_settings.beatSensitivity);
}

bool ProjectMInstance::isReady() const
{
    return m_projectM && m_playlist && m_errorMessage.isEmpty();
}

QString ProjectMInstance::errorMessage() const
{
    return m_errorMessage;
}

int ProjectMInstance::playlistSize() const
{
    if(!isReady()) {
        return 0;
    }

    return static_cast<int>(projectm_playlist_size(m_playlist));
}

int ProjectMInstance::selectedPresetIndex() const
{
    if(!isReady()) {
        return -1;
    }

    return static_cast<int>(projectm_playlist_get_position(m_playlist));
}

QString ProjectMInstance::presetName(int index) const
{
    return QFileInfo{presetPath(index)}.completeBaseName();
}

QString ProjectMInstance::presetPath(int index) const
{
    if(!isReady() || index < 0 || index >= playlistSize()) {
        return {};
    }

    char* item = projectm_playlist_item(m_playlist, static_cast<unsigned int>(index));
    if(!item) {
        return {};
    }

    const QString path = QString::fromUtf8(item);
    projectm_playlist_free_string(item);
    return path;
}

QString ProjectMInstance::selectedPresetPath() const
{
    if(!isReady()) {
        return {};
    }

    const int index = selectedPresetIndex();
    if(index < 0) {
        return {};
    }

    return presetPath(index);
}

void ProjectMInstance::resize(size_t width, size_t height)
{
    if(!isReady()) {
        return;
    }

    projectm_set_window_size(m_projectM, width, height);
}

void ProjectMInstance::renderFrame()
{
    if(!isReady()) {
        return;
    }

    projectm_opengl_render_frame(m_projectM);
}

int ProjectMInstance::presetIndexForPath(const QString& path) const
{
    if(!isReady()) {
        return -1;
    }

    const uint32_t size = projectm_playlist_size(m_playlist);
    for(uint32_t index{0}; index < size; ++index) {
        char* item = projectm_playlist_item(m_playlist, index);
        if(!item) {
            continue;
        }

        const QString itemPath = QString::fromUtf8(item);
        projectm_playlist_free_string(item);
        if(itemPath == path) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int ProjectMInstance::presetIndexForFailedPath(const QString& path) const
{
    if(!isReady() || path.isEmpty()) {
        return -1;
    }

    const QFileInfo failedInfo{path};
    const QString cleanPath     = QDir::cleanPath(path);
    const QString absolutePath  = QDir::cleanPath(failedInfo.absoluteFilePath());
    const QString fileName      = failedInfo.fileName();
    const uint32_t playlistSize = projectm_playlist_size(m_playlist);

    for(uint32_t index{0}; index < playlistSize; ++index) {
        char* item = projectm_playlist_item(m_playlist, index);
        if(!item) {
            continue;
        }

        const QString itemPath = QString::fromUtf8(item);
        projectm_playlist_free_string(item);

        const QFileInfo itemInfo{itemPath};
        if(QDir::cleanPath(itemPath) == cleanPath || QDir::cleanPath(itemInfo.absoluteFilePath()) == absolutePath
           || itemInfo.fileName() == fileName) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

void ProjectMInstance::notifyPresetChanged(int index)
{
    if(!isReady() || index < 0 || index >= playlistSize()) {
        return;
    }

    char* item = projectm_playlist_item(m_playlist, static_cast<unsigned int>(index));
    if(!item) {
        return;
    }

    const QString path = QString::fromUtf8(item);
    projectm_playlist_free_string(item);
    Q_EMIT presetChanged(path);
}

void ProjectMInstance::presetSwitched(bool /*isHardCut*/, unsigned int index, void* userData)
{
    if(auto* instance = static_cast<ProjectMInstance*>(userData)) {
        instance->notifyPresetChanged(static_cast<int>(index));
    }
}

void ProjectMInstance::presetSwitchFailed(const char* presetFilename, const char* message, void* userData)
{
    auto* instance = static_cast<ProjectMInstance*>(userData);
    if(!instance) {
        return;
    }

    const QString filename       = QString::fromUtf8(presetFilename ? presetFilename : "");
    const int index              = instance->presetIndexForFailedPath(filename);
    const QString path           = index >= 0 ? instance->presetPath(index) : filename;
    const QString failureMessage = QString::fromUtf8(message ? message : "");
    Q_EMIT instance->presetLoadFailed(index, path, failureMessage);
}
} // namespace Fooyin::ProjectM
