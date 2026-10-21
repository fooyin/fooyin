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

#include <QDialog>

class QAction;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QTabWidget;

namespace Fooyin {
class AutoPlaylistRulesWidget;
class Playlist;
class PlaylistHandler;
class ScriptLineEdit;
class ScriptTextEdit;

class AutoPlaylistDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoPlaylistDialog(QWidget* parent = nullptr);
    AutoPlaylistDialog(PlaylistHandler* playlisthandler, Playlist* playlist, QWidget* parent = nullptr);

    void done(int value) override;
    void accept() override;

Q_SIGNALS:
    void playlistEdited(const QString& name, const QString& query, const QString& sortQuery, bool forceSorted);

private:
    void updateButtonState();
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

    PlaylistHandler* m_playlistHandler;
    Playlist* m_playlist;
    std::vector<AutoPlaylistQuery> m_queries;
    bool m_queryChanged;
    QString m_rulesQuery;
    QString m_rulesSortQuery;
    bool m_haveRules;

    QLineEdit* m_nameEdit;
    QComboBox* m_presetBox;
    QTabWidget* m_modes;
    AutoPlaylistRulesWidget* m_rules;
    QLabel* m_errorLabel;
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
