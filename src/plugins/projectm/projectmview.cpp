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

#include "projectmview.h"

#include "projectminstance.h"

#include <gui/framerate.h>

#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QtGlobal>

#include <algorithm>

Q_LOGGING_CATEGORY(PROJECTM_GL, "fooyin.projectm")

using namespace Qt::StringLiterals;

constexpr uint64_t InitialPcmWindowMs = 50;
constexpr uint64_t MaxPcmCatchupMs    = 1000;
constexpr uint64_t PcmBacklogMs       = 5000;
constexpr uint32_t DebugRenderFrames  = 5;
constexpr uint32_t DebugPcmWindows    = 5;

namespace Fooyin::ProjectM {
namespace {
QStringList normaliseDirs(const QStringList& dirs)
{
    QStringList normalised;
    for(const QString& dir : dirs) {
        const QString trimmed = dir.trimmed();
        if(trimmed.isEmpty()) {
            continue;
        }
        const QString path = QDir::cleanPath(trimmed);
        if(!path.isEmpty() && !normalised.contains(path)) {
            normalised.append(path);
        }
    }
    return normalised;
}

class CurrentProjectMContext
{
public:
    explicit CurrentProjectMContext(ProjectMView* view)
        : m_view{view}
        , m_restorePrevious{view->context() && QOpenGLContext::currentContext() != view->context()}
    {
        if(m_restorePrevious) {
            m_view->makeCurrent();
        }
    }

    ~CurrentProjectMContext()
    {
        if(m_restorePrevious) {
            m_view->doneCurrent();
        }
    }

    [[nodiscard]] bool isCurrent() const
    {
        return m_view->context() && QOpenGLContext::currentContext() == m_view->context();
    }

private:
    ProjectMView* m_view;
    bool m_restorePrevious;
};
} // namespace

ProjectMView::ProjectMView()
    : QOpenGLWindow{NoPartialUpdate}
    , m_statusText{tr("Initialising projectM…")}
    , m_lastPcmTimeMs{0}
    , m_ready{false}
    , m_initialised{false}
    , m_recreateProjectM{false}
    , m_hasPcmCursor{false}
    , m_playbackIdle{false}
    , m_idlePresetActive{false}
    , m_presetLocked{false}
    , m_shuffle{false}
{ }

ProjectMView::~ProjectMView()
{
    shutdown();
}

void ProjectMView::setVisualisationSession(VisualisationSessionPtr session)
{
    m_visualisationSession = std::move(session);
    if(m_visualisationSession) {
        m_visualisationSession->requestBacklog(PcmBacklogMs);
        m_visualisationSession->setPcmTargetSampleRate(44100);
    }
    resetPcmCursor();
}

void ProjectMView::setPresetDirs(const QStringList& presetDirs)
{
    const QStringList normalised = normaliseDirs(presetDirs);
    if(normalised == m_presetDirs) {
        return;
    }

    m_presetDirs = normalised;
    if(!m_initialised) {
        return;
    }

    scheduleProjectMRecreation();
}

QStringList ProjectMView::defaultPresetDirs()
{
    QStringList dirs;

    const QStringList candidates = dataDirCandidates();
    for(const QString& dataDir : candidates) {
        const QString presetDir = dataDir + u"/presets"_s;
        if(QDir{presetDir}.exists()) {
            dirs.append(presetDir);
        }
    }
    return normaliseDirs(dirs);
}

bool ProjectMView::isReady() const
{
    return m_ready && m_projectM;
}

bool ProjectMView::hasInitialised() const
{
    return m_initialised;
}

bool ProjectMView::isRecreating() const
{
    return m_recreateProjectM;
}

QString ProjectMView::statusText() const
{
    return m_statusText;
}

void ProjectMView::shutdown()
{
    m_renderTimer.stop();
    m_pendingGLOperations.clear();
    m_visualisationSession.reset();
    m_ready = false;
    resetPcmCursor();

    if(!m_projectM && !m_debugLogger) {
        return;
    }

    if(context()) {
        makeCurrent();
        m_projectM.reset();
        if(m_debugLogger) {
            m_debugLogger->stopLogging();
            m_debugLogger.reset();
        }
        doneCurrent();
    }
    else {
        m_projectM.reset();
        m_debugLogger.reset();
    }
}

void ProjectMView::selectNextPreset()
{
    if(isReady()) {
        queueGLOperation([this]() {
            if(isReady()) {
                leaveIdleForUserSelection();
                m_projectM->selectNext(true);
                m_projectM->setShuffle(m_shuffle);
            }
        });
    }
}

void ProjectMView::selectPreviousPreset()
{
    if(isReady()) {
        queueGLOperation([this]() {
            if(isReady()) {
                leaveIdleForUserSelection();
                m_projectM->selectPrevious(true);
                m_projectM->setShuffle(m_shuffle);
            }
        });
    }
}

void ProjectMView::selectRandomPreset()
{
    if(isReady()) {
        queueGLOperation([this]() {
            if(isReady()) {
                leaveIdleForUserSelection();
                m_projectM->selectRandom(true);
            }
        });
    }
}

void ProjectMView::selectPreset(int index)
{
    if(isReady() && index >= 0) {
        queueGLOperation([this, index]() {
            if(isReady() && index >= 0 && index < m_projectM->playlistSize()) {
                leaveIdleForUserSelection();
                m_projectM->selectPreset(index, true);
            }
        });
    }
}

void ProjectMView::selectPreset(const QString& path)
{
    m_pendingPresetPath = path;
    if(isReady()) {
        queueGLOperation([this, path]() {
            if(isReady()) {
                leaveIdleForUserSelection();
                m_projectM->selectPreset(path, true);
            }
        });
    }
}

void ProjectMView::setPlaybackIdle(bool idle)
{
    if(std::exchange(m_playbackIdle, idle) == idle) {
        return;
    }

    if(isReady()) {
        queueGLOperation([this]() { applyIdleState(); });
    }
}

void ProjectMView::setPresetLocked(bool locked)
{
    m_presetLocked = locked;
    if(isReady()) {
        queueGLOperation([this]() { applyIdleState(); });
    }
}

void ProjectMView::setShuffle(bool shuffle)
{
    m_shuffle = shuffle;
    if(isReady()) {
        queueGLOperation([this, shuffle]() {
            if(isReady()) {
                m_projectM->setShuffle(shuffle);
            }
        });
    }
}

void ProjectMView::applySettings(const ProjectMSettings& settings)
{
    const bool recreate   = settings.scanRecursive != m_settings.scanRecursive;
    const bool fpsChanged = settings.maxFps != m_settings.maxFps;
    m_settings            = settings;

    if(recreate && m_initialised) {
        scheduleProjectMRecreation();
        return;
    }

    if(isReady()) {
        queueGLOperation([this, settings]() {
            if(isReady()) {
                m_projectM->applySettings(settings);
            }
        });
    }

    if(fpsChanged) {
        updateRenderTimer();
    }
}

int ProjectMView::selectedPresetIndex()
{
    if(!isReady()) {
        return -1;
    }

    const CurrentProjectMContext current{this};
    if(!current.isCurrent()) {
        return -1;
    }

    return m_projectM->selectedPresetIndex();
}

QString ProjectMView::selectedPresetPath()
{
    if(!isReady()) {
        return {};
    }

    const CurrentProjectMContext current{this};
    if(!current.isCurrent()) {
        return {};
    }

    return m_projectM->selectedPresetPath();
}

bool ProjectMView::presetLocked() const
{
    return m_presetLocked;
}

bool ProjectMView::shuffle() const
{
    return m_shuffle;
}

const ProjectMSettings& ProjectMView::settings() const
{
    return m_settings;
}

QString ProjectMView::presetFailureMessage(const QString& path) const
{
    const auto failure = m_failedPresetPaths.find(QDir::cleanPath(path));
    return failure == m_failedPresetPaths.cend() ? QString{} : failure->second;
}

std::vector<ProjectMPreset> ProjectMView::presets()
{
    if(!isReady()) {
        return {};
    }

    const CurrentProjectMContext current{this};
    if(!current.isCurrent()) {
        return {};
    }

    std::vector<ProjectMPreset> presets;
    const int size = m_projectM->playlistSize();
    presets.reserve(size);

    for(int index{0}; index < size; ++index) {
        const QString name      = m_projectM->presetName(index);
        const QString path      = m_projectM->presetPath(index);
        const auto failedPreset = m_failedPresets.find(index);
        presets.push_back(
            {.index          = index,
             .name           = name.isEmpty() ? tr("Preset %1").arg(index + 1) : name,
             .path           = path,
             .relativePath   = {},
             .failureMessage = failedPreset == m_failedPresets.cend() ? QString{} : failedPreset->second});
    }

    return presets;
}

void ProjectMView::initializeGL()
{
    m_initialised = true;
    initializeOpenGLFunctions();
    initialiseOpenGLDebugLogger();

    logOpenGLContext();

    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    const QSize viewportSize = framebufferSize(width(), height());
    glViewport(0, 0, viewportSize.width(), viewportSize.height());
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    createProjectM();
}

void ProjectMView::resizeGL(int /*w*/, int /*h*/)
{
    resizeRenderer(width(), height());
}

void ProjectMView::paintGL()
{
    resizeRenderer(width(), height());
    if(m_recreateProjectM) {
        createProjectM();
    }
    processQueuedGLOperations();
    feedPendingPcm();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(m_projectM) {
        m_projectM->renderFrame();
    }
}

void ProjectMView::exposeEvent(QExposeEvent* event)
{
    QOpenGLWindow::exposeEvent(event);
    if(m_recreateProjectM && isExposed()) {
        requestUpdate();
    }
    updateRenderTimer();
}

void ProjectMView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_renderTimer.timerId()) {
        requestUpdate();
        return;
    }

    QOpenGLWindow::timerEvent(event);
}

void ProjectMView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        Q_EMIT doubleClicked();
        event->accept();
        return;
    }

    QOpenGLWindow::mouseDoubleClickEvent(event);
}

void ProjectMView::mousePressEvent(QMouseEvent* event)
{
    Q_EMIT activated();

    if(event->button() == Qt::RightButton) {
        Q_EMIT contextMenuRequested(event->position().toPoint());
        event->accept();
        return;
    }

    QOpenGLWindow::mousePressEvent(event);
}

QStringList ProjectMView::dataDirCandidates()
{
    QStringList candidates;

    const auto appendCandidate = [&candidates](const QString& path) {
        if(!path.isEmpty() && !candidates.contains(path)) {
            candidates.append(path);
        }
    };

    appendCandidate(qEnvironmentVariable("FOOYIN_PROJECTM_DATADIR"));
    appendCandidate(qEnvironmentVariable("PROJECTM_DATADIR"));
    appendCandidate(u"/usr/share/projectM"_s);
    appendCandidate(u"/usr/local/share/projectM"_s);

    return candidates;
}

ProjectMView::ProjectMPaths ProjectMView::resolveProjectMPaths()
{
    const QStringList candidates = dataDirCandidates();

    for(const QString& dataDir : std::as_const(candidates)) {
        if(QDir{dataDir}.exists()) {
            return {.dataDir = dataDir};
        }
    }

    return {};
}

void ProjectMView::createProjectM()
{
    m_recreateProjectM = false;
    m_renderTimer.stop();
    m_pendingGLOperations.clear();
    m_projectM.reset();
    m_idlePresetActive = false;
    m_failedPresets.clear();
    m_failedPresetPaths.clear();
    m_ready = false;
    resetPcmCursor();

    if(m_presetDirs.empty()) {
        setUnavailable(tr("No projectM preset folders are configured."));
        return;
    }

    m_paths = resolveProjectMPaths();

    const char* version  = projectm_get_version_string();
    const char* revision = projectm_get_vcs_version_string();
    qCDebug(PROJECTM_GL) << "Creating projectM; runtime version=" << QString::fromUtf8(version ? version : "unknown")
                         << "revision=" << QString::fromUtf8(revision ? revision : "unknown")
                         << "dataDir=" << m_paths.dataDir << "presetDirs=" << m_presetDirs
                         << "recursive=" << m_settings.scanRecursive
                         << "mesh=" << QSize{m_settings.meshWidth, m_settings.meshHeight}
                         << "maxFps=" << m_settings.maxFps;
    if(version) {
        projectm_free_string(version);
    }
    if(revision) {
        projectm_free_string(revision);
    }

    auto projectM = std::make_unique<ProjectMInstance>(m_paths.dataDir, m_presetDirs, m_settings);
    QObject::connect(projectM.get(), &ProjectMInstance::presetLoadFailed, this,
                     [this](int index, const QString& path, const QString& message) {
                         const QString failureMessage = message.isEmpty() ? tr("Preset failed to load.") : message;
                         m_failedPresets.insert_or_assign(index, failureMessage);
                         if(!path.isEmpty()) {
                             m_failedPresetPaths.insert_or_assign(QDir::cleanPath(path), failureMessage);
                         }
                         qCWarning(PROJECTM_GL) << "projectM failed to load preset" << index << path << message;
                         Q_EMIT presetLoadFailed(index, path, failureMessage);
                     });
    QObject::connect(projectM.get(), &ProjectMInstance::presetChanged, this, [this](const QString& path) {
        m_pendingPresetPath = path;
        Q_EMIT presetChanged(path);
    });

    qCDebug(PROJECTM_GL) << "projectM instance created; ready=" << projectM->isReady()
                         << "error=" << projectM->errorMessage() << "presets=" << projectM->playlistSize();

    if(!projectM->isReady()) {
        const QString message
            = projectM->errorMessage().isEmpty() ? tr("projectM initialisation failed.") : projectM->errorMessage();
        setUnavailable(tr("projectM initialisation failed: %1").arg(message));
        return;
    }

    m_projectM = std::move(projectM);
    m_projectM->setPresetLocked(m_presetLocked);
    m_projectM->setShuffle(m_shuffle);

    if(!m_pendingPresetPath.isEmpty()) {
        m_projectM->selectPreset(m_pendingPresetPath, true);
    }

    resizeRenderer(width(), height(), true);

    m_ready = true;
    applyIdleState();

    m_statusText = m_presetDirs.size() == 1
                     ? tr("Using presets from %1").arg(m_presetDirs.front())
                     : tr("Using presets from %Ln folder(s)", nullptr, static_cast<int>(m_presetDirs.size()));

    emitAvailability();
    updateRenderTimer();
}

QSize ProjectMView::framebufferSize(int width, int height) const
{
    const qreal dpr = devicePixelRatio();
    return {std::max(qRound(static_cast<qreal>(width) * dpr), 1),
            std::max(qRound(static_cast<qreal>(height) * dpr), 1)};
}

void ProjectMView::resizeRenderer(int width, int height, bool force)
{
    const QSize size = framebufferSize(width, height);
    if(!force && size == m_rendererSize) {
        return;
    }

    m_rendererSize = size;
    glViewport(0, 0, size.width(), size.height());

    if(m_projectM) {
        m_projectM->resize(static_cast<size_t>(size.width()), static_cast<size_t>(size.height()));
    }
}

void ProjectMView::feedPendingPcm()
{
    if(!isReady() || !m_visualisationSession) {
        resetPcmCursor();
        return;
    }

    const uint64_t currentTimeMs = m_visualisationSession->currentTimeMs();
    if(currentTimeMs == 0) {
        resetPcmCursor();
        return;
    }

    if(!m_hasPcmCursor) {
        m_lastPcmTimeMs = currentTimeMs > InitialPcmWindowMs ? currentTimeMs - InitialPcmWindowMs : 0;
        m_hasPcmCursor  = true;
    }

    if(currentTimeMs + InitialPcmWindowMs < m_lastPcmTimeMs) {
        resetPcmCursor();
        m_lastPcmTimeMs = currentTimeMs > InitialPcmWindowMs ? currentTimeMs - InitialPcmWindowMs : 0;
        m_hasPcmCursor  = true;
    }

    if(currentTimeMs <= m_lastPcmTimeMs) {
        return;
    }

    const uint64_t durationMs = std::min<uint64_t>(currentTimeMs - m_lastPcmTimeMs, MaxPcmCatchupMs);
    if(!m_visualisationSession->getPcmWindowEndingAt(m_pcmWindow, currentTimeMs, durationMs)
       || !m_pcmWindow.isValid()) {
        m_lastPcmTimeMs = currentTimeMs;
        return;
    }

    feedPcmWindow(m_pcmWindow);
    m_lastPcmTimeMs = m_pcmWindow.startTimeMs + m_pcmWindow.format.durationForFrames(m_pcmWindow.frameCount);
}

void ProjectMView::feedPcmWindow(const VisualisationSession::PcmWindow& window)
{
    if(!isReady() || !window.isValid() || window.format.sampleFormat() != SampleFormat::F32
       || window.format.channelCount() <= 0) {
        return;
    }

    const int frameCount   = window.frameCount;
    const int channelCount = window.format.channelCount();
    const auto samples     = window.interleavedSamples();
    if(samples.size() < static_cast<size_t>(frameCount) * channelCount) {
        return;
    }

    m_stereoBuffer.resize(static_cast<size_t>(frameCount) * 2U);

    for(int frameIndex{0}; frameIndex < frameCount; ++frameIndex) {
        const size_t baseIndex = static_cast<size_t>(frameIndex) * static_cast<size_t>(channelCount);
        const float left       = samples[baseIndex];
        const float right      = channelCount > 1 ? samples[baseIndex + 1] : left;

        const size_t stereoIndex        = static_cast<size_t>(frameIndex) * 2U;
        m_stereoBuffer[stereoIndex]     = left;
        m_stereoBuffer[stereoIndex + 1] = right;
    }

    m_projectM->addPcm(m_stereoBuffer.data(), static_cast<unsigned int>(frameCount));
}

void ProjectMView::initialiseOpenGLDebugLogger()
{
    m_debugLogger = std::make_unique<QOpenGLDebugLogger>();
    if(!m_debugLogger->initialize()) {
        qCWarning(PROJECTM_GL) << "OpenGL debug logging is unavailable for this context";
        m_debugLogger.reset();
        return;
    }

    QObject::connect(m_debugLogger.get(), &QOpenGLDebugLogger::messageLogged, this,
                     [](const QOpenGLDebugMessage& message) { qCWarning(PROJECTM_GL) << message; });
    m_debugLogger->disableMessages(QOpenGLDebugMessage::AnySource, QOpenGLDebugMessage::AnyType,
                                   QOpenGLDebugMessage::NotificationSeverity);
    m_debugLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
}

void ProjectMView::logOpenGLContext()
{
    const auto* currentContext = QOpenGLContext::currentContext();
    if(!currentContext) {
        qCWarning(PROJECTM_GL) << "No current OpenGL context";
        return;
    }

    const QSurfaceFormat actualFormat    = currentContext->format();
    const QSurfaceFormat requestedFormat = format();
    const auto stringValue               = [this](GLenum name) {
        const auto* value = glGetString(name);
        return value ? QString::fromLatin1(value) : u"unavailable"_s;
    };

    qCDebug(PROJECTM_GL) << "Qt version=" << qVersion() << "platform=" << QGuiApplication::platformName();
    qCDebug(PROJECTM_GL) << "Requested OpenGL format:" << requestedFormat.majorVersion() << '.'
                         << requestedFormat.minorVersion() << "profile=" << requestedFormat.profile()
                         << "renderableType=" << requestedFormat.renderableType()
                         << "depth=" << requestedFormat.depthBufferSize()
                         << "stencil=" << requestedFormat.stencilBufferSize() << "samples=" << requestedFormat.samples()
                         << "swapBehavior=" << requestedFormat.swapBehavior()
                         << "swapInterval=" << requestedFormat.swapInterval()
                         << "options=" << requestedFormat.options();
    qCDebug(PROJECTM_GL) << "Actual OpenGL context:" << actualFormat.majorVersion() << '.'
                         << actualFormat.minorVersion() << "profile=" << actualFormat.profile()
                         << "GLES=" << currentContext->isOpenGLES() << "depth=" << actualFormat.depthBufferSize()
                         << "stencil=" << actualFormat.stencilBufferSize() << "samples=" << actualFormat.samples()
                         << "options=" << actualFormat.options();

    const bool versionTooOld
        = actualFormat.majorVersion() < 3 || (actualFormat.majorVersion() == 3 && actualFormat.minorVersion() < 3);
    if(currentContext->isOpenGLES()) {
        qCWarning(PROJECTM_GL) << "projectM requested desktop OpenGL 3.3 core, but Qt created an OpenGL ES context";
    }
    else if(versionTooOld) {
        qCWarning(PROJECTM_GL) << "projectM requires desktop OpenGL 3.3 or newer, but Qt created"
                               << actualFormat.majorVersion() << '.' << actualFormat.minorVersion();
    }
    else if(actualFormat.profile() != QSurfaceFormat::CoreProfile) {
        qCWarning(PROJECTM_GL) << "projectM requested an OpenGL core profile, but Qt created" << actualFormat.profile();
    }
    else {
        qCDebug(PROJECTM_GL) << "projectM OpenGL requirements satisfied: desktop OpenGL 3.3+ core profile";
    }

    qCDebug(PROJECTM_GL) << "GL_VERSION:" << stringValue(GL_VERSION);
    qCDebug(PROJECTM_GL) << "GLSL_VERSION:" << stringValue(GL_SHADING_LANGUAGE_VERSION);
    qCDebug(PROJECTM_GL) << "GL_VENDOR:" << stringValue(GL_VENDOR);
    qCDebug(PROJECTM_GL) << "GL_RENDERER:" << stringValue(GL_RENDERER);
    qCDebug(PROJECTM_GL) << "Window logical size=" << size()
                         << "framebuffer size=" << framebufferSize(width(), height())
                         << "devicePixelRatio=" << devicePixelRatio()
                         << "defaultFramebufferObject=" << defaultFramebufferObject();
}

void ProjectMView::resetPcmCursor()
{
    m_hasPcmCursor  = false;
    m_lastPcmTimeMs = 0;
}

void ProjectMView::queueGLOperation(std::function<void()> operation)
{
    if(!operation) {
        return;
    }

    m_pendingGLOperations.push_back(std::move(operation));
    if(isExposed()) {
        requestUpdate();
    }
}

void ProjectMView::processQueuedGLOperations()
{
    while(!m_pendingGLOperations.empty()) {
        auto operation = std::move(m_pendingGLOperations.front());
        m_pendingGLOperations.pop_front();
        operation();
    }
}

void ProjectMView::applyIdleState()
{
    if(!isReady()) {
        return;
    }

    const bool shouldShowIdle = m_playbackIdle && !m_presetLocked;
    if(shouldShowIdle == m_idlePresetActive) {
        m_projectM->setPresetLocked(shouldShowIdle || m_presetLocked);
        return;
    }

    if(shouldShowIdle) {
        m_projectM->showIdlePreset();
    }
    else if(m_presetLocked) {
        m_projectM->restoreSelectedPreset(true);
    }
    else {
        m_projectM->resumeIdlePreset();
    }

    m_idlePresetActive = shouldShowIdle;
}

void ProjectMView::leaveIdleForUserSelection()
{
    if(!m_idlePresetActive) {
        return;
    }

    m_projectM->setPresetLocked(m_presetLocked);
    m_idlePresetActive = false;
}

void ProjectMView::scheduleProjectMRecreation()
{
    m_recreateProjectM = true;
    m_ready            = false;
    m_statusText       = tr("Initialising projectM…");
    updateRenderTimer();
    emitAvailability();
    if(isExposed()) {
        requestUpdate();
    }
}

void ProjectMView::setUnavailable(const QString& statusText)
{
    m_projectM.reset();
    m_pendingGLOperations.clear();
    resetPcmCursor();
    m_ready      = false;
    m_statusText = statusText;
    emitAvailability();
    updateRenderTimer();
}

void ProjectMView::emitAvailability()
{
    Q_EMIT availabilityChanged(isReady(), m_statusText);
}

void ProjectMView::updateRenderTimer()
{
    if(isReady() && isExposed()) {
        m_renderTimer.start(Gui::FrameRate::intervalMsForFps(m_settings.maxFps), this);
        requestUpdate();
    }
    else {
        m_renderTimer.stop();
    }
}
} // namespace Fooyin::ProjectM
