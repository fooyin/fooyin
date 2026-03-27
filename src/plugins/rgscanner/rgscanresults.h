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

#include <cstdint>

class QDialogButtonBox;
class QLabel;
class QTableView;

namespace Fooyin {
class MusicLibrary;
class SettingsManager;

namespace RGScanner {
class RGScanResultsModel;

enum class RGScanType : uint8_t
{
    Track = 0,
    SingleAlbum,
    Album
};

class RGScanResults : public QDialog
{
    Q_OBJECT

public:
    RGScanResults(MusicLibrary* library, SettingsManager* settings, TrackList originalTracks, TrackList tracks,
                  RGScanType scanType, std::chrono::milliseconds timeTaken, QWidget* parent = nullptr);

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    TrackList m_originalTracks;
    TrackList m_tracks;
    RGScanType m_scanType;
    OpusRGWriteMode m_opusWriteMode;

    QTableView* m_resultsView;
    RGScanResultsModel* m_resultsModel;
    QLabel* m_status;
    QDialogButtonBox* m_buttonBox;
};
} // namespace RGScanner
} // namespace Fooyin
