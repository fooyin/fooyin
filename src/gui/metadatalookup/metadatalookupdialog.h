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

#include "metadataapply.h"

#include <QDialog>
#include <QPointer>

#include <functional>
#include <optional>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableView;
class QWidget;

namespace Fooyin {
class AudioLoader;
class ElapsedProgressDialog;
class MusicLibrary;
class NetworkAccessManager;
class SettingsManager;

class LookupResultsModel;
class MetadataLookupRegistry;
class MetadataLookupSource;
class RetrievedTrackModel;
class TrackMatchModel;

class FYGUI_EXPORT MetadataLookupDialog : public QDialog
{
    Q_OBJECT

public:
    MetadataLookupDialog(TrackList tracks, MusicLibrary* library, std::shared_ptr<AudioLoader> audioLoader,
                         std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings, LookupMode mode,
                         QWidget* parent = nullptr);
    MetadataLookupDialog(TrackList tracks, std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                         const LookupQuery& initialQuery, QWidget* parent = nullptr);
    ~MetadataLookupDialog() override;

    [[nodiscard]] bool hasSameTracks(const TrackList& tracks) const;
    void startLookup(LookupMode mode);

Q_SIGNALS:
    void tracksApplied(const Fooyin::TrackList& tracks);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class Purpose : uint8_t
    {
        WriteFiles = 0,
        ReturnTracks,
    };

    MetadataLookupDialog(TrackList tracks, MusicLibrary* library, std::shared_ptr<AudioLoader> audioLoader,
                         std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings, LookupMode mode,
                         Purpose purpose, const std::optional<LookupQuery>& initialQuery, QWidget* parent);

    void buildUi();
    void connectSource(MetadataLookupSource* source);
    void setSource(const QString& sourceId);

    void updateLookupModes();
    void setLookupMode(LookupMode mode);
    void setLookupQuery(const LookupQuery& query);
    void updateLookupEditor();
    void saveState();
    void restoreState();

    void startSearch();
    void selectRelease(const QModelIndex& current);
    void setRelease(const Release& release);
    void updateReleaseInformation(const ReleaseSummary& release);
    void selectMatchingRow(int row);

    void updatePreview();
    void showChanges();
    void updateActions();
    void applyMetadata();
    void writeMetadata();
    [[nodiscard]] bool confirmMetadataWipe();

    [[nodiscard]] LookupQuery query() const;
    [[nodiscard]] MetadataApplyOptions applyOptions() const;
    [[nodiscard]] bool canWriteAllTracks() const;

    TrackList m_tracks;
    MusicLibrary* m_library;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<NetworkAccessManager> m_network;
    SettingsManager* m_settings;
    Purpose m_purpose;
    QString m_discId;

    std::unique_ptr<MetadataLookupRegistry> m_registry;
    MetadataLookupSource* m_client;
    LookupResultsModel* m_resultsModel;
    RetrievedTrackModel* m_retrievedModel;
    TrackMatchModel* m_matchModel;

    QComboBox* m_source;
    QComboBox* m_lookupMode;
    QComboBox* m_idType;
    QStackedWidget* m_lookupEditor;
    QStackedWidget* m_identifierEditor;
    QLineEdit* m_artist;
    QLineEdit* m_album;
    QLineEdit* m_discToc;
    QLineEdit* m_releaseId;
    QLineEdit* m_releaseGroupId;
    QPushButton* m_search;
    QLabel* m_status;
    QLabel* m_releaseArtist;
    QLabel* m_releaseAlbum;
    QLabel* m_releaseDate;
    QLabel* m_releaseOriginalDate;
    QLabel* m_releaseCountry;
    QLabel* m_releaseLabel;
    QLabel* m_releaseCatalogNumber;
    QLabel* m_releaseBarcode;
    QLabel* m_releaseFormat;
    QLabel* m_releaseType;
    QLabel* m_releaseStatus;
    QLabel* m_releaseDisambiguation;
    QTableView* m_resultsView;
    QTableView* m_retrievedView;
    QTableView* m_matchView;
    QComboBox* m_policy;
    QCheckBox* m_allowUnresolved;
    QCheckBox* m_writeGenres;
    QCheckBox* m_writeIds;
    QCheckBox* m_originalDate;
    QPushButton* m_changesButton;
    QSplitter* m_splitter;
    QSplitter* m_resultsSplitter;
    QSplitter* m_matchSplitter;
    QDialogButtonBox* m_buttons;
    QPushButton* m_updateFiles;
    QPushButton* m_close;

    MetadataApplyResult m_applyResult;
    bool m_releaseLoaded;
    bool m_busy;
    bool m_writeInProgress;
    bool m_writeCompleted;
    bool m_syncingTrackScrollbars;
    std::function<void()> m_cancelWrite;
    QPointer<ElapsedProgressDialog> m_progressDialog;
};
} // namespace Fooyin
