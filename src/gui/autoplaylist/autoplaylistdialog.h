/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "autoplaylistquery.h"

#include <core/track.h>

#include <QBasicTimer>
#include <QDialog>
#include <QPointer>

#include <functional>

class QAction;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QTabWidget;
class QSplitter;

namespace Fooyin {
class ActionManager;
class AutoPlaylistRulesWidget;
class MusicLibrary;
class Playlist;
class PlaylistHandler;
class PlaylistWidget;
class ScriptLineEdit;
class ScriptTextEdit;

class AutoPlaylistDialog : public QDialog
{
    Q_OBJECT

public:
    AutoPlaylistDialog(ActionManager* actionManager, MusicLibrary* library, PlaylistHandler* playlisthandler,
                       Playlist* playlist, std::function<PlaylistWidget*(QWidget*)> createPreview,
                       QWidget* parent = nullptr);

    void done(int value) override;
    void accept() override;

Q_SIGNALS:
    void playlistCreated(Fooyin::Playlist* playlist);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void updateButtonState();
    void scheduleTrackCount();
    void startTrackCount();
    void setPreviewVisible(bool visible, bool resizeDialog = true);
    void updatePreview();
    void setQuery(const AutoPlaylistQuery& query);
    void rulesChanged();
    void changeMode(int index);

    [[nodiscard]] AutoPlaylistQuery currentQuery() const;
    std::vector<AutoPlaylistQuery>::iterator findQuery(const QString& name);
    void saveQuery();
    void loadQuery();
    void deleteQuery();
    void resetQueries();

    void saveQueries() const;
    void loadQueries();
    void populateQueries();
    void loadDefaultQueries();

    MusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    Playlist* m_playlist;
    std::vector<AutoPlaylistQuery> m_queries;
    QString m_lastPresetName;
    bool m_queryChanged;
    QString m_rulesQuery;
    QString m_rulesSortQuery;
    bool m_haveRules;

    QBasicTimer m_countTimer;
    QString m_countQuery;
    uint64_t m_countGeneration;
    bool m_countValid;
    bool m_countRunning;
    bool m_countPending;

    std::function<PlaylistWidget*(QWidget*)> m_createPreview;
    TrackList m_previewTracks;
    QPointer<PlaylistWidget> m_preview;
    QSplitter* m_splitter;
    QWidget* m_editor;
    QWidget* m_previewPanel;
    QAction* m_previewAction;
    int m_previewWidth;

    QLineEdit* m_nameEdit;
    QComboBox* m_presetBox;
    QTabWidget* m_modes;
    AutoPlaylistRulesWidget* m_rules;
    QLabel* m_statusLabel;
    QLabel* m_modeLabel;
    ScriptTextEdit* m_queryEdit;
    ScriptLineEdit* m_sortQueryEdit;
    QCheckBox* m_forceSorted;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;
    QPushButton* m_resetButton;
    QAction* m_formatAction;
    QPushButton* m_okButton;
};
} // namespace Fooyin
