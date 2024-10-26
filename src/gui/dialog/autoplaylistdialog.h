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

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace Fooyin {
class Playlist;
class PlaylistHandler;
class ScriptTextEdit;

struct AutoPlaylistQuery
{
    QString name;
    QString query;

    friend QDataStream& operator<<(QDataStream& stream, const AutoPlaylistQuery& preset)
    {
        static constexpr int version{1};
        stream << version;
        stream << preset.name;
        stream << preset.query;

        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, AutoPlaylistQuery& preset)
    {
        int version{0};
        stream >> version;
        stream >> preset.name;
        stream >> preset.query;

        return stream;
    }
};

class AutoPlaylistDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoPlaylistDialog(QWidget* parent = nullptr);
    AutoPlaylistDialog(PlaylistHandler* playlisthandler, Playlist* playlist, QWidget* parent = nullptr);

    void done(int value) override;
    void accept() override;

signals:
    void playlistEdited(const QString& name, const QString& query);

private:
    void updateButtonState() const;

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

    QComboBox* m_queryBox;
    ScriptTextEdit* m_queryEdit;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;
    QPushButton* m_resetButton;
    QPushButton* m_okButton;
};
} // namespace Fooyin
