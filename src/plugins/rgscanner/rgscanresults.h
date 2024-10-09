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

#pragma once

#include <core/track.h>

#include <QDialog>

class QTableView;

namespace Fooyin {
class MusicLibrary;

namespace RGScanner {
class RGScanResultsModel;

class RGScanResults : public QDialog
{
    Q_OBJECT

public:
    RGScanResults(MusicLibrary* library, TrackList tracks, QWidget* parent = nullptr);

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    MusicLibrary* m_library;
    TrackList m_tracks;

    QTableView* m_resultsView;
    RGScanResultsModel* m_resultsModel;
};
} // namespace RGScanner
} // namespace Fooyin
