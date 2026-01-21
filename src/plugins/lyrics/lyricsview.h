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

#include <QListView>

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
class LyricsDelegate;
class LyricsModel;

class LyricsView : public QListView
{
    Q_OBJECT

public:
    explicit LyricsView(QWidget* parent = nullptr);

    void setDisplayString(const QString& string);

signals:
    void lineClicked(const QModelIndex& index, const QPoint& pos);
    void userScrolling();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QString m_displayString;
};
} // namespace Lyrics
} // namespace Fooyin
