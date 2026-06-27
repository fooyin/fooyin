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
#include "projectmsettings.h"

#include <core/engine/visualisationservice.h>

#include <QBasicTimer>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLWindow>
#include <QSize>
#include <QStringList>

#include <deque>
#include <functional>
#include <map>
#include <unordered_map>

namespace Fooyin::ProjectM {
class ProjectMInstance;

class ProjectMView : public QOpenGLWindow,
                     protected QOpenGLFunctions_2_1
{
    Q_OBJECT

public:
    ProjectMView();
    ~ProjectMView() override;

    void setVisualisationSession(VisualisationSessionPtr session);
    void setPresetDirs(const QStringList& presetDirs);

    [[nodiscard]] static QStringList defaultPresetDirs();

    [[nodiscard]] bool isReady() const;
    [[nodiscard]] bool hasInitialised() const;
    [[nodiscard]] bool isRecreating() const;

    [[nodiscard]] QString statusText() const;

    void shutdown();

    void selectNextPreset();
    void selectPreviousPreset();
    void selectRandomPreset();
    void selectPreset(int index);
    void selectPreset(const QString& path);
    void setPresetLocked(bool locked);
    void setShuffle(bool shuffle);
    void applySettings(const ProjectMSettings& settings);

    [[nodiscard]] int selectedPresetIndex();
    [[nodiscard]] QString selectedPresetPath();
    [[nodiscard]] bool presetLocked() const;
    [[nodiscard]] bool shuffle() const;
    [[nodiscard]] const ProjectMSettings& settings() const;
    [[nodiscard]] QString presetFailureMessage(const QString& path) const;

    [[nodiscard]] std::vector<ProjectMPreset> presets();

Q_SIGNALS:
    void activated();
    void availabilityChanged(bool ready, const QString& statusText);
    void contextMenuRequested(const QPoint& pos);
    void doubleClicked();
    void presetChanged(const QString& path);
    void presetLoadFailed(int index, const QString& path, const QString& message);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void exposeEvent(QExposeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct ProjectMPaths
    {
        QString dataDir;

        [[nodiscard]] bool isValid() const
        {
            return !dataDir.isEmpty();
        }
    };

    [[nodiscard]] static QStringList dataDirCandidates();
    ProjectMPaths resolveProjectMPaths();
    void createProjectM();

    [[nodiscard]] QSize framebufferSize(int width, int height) const;
    void resizeRenderer(int width, int height, bool force = false);
    void feedPendingPcm();
    void feedPcmWindow(const VisualisationSession::PcmWindow& window);
    void resetPcmCursor();
    void queueGLOperation(std::function<void()> operation);
    void processQueuedGLOperations();
    void scheduleProjectMRecreation();

    void setUnavailable(const QString& statusText);
    void emitAvailability();
    void updateRenderTimer();

    std::unique_ptr<ProjectMInstance> m_projectM;
    VisualisationSessionPtr m_visualisationSession;
    ProjectMPaths m_paths;
    VisualisationSession::PcmWindow m_pcmWindow;
    QSize m_rendererSize;
    std::unordered_map<int, QString> m_failedPresets;
    std::map<QString, QString> m_failedPresetPaths;
    std::vector<float> m_stereoBuffer;
    std::deque<std::function<void()>> m_pendingGLOperations;
    QBasicTimer m_renderTimer;
    QString m_statusText;
    QString m_pendingPresetPath;
    QStringList m_presetDirs;
    uint64_t m_lastPcmTimeMs;
    bool m_ready;
    bool m_initialised;
    bool m_recreateProjectM;
    bool m_hasPcmCursor;
    bool m_presetLocked;
    bool m_shuffle;
    ProjectMSettings m_settings;
};
} // namespace Fooyin::ProjectM
