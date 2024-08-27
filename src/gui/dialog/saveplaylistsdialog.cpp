/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "saveplaylistsdialog.h"

#include "internalguisettings.h"

#include <core/application.h>
#include <core/internalcoresettings.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistparser.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fooyin {
SavePlaylistsDialog::SavePlaylistsDialog(Application* core, QWidget* parent)
    : QDialog{parent}
    , m_core{core}
    , m_settings{core->settingsManager()}
    , m_formats{new QComboBox(this)}
{
    for(const QString& ext : m_core->playlistLoader()->supportedSaveExtensions()) {
        m_formats->addItem(ext);
    }

    auto* formatLabel = new QLabel(tr("Playlist file format") + u":", this);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(formatLabel);
    layout->addWidget(m_formats);
    layout->addWidget(buttonBox);
}

void SavePlaylistsDialog::accept()
{
    QString dir{QDir::homePath()};
    if(const auto lastPath = m_settings->fileValue(Settings::Gui::Internal::LastFilePath).toString();
       !lastPath.isEmpty()) {
        dir = lastPath;
    }

    QLoggingCategory log{"fy.saveplaylists"};

    const auto saveDir = QFileDialog::getExistingDirectory(Utils::getMainWindow(), tr("Select Directory"), dir);
    if(saveDir.isEmpty()) {
        qCInfo(log) << "Could not save playlists: Invalid directory";
        QDialog::accept();
        return;
    }

    m_settings->fileSet(Settings::Gui::Internal::LastFilePath, saveDir);

    const QString extension = m_formats->currentText();
    if(extension.isEmpty()) {
        qCInfo(log) << "Could not save playlists: Invalid format";
        QDialog::accept();
        return;
    }

    auto* parser = m_core->playlistLoader()->parserForExtension(extension);
    if(!parser) {
        qCInfo(log) << "Could not save playlists: No parser found for" << extension;
        QDialog::accept();
        return;
    }

    const auto playlists = m_core->playlistHandler()->playlists();
    for(const auto* playlist : playlists) {
        if(playlist->isTemporary() || playlist->trackCount() == 0) {
            continue;
        }

        const QString filepath = QStringLiteral("%1/%2.%3").arg(saveDir, playlist->name(), extension);
        QFile playlistFile{QDir::cleanPath(filepath)};
        if(!playlistFile.open(QIODevice::WriteOnly)) {
            qCWarning(log) << "Could not open playlist file" << filepath
                           << "for writing:" << playlistFile.errorString();
            QDialog::accept();
            return;
        }

        const QFileInfo info{playlistFile};
        const QDir playlistDir{info.absolutePath()};
        const auto pathType = static_cast<PlaylistParser::PathType>(
            m_settings->fileValue(Settings::Core::Internal::PlaylistSavePathType, 0).toInt());
        const bool writeMetadata
            = m_settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool();

        parser->savePlaylist(&playlistFile, extension, playlist->tracks(), playlistDir, pathType, writeMetadata);
    }

    QDialog::accept();
}
} // namespace Fooyin
