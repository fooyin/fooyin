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

#include "internalguisettings.h"

#include <core/scripting/scriptparser.h>
#include <core/track.h>

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;

namespace Fooyin {
class ArtworkFinder;
class ArtworkModel;
class ExpandedTreeView;
class MusicLibrary;
class NetworkAccessManager;
class SettingsManager;

class ArtworkDialog : public QDialog
{
    Q_OBJECT

public:
    ArtworkDialog(std::shared_ptr<NetworkAccessManager> networkManager, MusicLibrary* library,
                  SettingsManager* settings, TrackList tracks, Track::Cover type, QWidget* parent = nullptr);

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void searchArtwork();
    void searchFinished();

    std::shared_ptr<NetworkAccessManager> m_networkManager;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    TrackList m_tracks;
    Track::Cover m_type;

    ArtworkFinder* m_artworkFinder;
    ExpandedTreeView* m_view;
    ArtworkModel* m_model;
    ScriptParser m_parser;
    ArtworkSaveMethods m_saveMethods;

    QLabel* m_status;
    QLineEdit* m_artist;
    QLineEdit* m_album;
    QComboBox* m_searchType;
    QPushButton* m_startSearch;
};
} // namespace Fooyin
