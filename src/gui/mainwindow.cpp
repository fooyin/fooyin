/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "mainwindow.h"

#include "internalguisettings.h"
#include "menubar/mainmenubar.h"
#include "plugininstallhandler.h"
#include "scanprogresstext.h"
#include "toolbarmanager.h"
#include "widgets/statuswidget.h"

#include <core/application.h>
#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/statusevent.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/enum.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QWindow>

using namespace Qt::StringLiterals;

constexpr auto MainWindowPrevState = "Interface/PrevState"_L1;
constexpr auto MainWindowGeometry  = "MainWindow/Geometry"_L1;
constexpr auto MainWindowSize      = "MainWindow/Size"_L1;

namespace Fooyin {
MainWindow::MainWindow(ActionManager* actionManager, MainMenuBar* menubar, MusicLibrary* library,
                       WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent)
    : QMainWindow{parent}
    , m_actionManager{actionManager}
    , m_mainMenu{menubar}
    , m_library{library}
    , m_settings{settings}
    , m_toolbarManager{nullptr}
    , m_prevState{Normal}
    , m_state{Normal}
    , m_isHiding{false}
    , m_hasQuit{false}
    , m_showStatusTips{m_settings->value<Settings::Gui::ShowStatusTips>()}
    , m_altPressed{false}
{
    setAcceptDrops(true);
    qApp->installEventFilter(this);

    actionManager->setMainWindow(this);
    m_toolbarManager
        = std::make_unique<ToolbarManager>(this, m_mainMenu->menuBar(), actionManager, widgetProvider, settings, this);
    m_settings->createSettingsDialog(this);

    const FyStateSettings stateSettings;
    m_settings->settingsDialog()->restoreState(stateSettings);

    if(auto prevState = stateSettings.value(MainWindowPrevState).toString(); !prevState.isEmpty()) {
        if(auto state = Utils::Enum::fromString<WindowState>(prevState)) {
            m_prevState = state.value();
        }
    }

    resetTitle();

    setWindowIcon(Gui::applicationIcon());

    if(windowHandle()) {
        QObject::connect(windowHandle(), &QWindow::screenChanged, this,
                         [this]() { m_settings->set<Settings::Gui::MainWindowPixelRatio>(devicePixelRatioF()); });
    }

    m_settings->subscribe<Settings::Gui::ShowStatusTips>(this, [this](const bool show) { m_showStatusTips = show; });
    m_settings->subscribe<Settings::Gui::ShowMenuBar>(this, [this](const bool show) {
        if(show && m_altMenuBar) {
            m_altMenuBar->close();
        }
        m_toolbarManager->setMenuVisible(show);
    });
    QObject::connect(m_library, &MusicLibrary::scanProgress, this, &MainWindow::showScanProgress);

    m_toolbarManager->setMenuVisible(m_settings->value<Settings::Gui::ShowMenuBar>());
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    exit();
}

void MainWindow::open()
{
    switch(m_settings->value<Settings::Gui::StartupBehaviour>()) {
        case StartMaximised:
            showMaximized();
            break;
        case StartPrev:
            restoreState(m_prevState);
            break;
        case StartHidden:
            m_state = Hidden;
            break;
        case StartNormal:
        default:
            show();
            break;
    }
}

void MainWindow::raiseWindow()
{
    raise();
    show();
    activateWindow();

    m_state = currentState();
}

void MainWindow::toggleVisibility()
{
    if(m_state == Hidden) {
        hideToTray(false);
    }
    else if(isActiveWindow()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        hideToTray(true);
    }
    else if(isMinimized()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        hideToTray(false);
    }
    else if(!isVisible()) {
        show();
        activateWindow();
    }
    else {
        activateWindow();
        raise();
    }
}

void MainWindow::exit()
{
    if(!m_hasQuit) {
        m_hasQuit = true;

        FyStateSettings stateSettings;
        m_settings->settingsDialog()->saveState(stateSettings);

        saveWindowGeometry();

        stateSettings.setValue(MainWindowPrevState, Utils::Enum::toString(currentState()));

        m_settings->set<Settings::Core::Shutdown>(true);
    }
}

void MainWindow::setTitle(const QString& title)
{
    QString windowTitle{title};
    if(m_settings->value<Settings::Gui::LayoutEditing>()) {
        windowTitle.append(u" - "_s + tr("Layout Editing Mode"));
    }
    setWindowTitle(windowTitle);
}

void MainWindow::resetTitle()
{
    setTitle(u"fooyin"_s);
}

void MainWindow::showHiddenMenu(QAction* activeAction, QWidget* anchor)
{
    if(!m_altMenuBar) {
        m_altMenuBar = new QMenu(this);
    }

    m_mainMenu->populateMenu(m_altMenuBar);
    if(m_altMenuBar->isEmpty()) {
        return;
    }

    const QPoint menuPosition = anchor ? anchor->mapToGlobal(QPoint{0, anchor->height()}) : frameGeometry().topLeft();
    m_altMenuBar->popup(menuPosition);
    if(activeAction) {
        m_altMenuBar->setActiveAction(activeAction);
    }
}

void MainWindow::installStatusWidget(StatusWidget* statusWidget)
{
    m_statusWidget = statusWidget;
}

ToolbarManager* MainWindow::toolbarManager() const
{
    return m_toolbarManager.get();
}

QSize MainWindow::sizeHint() const
{
    return Utils::proportionateSize(this, 0.6, 0.6);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    auto* target = qobject_cast<QWidget*>(watched);
    if(!target) {
        return QMainWindow::eventFilter(watched, event);
    }

    if(m_altMenuBar && m_altMenuBar->isVisible() && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Alt
           || (keyEvent->key() == Qt::Key_F10 && keyEvent->modifiers() == Qt::NoModifier)) {
            m_altPressed = false;
            m_altMenuBar->close();
            return true;
        }
    }

    if(target->window() == this && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
       && handleHiddenMenuKeyEvent(static_cast<QKeyEvent*>(event))) {
        return true;
    }

    if(target == this && event->type() == QEvent::WindowDeactivate) {
        m_altPressed = false;
    }

    if(target->window() != this
       || (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto* dropEvent = static_cast<QDropEvent*>(event);
    const QString filepath
        = PluginInstall::fileFromMimeData(dropEvent->mimeData(), PluginInstall::DropTarget::MainWindow);
    if(filepath.isEmpty()) {
        return QMainWindow::eventFilter(watched, event);
    }

    dropEvent->setDropAction(Qt::CopyAction);
    dropEvent->accept();
    if(event->type() == QEvent::Drop) {
        PluginInstall::install(filepath, this);
    }

    return true;
}

bool MainWindow::event(QEvent* event)
{
    if(!m_statusWidget) {
        return QMainWindow::event(event);
    }

    if(m_showStatusTips && event->type() == QEvent::StatusTip) {
        const QString tip = static_cast<QStatusTipEvent*>(event)->tip();
        m_statusWidget->showStatusTip(tip);
    }
    else if(event->type() == StatusEvent::StatusEventType) {
        const auto* status = static_cast<StatusEvent*>(event);
        if(status->timeout() >= 0) {
            m_statusWidget->showTempMessage(status->message(), status->timeout());
        }
        else {
            m_statusWidget->showTempMessage(status->message());
        }
        return true;
    }

    return QMainWindow::event(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_settings->set<Settings::Gui::MainWindowPixelRatio>(devicePixelRatioF());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if(!m_isHiding) {
        if(event->spontaneous()) {
            const bool canHide = m_settings->value<Settings::Gui::Internal::ShowTrayIcon>()
                              && m_settings->value<Settings::Gui::Internal::TrayOnClose>();
            if(!isHidden() && QSystemTrayIcon::isSystemTrayAvailable() && canHide) {
                hideToTray(true);
            }
            else {
                exit();
            }
        }
        else {
            exit();
        }
    }

    event->accept();
    QMainWindow::closeEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::BackButton) {
        if(auto* prevCmd = m_actionManager->command(Constants::Actions::Previous)) {
            prevCmd->action()->activate(QAction::Trigger);
        }
    }
    if(event->button() == Qt::ForwardButton) {
        if(auto* nextCmd = m_actionManager->command(Constants::Actions::Next)) {
            nextCmd->action()->activate(QAction::Trigger);
        }
    }
    QMainWindow::mousePressEvent(event);
}

bool MainWindow::handleHiddenMenuKeyEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_F10 && event->modifiers() == Qt::NoModifier) {
        if(event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            if(m_settings->value<Settings::Gui::ShowMenuBar>()) {
                const auto menuActions = menuBar()->actions();
                for(auto* action : menuActions) {
                    if(action->isVisible() && action->isEnabled() && action->menu()) {
                        menuBar()->setActiveAction(action);
                        const QPoint menuPosition
                            = menuBar()->mapToGlobal(menuBar()->actionGeometry(action).bottomLeft());
                        action->menu()->popup(menuPosition);
                        break;
                    }
                }
            }
            else {
                showHiddenMenu();
            }
        }
        return true;
    }

    if(m_settings->value<Settings::Gui::ShowMenuBar>()) {
        m_altPressed = false;
        return false;
    }

    if(event->key() == Qt::Key_Alt) {
        if(event->isAutoRepeat()) {
            return true;
        }

        if(event->type() == QEvent::KeyPress) {
            m_altPressed = true;
        }
        else if(std::exchange(m_altPressed, false)) {
            showHiddenMenu();
        }
        return true;
    }

    if(event->type() != QEvent::KeyPress || event->modifiers() != Qt::AltModifier) {
        m_altPressed = false;
        return false;
    }

    m_altPressed = false;

    const QKeySequence pressedKey = event->keyCombination();
    const auto menuActions        = menuBar()->actions();
    for(auto* action : menuActions) {
        if(action->isVisible() && action->isEnabled() && action->menu()
           && QKeySequence::mnemonic(action->text()) == pressedKey) {
            showHiddenMenu(action);
            return true;
        }
    }

    return false;
}

void MainWindow::showScanProgress(const ScanProgress& progress)
{
    if(!m_statusWidget) {
        return;
    }

    if(progress.id < 0) {
        m_statusWidget->setScanProgress({});
        return;
    }

    QString scanText = ScanProgressText::requestText(progress, "StatusWidget");

    if(const QString phaseText = ScanProgressText::phaseText(progress, "StatusWidget"); !phaseText.isEmpty()) {
        scanText += u": "_s + phaseText;
    }

    if(progress.discovered > 0) {
        scanText += u": "_s + ScanProgressText::discoveredText(progress.discovered, "StatusWidget");
    }

    if(progress.phase == ScanProgress::Phase::Finished) {
        m_statusWidget->setScanProgress({});
        return;
    }

    m_statusWidget->setScanProgress(scanText,
                                    [library = m_library, scanId = progress.id]() { library->cancelScan(scanId); });
}

MainWindow::WindowState MainWindow::currentState()
{
    if(isHidden()) {
        return Hidden;
    }
    if(isMaximized()) {
        return Maximised;
    }

    return Normal;
}

void MainWindow::saveWindowGeometry()
{
    FyStateSettings stateSettings;
    stateSettings.setValue(MainWindowGeometry, saveGeometry());
    stateSettings.setValue(MainWindowSize, size());
}

void MainWindow::restoreWindowGeometry()
{
    const FyStateSettings stateSettings;
    if(const auto geometry = stateSettings.value(MainWindowGeometry).toByteArray(); !geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if(const auto savedSize = stateSettings.value(MainWindowSize).toSize(); savedSize.isValid()) {
        resize(savedSize);
    }
}

void MainWindow::restoreState(WindowState state)
{
    switch(state) {
        case Normal: {
            restoreWindowGeometry();
            show();
            break;
        }
        case Maximised:
            showMaximized();
            break;
        case Hidden:
            m_state = Hidden;
            break;
    }
}

void MainWindow::hideToTray(bool hide)
{
    if(hide) {
        m_isHiding  = true;
        m_prevState = std::exchange(m_state, Hidden);
        saveWindowGeometry();
        close();
        m_isHiding = false;
    }
    else {
        if(m_prevState == Maximised) {
            m_state = m_prevState;
            showMaximized();
        }
        else {
            m_state = Normal;
            restoreWindowGeometry();
            show();
        }
    }
}
} // namespace Fooyin

#include "moc_mainwindow.cpp"
