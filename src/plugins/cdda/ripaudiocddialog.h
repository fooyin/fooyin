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

#include <core/track.h>
#include <gui/conversion/conversionservice.h>

#include <QDialog>

#include <vector>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Fooyin::Cdda {
struct AccurateRipTrackResult;

void showAccurateRipResults(QWidget* parent, const std::vector<AccurateRipTrackResult>& results,
                            const QString& message);

class RipAudioCdDialog final : public QDialog
{
    Q_OBJECT

public:
    RipAudioCdDialog(TrackList tracks, const std::vector<ConversionPresetInfo>& presets, bool metadataLookupAvailable,
                     bool driveOffsetConfigured, QWidget* parent = nullptr);

    void applyLookupTracks(const TrackList& tracks);
    void metadataLookupFinished();
    void ripStartFailed();

Q_SIGNALS:
    void metadataLookupRequested(const Fooyin::TrackList& tracks);
    void ripRequested(const Fooyin::TrackList& tracks, const QString& presetId, bool showSetup, bool verifyAccurateRip);

private:
    [[nodiscard]] TrackList selectedTracks() const;
    [[nodiscard]] TrackList tracksFromTable(bool selectedOnly) const;

    void updateActions();

    void showMetadataLookup();
    void showConverterSetup();

    void ripWithPreset();
    void requestRip(bool showSetup);

    TrackList m_tracks;
    bool m_metadataLookupAvailable;
    bool m_metadataLookupPending;
    bool m_ripPending;

    QLineEdit* m_albumArtist;
    QLineEdit* m_albumTitle;
    QLineEdit* m_discNumber;
    QLineEdit* m_genre;
    QLineEdit* m_date;
    QTableWidget* m_trackTable;
    QComboBox* m_presets;
    QCheckBox* m_verifyAccurateRip;
    QLabel* m_accurateRipStatus;
    QPushButton* m_lookupButton;
    QPushButton* m_setupButton;
    QPushButton* m_ripButton;
};
} // namespace Fooyin::Cdda
