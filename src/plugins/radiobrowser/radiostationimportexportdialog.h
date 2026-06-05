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

#include "radiostationstore.h"

#include <QObject>
#include <QPointer>

class QProgressDialog;
class QWidget;

namespace Fooyin::RadioBrowser {
class RadioBrowserController;

class RadioStationImportExportDialog : public QObject
{
    Q_OBJECT

public:
    static void importStations(RadioBrowserController* controller, QWidget* parent);
    static void exportStations(RadioBrowserController* controller, QWidget* parent);

private:
    RadioStationImportExportDialog(RadioBrowserController* controller, QWidget* parent);

    void startImport();
    void handleImportFinished(QObject* context, SavedStationImportResult result);
    void handleImportFailed(QObject* context, const QString& error);

    static QString stationsFileFilter();

    QPointer<RadioBrowserController> m_controller;
    QPointer<QWidget> m_parent;
    QPointer<QProgressDialog> m_progress;
    bool m_finished{false};
};
} // namespace Fooyin::RadioBrowser
