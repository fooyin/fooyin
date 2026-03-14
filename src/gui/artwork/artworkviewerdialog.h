/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

class QAction;

namespace Fooyin {
class ArtworkView;

class ArtworkViewerDialog : public QDialog
{
    Q_OBJECT

public:
    ArtworkViewerDialog(const Track& track, const QPixmap& cover, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

    void setCover(const QPixmap& cover);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    ArtworkView* m_view;

    QAction* m_fitToWindowAction;
    QAction* m_actualSizeAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;

    QPixmap m_cover;
};
} // namespace Fooyin
