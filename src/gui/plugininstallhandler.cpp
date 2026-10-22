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
    const QString pluginsPath = Core::userPluginsPath();
    auto installResult        = PluginInstaller::install(filepath, pluginsPath, false);
    if(!installResult && installResult.error() == PluginInstaller::InstallError::AlreadyInstalled) {
        QMessageBox message{
            QMessageBox::Question, QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Already Installed"),
            QCoreApplication::translate("Fooyin::PluginPageWidget", "This plugin is already installed. Update it?"),
            QMessageBox::Yes | QMessageBox::No, parent};
        message.button(QMessageBox::Yes)->setText(QCoreApplication::translate("Fooyin::PluginPageWidget", "Update"));
        if(message.exec() != QMessageBox::Yes) {
            return {};
        }
        updating      = true;
        installResult = PluginInstaller::install(filepath, pluginsPath, true);
    }

    if(!installResult) {
        QMessageBox::critical(
            parent, QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Installation Failed"),
            QCoreApplication::translate("Fooyin::PluginPageWidget", "The plugin could not be installed."));
        return {};
    }

    if(promptRestart) {
        const QString title = updating ? QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Updated")
                                       : QCoreApplication::translate("Fooyin::PluginPageWidget", "Plugin Installed");
        promptForRestart(parent, title);
    }

    return std::move(*installResult);
}
} // namespace Fooyin::PluginInstall
