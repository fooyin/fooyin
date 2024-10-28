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

#include <core/library/musiclibrary.h>
#include <gui/propertiesdialog.h>

#include <QFutureWatcher>

class QGridLayout;
class QLabel;
class QPushButton;

namespace Fooyin {
class ArtworkRow;
class AudioLoader;
class MusicLibrary;

class ArtworkProperties : public PropertiesTabWidget
{
    Q_OBJECT

public:
    ArtworkProperties(AudioLoader* loader, MusicLibrary* library, TrackList tracks, bool readOnly,
                      QWidget* parent = nullptr);
    ~ArtworkProperties() override;

    void loadTrackArtwork();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void apply() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    AudioLoader* m_audioLoader;
    MusicLibrary* m_library;

    TrackList m_tracks;
    QFutureWatcher<void>* m_watcher;
    bool m_loading;
    bool m_writing;

    QWidget* m_artworkWidget;
    std::array<ArtworkRow*, 3> m_rows;
    std::optional<WriteRequest> m_writeRequest;
};
} // namespace Fooyin
