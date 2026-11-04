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

#include "plugininstallhandler.h"

#include <core/application.h>
#include <core/corepaths.h>
#include <core/plugins/plugininstaller.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::PluginInstall {
namespace {
QString installErrorText(PluginInstaller::InstallError error)
{
    using PluginInstaller::InstallError;

    switch(error) {
        case InstallError::AlreadyInstalled:
            return QCoreApplication::translate("Fooyin::PluginPageWidget", "This plugin is already installed.");
        case InstallError::SameVersionInstalled:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "This version of the plugin is already installed.");
        case InstallError::InvalidSource:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "The selected plugin file does not exist or is not a regular file.");
        case InstallError::UnsupportedPlatform:
            return QCoreApplication::translate(
                "Fooyin::PluginPageWidget",
                "Plugin bundles are not supported on this operating system or processor architecture.");
        case InstallError::BundleReadFailed:
            return QCoreApplication::translate(
                "Fooyin::PluginPageWidget", "The plugin bundle could not be read. It may be damaged or inaccessible.");
        case InstallError::InvalidBundle:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "The plugin bundle contains invalid or unsupported files.");
        case InstallError::MissingPlatform:
            return QCoreApplication::translate(
                "Fooyin::PluginPageWidget",
                "The plugin bundle does not contain files for this operating system and processor architecture.");
        case InstallError::InvalidPlugin:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "No recognised fooyin plugin was found in the installation files.");
        case InstallError::IncompatibleApi:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "This plugin uses an incompatible version of the fooyin plugin API.");
        case InstallError::InvalidMetadata:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "The plugin has an invalid identifier or is missing its version.");
        case InstallError::MultiplePlugins:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "The bundle contains more than one fooyin plugin.");
        case InstallError::WriteFailed:
            return QCoreApplication::translate("Fooyin::PluginPageWidget",
                                               "The plugin files could not be written to the installation directory. "
                                               "Check that it is writable and that there is enough free disk space.");
    }
    return {};
}
} // namespace

QString fileFromMimeData(const QMimeData* mimeData, DropTarget target)
{
    if(!mimeData || !mimeData->hasUrls()) {
        return {};
    }

    const QList<QUrl> urls = mimeData->urls();
    if(urls.size() != 1 || !urls.front().isLocalFile()) {
        return {};
    }

    QString filepath = urls.front().toLocalFile();
    const QFileInfo file{filepath};
    if(!file.isFile()) {
        return {};
    }

    const QString suffix = file.suffix().toLower();
    if(suffix == "fyplugin"_L1) {
        return filepath;
    }
    if(target == DropTarget::PluginsPage && (suffix == "zip"_L1 || QLibrary::isLibrary(filepath))) {
        return filepath;
    }

    return {};
}

void promptForRestart(QWidget* parent, const QString& title)
{
    QMessageBox message{
        QMessageBox::Question, title,
        QCoreApplication::translate("Fooyin::PluginPageWidget", "Restart for changes to take effect. Restart now?"),
        QMessageBox::Yes | QMessageBox::No, parent};
    if(message.exec() == QMessageBox::Yes) {
        Application::restart();
    }
}

std::unique_ptr<PluginInfo> install(const QString& filepath, QWidget* parent, bool promptRestart)
{
    bool updating{false};
    bool reinstalling{false};
    const QString pluginsPath = Core::userPluginsPath();
    auto installResult        = PluginInstaller::install(filepath, pluginsPath, false);
    if(!installResult
       && (installResult.error() == PluginInstaller::InstallError::AlreadyInstalled
           || installResult.error() == PluginInstaller::InstallError::SameVersionInstalled)) {
        reinstalling = installResult.error() == PluginInstaller::InstallError::SameVersionInstalled;
        QMessageBox message{
            QMessageBox::Question, QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Already Installed"),
            reinstalling ? QCoreApplication::translate("Fooyin::PluginPageWidget",
                                                       "This version of the plugin is already installed. Reinstall it?")
                         : QCoreApplication::translate("Fooyin::PluginPageWidget",
                                                       "This plugin is already installed. Update it?"),
            QMessageBox::Yes | QMessageBox::No, parent};
        message.button(QMessageBox::Yes)
            ->setText(reinstalling ? QCoreApplication::translate("Fooyin::PluginPageWidget", "Reinstall")
                                   : QCoreApplication::translate("Fooyin::PluginPageWidget", "Update"));
        if(message.exec() != QMessageBox::Yes) {
            return {};
        }
        updating      = !reinstalling;
        installResult = PluginInstaller::install(filepath, pluginsPath, true);
    }

    if(!installResult) {
        QMessageBox::critical(parent,
                              QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Installation Failed"),
                              installErrorText(installResult.error()));
        return {};
    }

    if(promptRestart) {
        QString title = QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Installed");
        if(reinstalling) {
            title = QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Reinstalled");
        }
        else if(updating) {
            title = QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Updated");
        }
        promptForRestart(parent, title);
    }

    return std::move(*installResult);
}
} // namespace Fooyin::PluginInstall
