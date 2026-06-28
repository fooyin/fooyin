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

#include "projectmwidget.h"

#include "projectmconfigdialog.h"
#include "projectmpresetdialog.h"
#include "projectmview.h"

#include <core/coresettings.h>
#include <core/engine/enginecontroller.h>
#include <core/engine/visualisationservice.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QOpenGLWindow>
#include <QPixmap>
#include <QRandomGenerator>
#include <QSizePolicy>
#include <QSplitter>
#include <QSplitterHandle>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

constexpr auto PresetLockKey                 = "PresetLock"_L1;
constexpr auto PresetShuffleKey              = "PresetShuffle"_L1;
constexpr auto PresetDurationKey             = "PresetDuration"_L1;
constexpr auto RememberPresetKey             = "RememberPreset"_L1;
constexpr auto PresetPathKey                 = "PresetPath"_L1;
constexpr auto ScanRecursiveLayoutKey        = "ScanRecursive"_L1;
constexpr auto MaxFpsLayoutKey               = "MaxFps"_L1;
constexpr auto MeshWidthLayoutKey            = "MeshWidth"_L1;
constexpr auto MeshHeightLayoutKey           = "MeshHeight"_L1;
constexpr auto AspectCorrectionLayoutKey     = "AspectCorrection"_L1;
constexpr auto SoftCutDurationLayoutKey      = "SoftCutDuration"_L1;
constexpr auto HardCutsEnabledLayoutKey      = "HardCutsEnabled"_L1;
constexpr auto HardCutDurationLayoutKey      = "HardCutDuration"_L1;
constexpr auto HardCutSensitivityLayoutKey   = "HardCutSensitivity"_L1;
constexpr auto BeatSensitivityLayoutKey      = "BeatSensitivity"_L1;
constexpr auto PresetDirectoryKey            = "ProjectM/PresetDirectory";
constexpr auto PresetDirectoriesKey          = "ProjectM/PresetDirectories";
constexpr auto ScanRecursiveKey              = "ProjectM/ScanRecursive";
constexpr auto MaxFpsKey                     = "ProjectM/MaxFps";
constexpr auto MeshWidthKey                  = "ProjectM/MeshWidth";
constexpr auto MeshHeightKey                 = "ProjectM/MeshHeight";
constexpr auto AspectCorrectionKey           = "ProjectM/AspectCorrection";
constexpr auto GlobalPresetDurationKey       = "ProjectM/PresetDuration";
constexpr auto SoftCutDurationKey            = "ProjectM/SoftCutDuration";
constexpr auto HardCutsEnabledKey            = "ProjectM/HardCutsEnabled";
constexpr auto HardCutDurationKey            = "ProjectM/HardCutDuration";
constexpr auto HardCutSensitivityKey         = "ProjectM/HardCutSensitivity";
constexpr auto BeatSensitivityKey            = "ProjectM/BeatSensitivity";
constexpr auto SelectedCategoriesKey         = "ProjectM/SelectedCategories";
constexpr auto CategoriesConfiguredKey       = "ProjectM/CategoriesConfigured";
constexpr auto SelectedFavoriteCategoriesKey = "ProjectM/SelectedFavoriteCategories";
constexpr auto FavoritesKey                  = "ProjectM/Favorites";
constexpr std::array PresetDurations{5, 10, 20, 30, 45, 60};

constexpr auto FullScreenAction     = "ProjectM.FullScreen";
constexpr auto SelectPresetAction   = "ProjectM.SelectPreset";
constexpr auto PreviousPresetAction = "ProjectM.PreviousPreset";
constexpr auto NextPresetAction     = "ProjectM.NextPreset";
constexpr auto RandomPresetAction   = "ProjectM.RandomPreset";
constexpr auto LockPresetAction     = "ProjectM.LockPreset";
constexpr auto ShufflePresetsAction = "ProjectM.ShufflePresets";

constexpr auto GeometryKey            = "Geometry"_L1;
constexpr auto ProjectMWindowStateKey = "ProjectM/WindowState"_L1;

namespace Fooyin::ProjectM {
namespace {
bool isPresetDuration(int seconds)
{
    return std::ranges::find(PresetDurations, seconds) != PresetDurations.end();
}

ProjectMSettings normaliseSettings(ProjectMSettings settings)
{
    settings.maxFps             = std::clamp(settings.maxFps, 1, 240);
    settings.meshWidth          = std::clamp(settings.meshWidth, 8, 256);
    settings.meshHeight         = std::clamp(settings.meshHeight, 8, 256);
    settings.presetDuration     = std::clamp(settings.presetDuration, 1.0, 300.0);
    settings.softCutDuration    = std::clamp(settings.softCutDuration, 0.0, 30.0);
    settings.hardCutDuration    = std::clamp(settings.hardCutDuration, 1.0, 300.0);
    settings.hardCutSensitivity = std::clamp(settings.hardCutSensitivity, 0.0F, 5.0F);
    settings.beatSensitivity    = std::clamp(settings.beatSensitivity, 0.0F, 5.0F);
    return settings;
}

ProjectMWidget::ConfigData normaliseConfig(ProjectMWidget::ConfigData config)
{
    config.presetDir = ProjectMPresetLibrary::normalisePath(config.presetDir);
    config.settings  = normaliseSettings(config.settings);
    return config;
}

QStringList presetDirs(const ProjectMWidget::ConfigData& config)
{
    if(config.presetDir.isEmpty()) {
        return {};
    }
    return {config.presetDir};
}
} // namespace

ProjectMWidget::ProjectMWidget(ActionManager* actionManager, EngineController* engine, SettingsManager* settings,
                               QWidget* parent)
    : FyWidget{parent}
    , m_view{new ProjectMView()}
    , m_viewContainer{createWindowContainer(m_view, this)}
    , m_statusLabel{new QLabel(this)}
    , m_resizeSnapshot{new QLabel(this)}
    , m_actionManager{actionManager}
    , m_context{new WidgetContext(m_viewContainer, Context{Id{"Fooyin.Context.ProjectM."}.append(id())}, this)}
    , m_settings{settings}
    , m_fullScreenAction{new QAction(tr("&Full Screen"), this)}
    , m_selectPresetAction{new QAction(tr("&Select Preset…"), this)}
    , m_previousPresetAction{new QAction(tr("&Previous Preset"), this)}
    , m_nextPresetAction{new QAction(tr("&Next Preset"), this)}
    , m_randomPresetAction{new QAction(tr("&Random Preset"), this)}
    , m_lockPresetAction{new QAction(tr("&Lock Current Preset"), this)}
    , m_shufflePresetsAction{new QAction(tr("&Shuffle Presets"), this)}
    , m_rememberPresetAction{new QAction(tr("Remember &Current Preset"), this)}
    , m_fullScreenCmd{nullptr}
    , m_selectPresetCmd{nullptr}
    , m_previousPresetCmd{nullptr}
    , m_nextPresetCmd{nullptr}
    , m_randomPresetCmd{nullptr}
    , m_lockPresetCmd{nullptr}
    , m_shufflePresetsCmd{nullptr}
    , m_rememberPreset{false}
    , m_detachedWindowFullScreen{false}
    , m_splitterResizeActive{false}
    , m_topLevelStateLoaded{false}
{
    m_actionManager->addContextObject(m_context);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setMargin(12);
    m_statusLabel->setText(m_view->statusText());
    m_statusLabel->setVisible(false);

    m_viewContainer->setFocusPolicy(Qt::StrongFocus);
    m_viewContainer->installEventFilter(this);

    m_resizeSnapshot->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_resizeSnapshot->setScaledContents(true);
    m_resizeSnapshot->setVisible(false);

    layout->addWidget(m_viewContainer);
    layout->addWidget(m_statusLabel);

    m_config = normaliseConfig(defaultConfig());
    setupActions();
    scanPresetLibrary();
    m_view->setPresetDirs(presetDirs(m_config));
    m_view->applySettings(m_config.settings);

    QObject::connect(m_view, &ProjectMView::availabilityChanged, this, [this](bool ready, const QString& text) {
        const bool showView = ready || !m_view->hasInitialised() || m_view->isRecreating();
        m_viewContainer->setVisible(showView);
        m_statusLabel->setVisible(!showView);
        m_statusLabel->setText(text);
        updateActionState();
    });
    QObject::connect(m_view, &ProjectMView::activated, m_viewContainer, [this]() { m_viewContainer->setFocus(); });
    QObject::connect(m_view, &ProjectMView::doubleClicked, this, &ProjectMWidget::toggleFullScreen);
    QObject::connect(m_view, &ProjectMView::contextMenuRequested, this, [this](const QPoint& pos) {
        m_viewContainer->setFocus();
        showContextMenu(m_fullScreenWindow ? QCursor::pos() : m_viewContainer->mapToGlobal(pos));
    });
    QObject::connect(m_view, &ProjectMView::presetChanged, this, [this](const QString& path) {
        if(m_rememberPreset && !path.isEmpty()) {
            m_presetPath = path;
        }
    });

    m_view->setVisualisationSession(engine->visualisationService()->createSession());
}

ProjectMWidget::~ProjectMWidget()
{
    endSplitterResize();
    for(const auto& object : std::as_const(m_splitterEventObjects)) {
        if(object) {
            object->removeEventFilter(this);
        }
    }

    m_view->shutdown();
    m_actionManager->removeContextObject(m_context);
    leaveFullScreen();
}

QString ProjectMWidget::name() const
{
    return tr("projectM");
}

QString ProjectMWidget::layoutName() const
{
    return u"ProjectM"_s;
}

QSize ProjectMWidget::minimumSizeHint() const
{
    return {240, 135};
}

void ProjectMWidget::saveLayoutData(QJsonObject& layout)
{
    saveSelectedPreset();

    layout[PresetLockKey]               = m_view->presetLocked();
    layout[PresetShuffleKey]            = m_view->shuffle();
    layout[RememberPresetKey]           = m_rememberPreset;
    layout[ScanRecursiveLayoutKey]      = m_config.settings.scanRecursive;
    layout[MaxFpsLayoutKey]             = m_config.settings.maxFps;
    layout[MeshWidthLayoutKey]          = m_config.settings.meshWidth;
    layout[MeshHeightLayoutKey]         = m_config.settings.meshHeight;
    layout[AspectCorrectionLayoutKey]   = m_config.settings.aspectCorrection;
    layout[PresetDurationKey]           = m_config.settings.presetDuration;
    layout[SoftCutDurationLayoutKey]    = m_config.settings.softCutDuration;
    layout[HardCutsEnabledLayoutKey]    = m_config.settings.hardCutsEnabled;
    layout[HardCutDurationLayoutKey]    = m_config.settings.hardCutDuration;
    layout[HardCutSensitivityLayoutKey] = m_config.settings.hardCutSensitivity;
    layout[BeatSensitivityLayoutKey]    = m_config.settings.beatSensitivity;
    if(m_rememberPreset && !m_presetPath.isEmpty()) {
        layout[PresetPathKey] = m_presetPath;
    }
}

void ProjectMWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(PresetLockKey)) {
        m_view->setPresetLocked(layout.value(PresetLockKey).toBool());
    }
    if(layout.contains(PresetShuffleKey)) {
        m_view->setShuffle(layout.value(PresetShuffleKey).toBool());
    }
    if(layout.contains(RememberPresetKey)) {
        m_rememberPreset = layout.value(RememberPresetKey).toBool(true);
    }

    m_config.settings.scanRecursive = layout.value(ScanRecursiveLayoutKey).toBool(m_config.settings.scanRecursive);
    m_config.settings.maxFps        = layout.value(MaxFpsLayoutKey).toInt(m_config.settings.maxFps);
    m_config.settings.meshWidth     = layout.value(MeshWidthLayoutKey).toInt(m_config.settings.meshWidth);
    m_config.settings.meshHeight    = layout.value(MeshHeightLayoutKey).toInt(m_config.settings.meshHeight);
    m_config.settings.aspectCorrection
        = layout.value(AspectCorrectionLayoutKey).toBool(m_config.settings.aspectCorrection);
    m_config.settings.presetDuration = layout.value(PresetDurationKey).toDouble(m_config.settings.presetDuration);
    m_config.settings.softCutDuration
        = layout.value(SoftCutDurationLayoutKey).toDouble(m_config.settings.softCutDuration);
    m_config.settings.hardCutsEnabled
        = layout.value(HardCutsEnabledLayoutKey).toBool(m_config.settings.hardCutsEnabled);
    m_config.settings.hardCutDuration
        = layout.value(HardCutDurationLayoutKey).toDouble(m_config.settings.hardCutDuration);
    m_config.settings.hardCutSensitivity
        = layout.value(HardCutSensitivityLayoutKey).toDouble(m_config.settings.hardCutSensitivity);
    m_config.settings.beatSensitivity
        = layout.value(BeatSensitivityLayoutKey).toDouble(m_config.settings.beatSensitivity);
    m_config = normaliseConfig(m_config);
    scanPresetLibrary();
    m_view->applySettings(m_config.settings);

    m_presetPath = layout.value(PresetPathKey).toString();
    if(m_rememberPreset && !m_presetPath.isEmpty()) {
        m_view->selectPreset(m_presetPath);
    }

    updateActionState();
}

ProjectMWidget::ConfigData ProjectMWidget::factoryConfig() const
{
    const QStringList defaultDirs = ProjectMView::defaultPresetDirs();
    ConfigData config;
    config.presetDir = defaultDirs.isEmpty() ? QString{} : defaultDirs.front();
    return normaliseConfig(std::move(config));
}

ProjectMWidget::ConfigData ProjectMWidget::defaultConfig() const
{
    ConfigData config{factoryConfig()};

    const QString presetDir = m_settings->fileValue(PresetDirectoryKey).toString().trimmed();
    if(!presetDir.isEmpty()) {
        config.presetDir = presetDir;
    }

    config.settings.scanRecursive = m_settings->fileValue(ScanRecursiveKey, config.settings.scanRecursive).toBool();
    config.settings.maxFps        = m_settings->fileValue(MaxFpsKey, config.settings.maxFps).toInt();
    config.settings.meshWidth     = m_settings->fileValue(MeshWidthKey, config.settings.meshWidth).toInt();
    config.settings.meshHeight    = m_settings->fileValue(MeshHeightKey, config.settings.meshHeight).toInt();
    config.settings.aspectCorrection
        = m_settings->fileValue(AspectCorrectionKey, config.settings.aspectCorrection).toBool();
    config.settings.presetDuration
        = m_settings->fileValue(GlobalPresetDurationKey, config.settings.presetDuration).toDouble();
    config.settings.softCutDuration
        = m_settings->fileValue(SoftCutDurationKey, config.settings.softCutDuration).toDouble();
    config.settings.hardCutsEnabled
        = m_settings->fileValue(HardCutsEnabledKey, config.settings.hardCutsEnabled).toBool();
    config.settings.hardCutDuration
        = m_settings->fileValue(HardCutDurationKey, config.settings.hardCutDuration).toDouble();
    config.settings.hardCutSensitivity
        = m_settings->fileValue(HardCutSensitivityKey, config.settings.hardCutSensitivity).toFloat();
    config.settings.beatSensitivity
        = m_settings->fileValue(BeatSensitivityKey, config.settings.beatSensitivity).toFloat();

    return normaliseConfig(std::move(config));
}

const ProjectMWidget::ConfigData& ProjectMWidget::currentConfig() const
{
    return m_config;
}

void ProjectMWidget::applyConfig(const ConfigData& config)
{
    m_config = normaliseConfig(config);
    m_settings->fileSet(PresetDirectoryKey, m_config.presetDir);
    scanPresetLibrary();
    m_view->setPresetDirs(presetDirs(m_config));
    m_view->applySettings(m_config.settings);
}

void ProjectMWidget::saveDefaults(const ConfigData& config) const
{
    const ConfigData normalised = normaliseConfig(config);
    m_settings->fileSet(PresetDirectoryKey, normalised.presetDir);
    m_settings->fileSet(ScanRecursiveKey, normalised.settings.scanRecursive);
    m_settings->fileSet(MaxFpsKey, normalised.settings.maxFps);
    m_settings->fileSet(MeshWidthKey, normalised.settings.meshWidth);
    m_settings->fileSet(MeshHeightKey, normalised.settings.meshHeight);
    m_settings->fileSet(AspectCorrectionKey, normalised.settings.aspectCorrection);
    m_settings->fileSet(GlobalPresetDurationKey, normalised.settings.presetDuration);
    m_settings->fileSet(SoftCutDurationKey, normalised.settings.softCutDuration);
    m_settings->fileSet(HardCutsEnabledKey, normalised.settings.hardCutsEnabled);
    m_settings->fileSet(HardCutDurationKey, normalised.settings.hardCutDuration);
    m_settings->fileSet(HardCutSensitivityKey, normalised.settings.hardCutSensitivity);
    m_settings->fileSet(BeatSensitivityKey, normalised.settings.beatSensitivity);
    m_settings->fileRemove(PresetDirectoriesKey);
    m_settings->fileRemove(CategoriesConfiguredKey);
    m_settings->fileRemove(SelectedCategoriesKey);
    m_settings->fileRemove(SelectedFavoriteCategoriesKey);
    m_settings->fileRemove(FavoritesKey);
}

void ProjectMWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(PresetDirectoryKey);
    m_settings->fileRemove(PresetDirectoriesKey);
    m_settings->fileRemove(ScanRecursiveKey);
    m_settings->fileRemove(MaxFpsKey);
    m_settings->fileRemove(MeshWidthKey);
    m_settings->fileRemove(MeshHeightKey);
    m_settings->fileRemove(AspectCorrectionKey);
    m_settings->fileRemove(GlobalPresetDurationKey);
    m_settings->fileRemove(SoftCutDurationKey);
    m_settings->fileRemove(HardCutsEnabledKey);
    m_settings->fileRemove(HardCutDurationKey);
    m_settings->fileRemove(HardCutSensitivityKey);
    m_settings->fileRemove(BeatSensitivityKey);
    m_settings->fileRemove(CategoriesConfiguredKey);
    m_settings->fileRemove(SelectedCategoriesKey);
    m_settings->fileRemove(SelectedFavoriteCategoriesKey);
    m_settings->fileRemove(FavoritesKey);
}

void ProjectMWidget::showDetachedWindow()
{
    if(m_fullScreenWindow) {
        if(m_detachedWindowFullScreen) {
            m_fullScreenWindow->showNormal();
            m_detachedWindowFullScreen = false;
        }
        m_fullScreenWindow->raise();
        m_fullScreenWindow->activateWindow();
        updateActionState();
        return;
    }

    openDetachedWindow(false);
}

bool ProjectMWidget::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_viewContainer
       && (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateResizeSnapshotGeometry();
    }

    if(m_splitterResizeActive && event->type() == QEvent::MouseButtonRelease) {
        endSplitterResize();
    }

    if(watched == m_fullScreenWindow) {
        if(event->type() == QEvent::Close) {
            leaveFullScreen();
            event->ignore();
            return true;
        }
        if(event->type() == QEvent::KeyPress) {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            if(keyEvent->key() == Qt::Key_Escape || keyEvent->key() == Qt::Key_F11) {
                leaveFullScreen();
                return true;
            }
        }
    }

    if(event->type() == QEvent::MouseButtonPress
       && std::ranges::any_of(m_splitterEventObjects, [watched](const auto& object) { return object == watched; })) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::LeftButton) {
            beginSplitterResize();
        }
    }

    return FyWidget::eventFilter(watched, event);
}

void ProjectMWidget::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void ProjectMWidget::showEvent(QShowEvent* event)
{
    if(isWindowWidget() && !m_topLevelStateLoaded) {
        loadTopLevelState();
    }

    FyWidget::showEvent(event);
    installSplitterEventFilters();
}

void ProjectMWidget::closeEvent(QCloseEvent* event)
{
    if(isWindowWidget()) {
        saveTopLevelState();
    }

    FyWidget::closeEvent(event);
}

void ProjectMWidget::openConfigDialog()
{
    showConfigDialog(new ProjectMConfigDialog(this, m_fullScreenWindow ? m_fullScreenWindow.data() : this));
}

void ProjectMWidget::setupActions()
{
    const QStringList category{u"projectM"_s};
    const Context actionContext = m_context->context();

    const auto registerAction
        = [this, &category, &actionContext](QAction* action, const Id& id, const QString& description) {
              action->setStatusTip(description);
              auto* command = m_actionManager->registerAction(action, id, actionContext);
              command->setCategories(category);
              command->setDescription(description);
              return command;
          };

    m_fullScreenAction->setCheckable(true);
    m_fullScreenCmd = registerAction(m_fullScreenAction, FullScreenAction, tr("Toggle projectM full screen"));
    m_fullScreenCmd->setDefaultShortcut(QKeySequence::FullScreen);
    QObject::connect(m_fullScreenAction, &QAction::triggered, this, [this]() { toggleFullScreen(); });

    m_selectPresetCmd = registerAction(m_selectPresetAction, SelectPresetAction, tr("Select a projectM preset"));
    QObject::connect(m_selectPresetAction, &QAction::triggered, this, [this]() { showPresetDialog(); });

    m_previousPresetCmd
        = registerAction(m_previousPresetAction, PreviousPresetAction, tr("Switch to the previous projectM preset"));
    QObject::connect(m_previousPresetAction, &QAction::triggered, this, [this]() {
        m_view->selectPreviousPreset();
        saveSelectedPreset();
    });

    m_nextPresetCmd = registerAction(m_nextPresetAction, NextPresetAction, tr("Switch to the next projectM preset"));
    QObject::connect(m_nextPresetAction, &QAction::triggered, this, [this]() {
        m_view->selectNextPreset();
        saveSelectedPreset();
    });

    m_randomPresetCmd
        = registerAction(m_randomPresetAction, RandomPresetAction, tr("Switch to a random projectM preset"));
    QObject::connect(m_randomPresetAction, &QAction::triggered, this, [this]() { selectRandomPreset(); });

    m_lockPresetAction->setCheckable(true);
    m_lockPresetCmd = registerAction(m_lockPresetAction, LockPresetAction, tr("Lock the current projectM preset"));
    QObject::connect(m_lockPresetAction, &QAction::triggered, this, [this](bool checked) { setPresetLocked(checked); });

    m_shufflePresetsAction->setCheckable(true);
    m_shufflePresetsCmd = registerAction(m_shufflePresetsAction, ShufflePresetsAction, tr("Shuffle projectM presets"));
    QObject::connect(m_shufflePresetsAction, &QAction::triggered, this, [this](bool checked) { setShuffle(checked); });

    m_rememberPresetAction->setCheckable(true);
    m_rememberPresetAction->setStatusTip(tr("Remember the current projectM preset"));
    QObject::connect(m_rememberPresetAction, &QAction::triggered, this,
                     [this](bool checked) { setRememberPreset(checked); });

    updateActionState();
}

void ProjectMWidget::updateActionState()
{
    const bool ready = m_view->isReady();

    m_fullScreenAction->setEnabled(ready || m_fullScreenWindow);
    m_fullScreenAction->setText(m_detachedWindowFullScreen ? tr("E&xit Full Screen") : tr("&Full Screen"));
    m_fullScreenAction->setChecked(m_detachedWindowFullScreen);
    m_selectPresetAction->setEnabled(ready);
    m_previousPresetAction->setEnabled(ready);
    m_nextPresetAction->setEnabled(ready);
    m_randomPresetAction->setEnabled(ready);
    m_lockPresetAction->setEnabled(ready);
    m_lockPresetAction->setChecked(m_view->presetLocked());
    m_shufflePresetsAction->setChecked(m_view->shuffle());
    m_rememberPresetAction->setChecked(m_rememberPreset);
}

void ProjectMWidget::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(m_fullScreenWindow ? m_fullScreenWindow.data() : this);
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    updateActionState();

    menu->addAction(m_selectPresetCmd->action());
    menu->addSeparator();
    menu->addAction(m_previousPresetCmd->action());
    menu->addAction(m_nextPresetCmd->action());
    menu->addAction(m_randomPresetCmd->action());
    menu->addSeparator();
    menu->addAction(m_lockPresetCmd->action());
    menu->addAction(m_shufflePresetsCmd->action());
    menu->addAction(m_rememberPresetAction);
    menu->addSeparator();
    menu->addAction(m_fullScreenCmd->action());

    auto* durationMenu  = menu->addMenu(tr("Preset &Duration"));
    auto* durationGroup = new QActionGroup(durationMenu);
    durationGroup->setExclusive(true);
    for(const int seconds : PresetDurations) {
        auto* action = durationMenu->addAction(tr("%1 seconds").arg(seconds));
        action->setCheckable(true);
        action->setChecked(seconds == qRound(m_config.settings.presetDuration));
        action->setData(seconds);
        durationGroup->addAction(action);
    }
    QObject::connect(durationGroup, &QActionGroup::triggered, this,
                     [this](QAction* action) { setPresetDuration(action->data().toInt()); });

    if(!m_view->isReady()) {
        menu->addSeparator();
        auto* statusAction = menu->addAction(m_view->statusText());
        statusAction->setEnabled(false);
    }

    addConfigureAction(menu);

    menu->popup(globalPos);
}

void ProjectMWidget::showPresetDialog()
{
    auto presets = m_library.presets();
    if(presets.empty()) {
        presets = m_view->presets();
    }
    if(presets.empty()) {
        return;
    }

    for(auto& preset : presets) {
        if(!preset.path.isEmpty()) {
            preset.failureMessage = m_view->presetFailureMessage(preset.path);
        }
    }

    QWidget* dialogParent = m_fullScreenWindow ? m_fullScreenWindow.data() : this;
    auto* dialog = new PresetDialog(presets, m_view->selectedPresetIndex(), m_view->selectedPresetPath(), dialogParent);
    QObject::connect(dialog, &PresetDialog::presetPathSelected, this, [this](const QString& path) {
        m_view->selectPreset(path);
        saveSelectedPreset();
    });
    QObject::connect(dialog, &PresetDialog::presetIndexSelected, this, [this](int index) {
        m_view->selectPreset(index);
        saveSelectedPreset();
    });
    QObject::connect(m_view, &ProjectMView::presetLoadFailed, dialog,
                     [dialog](int index, const QString& path, const QString& message) {
                         dialog->markPresetFailed(index, path, message);
                     });
    dialog->show();
}

void ProjectMWidget::toggleFullScreen()
{
    if(m_fullScreenWindow && m_detachedWindowFullScreen) {
        leaveFullScreen();
    }
    else if(m_fullScreenWindow) {
        m_fullScreenWindow->showFullScreen();
        m_detachedWindowFullScreen = true;
        updateActionState();
    }
    else {
        enterFullScreen();
    }
}

void ProjectMWidget::openDetachedWindow(bool fullScreen)
{
    if(m_fullScreenWindow || !m_view->isReady()) {
        return;
    }

    auto* window = new QWidget(nullptr, Qt::Window);
    window->setWindowTitle(tr("projectM"));
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->installEventFilter(this);

    auto* layout = new QHBoxLayout(window);
    layout->setContentsMargins({});
    layout->addWidget(m_viewContainer);

    m_fullScreenWindow         = window;
    m_detachedWindowFullScreen = fullScreen;
    QObject::connect(window, &QWidget::destroyed, this, [this]() {
        if(m_fullScreenWindow) {
            m_fullScreenWindow = nullptr;
        }
        m_detachedWindowFullScreen = false;
    });

    m_viewContainer->setFocus();
    if(fullScreen) {
        window->showFullScreen();
    }
    else {
        window->resize(800, 450);
        window->show();
    }
    updateActionState();
}

void ProjectMWidget::enterFullScreen()
{
    openDetachedWindow(true);
}

void ProjectMWidget::leaveFullScreen()
{
    if(!m_fullScreenWindow) {
        return;
    }

    QWidget* window = m_fullScreenWindow;
    m_fullScreenWindow.clear();
    m_detachedWindowFullScreen = false;
    window->removeEventFilter(this);

    if(auto* windowLayout = window->layout()) {
        windowLayout->removeWidget(m_viewContainer);
    }

    if(auto* widgetLayout = qobject_cast<QBoxLayout*>(layout())) {
        widgetLayout->removeWidget(m_viewContainer);
        widgetLayout->insertWidget(0, m_viewContainer);
    }

    const bool showView = m_view->isReady() || !m_view->hasInitialised();
    m_viewContainer->setVisible(showView);
    m_statusLabel->setVisible(!showView);
    m_viewContainer->setFocus();

    window->deleteLater();
    updateActionState();
}

void ProjectMWidget::saveSelectedPreset()
{
    const QString presetPath = m_view->selectedPresetPath();
    if(m_rememberPreset && !presetPath.isEmpty()) {
        m_presetPath = presetPath;
    }
}

void ProjectMWidget::scanPresetLibrary()
{
    m_library.setRecursive(m_config.settings.scanRecursive);
    m_library.setRootDir(m_config.presetDir);
    m_library.scan();
}

void ProjectMWidget::selectRandomPreset()
{
    const auto& presets = m_library.presets();
    if(presets.empty()) {
        m_view->selectRandomPreset();
        saveSelectedPreset();
        return;
    }

    const int index = QRandomGenerator::global()->bounded(static_cast<int>(presets.size()));
    m_view->selectPreset(presets.at(static_cast<size_t>(index)).path);
    saveSelectedPreset();
}

void ProjectMWidget::setRememberPreset(bool remember)
{
    m_rememberPreset = remember;
    if(remember) {
        saveSelectedPreset();
    }
    updateActionState();
}

void ProjectMWidget::setPresetLocked(bool locked)
{
    m_view->setPresetLocked(locked);
    updateActionState();
}

void ProjectMWidget::setShuffle(bool shuffle)
{
    m_view->setShuffle(shuffle);
    updateActionState();
}

void ProjectMWidget::setPresetDuration(int seconds)
{
    m_config.settings.presetDuration = isPresetDuration(seconds) ? seconds : ProjectMSettings{}.presetDuration;
    m_config                         = normaliseConfig(m_config);
    m_view->applySettings(m_config.settings);
    updateActionState();
}

void ProjectMWidget::installSplitterEventFilters()
{
    // QOpenGLWindow is embedded as a native child window.
    // After it has grown, the window can overlap an ancestor splitter handle while the handle is dragged back across
    // it, stealing mouse events and preventing the splitter from shrinking. Watch the splitters so we can replace the
    // live window with a snapshot during a drag.

    const auto addObject = [this](QObject* object) {
        if(!object || std::ranges::any_of(m_splitterEventObjects, [object](const auto& watched) {
               return watched == object;
           })) {
            return;
        }

        object->installEventFilter(this);
        m_splitterEventObjects.emplace_back(object);
    };

    for(QWidget* widget = parentWidget(); widget; widget = widget->parentWidget()) {
        auto* splitter = qobject_cast<QSplitter*>(widget);
        if(!splitter) {
            continue;
        }

        addObject(splitter);
        for(int index{1}; index < splitter->count(); ++index) {
            addObject(splitter->handle(index));
        }
    }
}

void ProjectMWidget::beginSplitterResize()
{
    if(m_splitterResizeActive || m_fullScreenWindow) {
        return;
    }

    m_splitterResizeActive = true;
    qApp->installEventFilter(this);

    const QImage snapshot = m_view->grabFramebuffer();
    if(!snapshot.isNull()) {
        m_resizeSnapshot->setPixmap(QPixmap::fromImage(snapshot));
        updateResizeSnapshotGeometry();
        m_resizeSnapshot->show();
        m_resizeSnapshot->raise();
    }

    m_view->hide();
}

void ProjectMWidget::endSplitterResize()
{
    if(!m_splitterResizeActive) {
        return;
    }

    m_splitterResizeActive = false;
    qApp->removeEventFilter(this);
    m_resizeSnapshot->hide();
    m_resizeSnapshot->clear();

    if(m_viewContainer->isVisible() && !m_fullScreenWindow) {
        m_view->show();
        m_view->requestUpdate();
    }
}

void ProjectMWidget::updateResizeSnapshotGeometry()
{
    if(m_splitterResizeActive || m_resizeSnapshot->isVisible()) {
        m_resizeSnapshot->setGeometry(m_viewContainer->geometry());
    }
}

bool ProjectMWidget::isWindowWidget() const
{
    return parentWidget() == nullptr;
}

void ProjectMWidget::saveTopLevelState()
{
    QJsonObject state;
    saveLayoutData(state);
    state[GeometryKey] = QString::fromUtf8(saveGeometry().toBase64());

    FyStateSettings settings;
    settings.setValue(ProjectMWindowStateKey, state);
}

void ProjectMWidget::loadTopLevelState()
{
    const FyStateSettings settings;
    const QJsonObject state = settings.value(ProjectMWindowStateKey).toJsonObject();
    if(!state.isEmpty()) {
        loadLayoutData(state);
        if(state.contains(GeometryKey)) {
            restoreGeometry(QByteArray::fromBase64(state.value(GeometryKey).toString().toUtf8()));
        }
    }

    m_topLevelStateLoaded = true;
}
} // namespace Fooyin::ProjectM
