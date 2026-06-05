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

#include "radiostationimportexportdialog.h"

#include "radiobrowsercontroller.h"

#include <gui/statusevent.h>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace {
QString defaultStationsFilePath()
{
    return QDir{QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)}.filePath(u"stations.m3u8"_s);
}
} // namespace

namespace Fooyin::RadioBrowser {
void RadioStationImportExportDialog::importStations(RadioBrowserController* controller, QWidget* parent)
{
    auto* dialog = new RadioStationImportExportDialog{controller, parent};
    dialog->startImport();
}

void RadioStationImportExportDialog::exportStations(RadioBrowserController* controller, QWidget* parent)
{
    const QFileInfo defaultFile{defaultStationsFilePath()};

    QFileDialog saveDialog{parent, tr("Export Radio Stations"), defaultFile.absolutePath(), stationsFileFilter()};
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setOption(QFileDialog::DontResolveSymlinks);
    saveDialog.setDefaultSuffix(u"m3u8"_s);
    saveDialog.selectFile(defaultFile.fileName());

    if(saveDialog.exec() == 0) {
        return;
    }

    const QStringList selectedFiles = saveDialog.selectedFiles();
    if(selectedFiles.isEmpty() || selectedFiles.constFirst().isEmpty()) {
        return;
    }

    QString error;
    if(!controller->exportSavedStationsToM3u(selectedFiles.constFirst(), &error)) {
        QMessageBox::warning(parent, tr("Export Radio Stations"), error);
        return;
    }

    StatusEvent::post(
        tr("Exported %n radio station(s).", nullptr, static_cast<int>(controller->savedStations().size())));
}

RadioStationImportExportDialog::RadioStationImportExportDialog(RadioBrowserController* controller, QWidget* parent)
    : QObject{parent}
    , m_controller{controller}
    , m_parent{parent}
{ }

void RadioStationImportExportDialog::startImport()
{
    QMessageBox modeBox{QMessageBox::Question, tr("Import Radio Stations"),
                        tr("Append imported stations to My Stations, or replace the current list?"),
                        QMessageBox::Cancel, m_parent};
    auto* appendButton  = modeBox.addButton(tr("Append"), QMessageBox::AcceptRole);
    auto* replaceButton = modeBox.addButton(tr("Replace"), QMessageBox::DestructiveRole);
    modeBox.setDefaultButton(qobject_cast<QPushButton*>(appendButton));
    modeBox.exec();

    SavedStationImportMode mode;
    if(modeBox.clickedButton() == appendButton) {
        mode = SavedStationImportMode::Append;
    }
    else if(modeBox.clickedButton() == replaceButton) {
        mode = SavedStationImportMode::Replace;
    }
    else {
        deleteLater();
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        m_parent, tr("Import Radio Stations"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        stationsFileFilter(), nullptr, QFileDialog::DontResolveSymlinks);
    if(filePath.isEmpty()) {
        deleteLater();
        return;
    }

    QObject::connect(m_controller, &RadioBrowserController::savedStationsImportFinished, this,
                     &RadioStationImportExportDialog::handleImportFinished);
    QObject::connect(m_controller, &RadioBrowserController::savedStationsImportFailed, this,
                     &RadioStationImportExportDialog::handleImportFailed);

    QProgressDialog progress{tr("Importing radio stations…"), {}, 0, 0, m_parent};
    progress.setModal(true);
    progress.setCancelButton(nullptr);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    m_progress = &progress;

    m_controller->importSavedStationsFromM3u(filePath, mode, this);

    if(!m_finished) {
        progress.exec();
    }

    m_progress = nullptr;
    deleteLater();
}

void RadioStationImportExportDialog::handleImportFinished(QObject* context, const SavedStationImportResult result)
{
    if(context != this) {
        return;
    }

    m_finished = true;
    if(m_progress) {
        m_progress->close();
    }

    if(result.skipped > 0) {
        StatusEvent::post(tr("Imported %Ln radio station(s), skipped %Ln duplicate(s).", nullptr, result.imported)
                              .arg(result.skipped));
    }
    else {
        StatusEvent::post(tr("Imported %Ln radio station(s).", nullptr, result.imported));
    }
}

void RadioStationImportExportDialog::handleImportFailed(QObject* context, const QString& error)
{
    if(context != this) {
        return;
    }

    m_finished = true;
    if(m_progress) {
        m_progress->close();
    }

    QMessageBox::warning(m_parent, tr("Import Radio Stations"), error);
}

QString RadioStationImportExportDialog::stationsFileFilter()
{
    return tr("M3U Playlists (*.m3u8);;All Files (*)");
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiostationimportexportdialog.cpp"
