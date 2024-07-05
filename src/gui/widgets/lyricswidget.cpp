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

#include "lyricswidget.h"

#include <QJsonObject>
#include <QTextEdit>
#include <QVBoxLayout>

#include <core/player/playercontroller.h>

namespace Fooyin {
LyricsWidget::LyricsWidget(PlayerController* playerController, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_lyricsTextArea{new QTextEdit(this)}
{
    setObjectName(LyricsWidget::name());

    m_lyricsTextArea->setAcceptRichText(false);
    m_lyricsTextArea->setReadOnly(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_lyricsTextArea);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &LyricsWidget::updateLyrics);

    updateLyrics(m_playerController->currentTrack());
}

QString LyricsWidget::name() const
{
    return tr("Lyrics");
}

QString LyricsWidget::layoutName() const
{
    return QStringLiteral("Lyrics");
}

void LyricsWidget::updateLyrics(const Track& track)
{
    if(!track.isValid()) {
        return;
    }

    const QString lyrics = track.extraTag(QStringLiteral("LYRICS")).join(QString{});
    m_lyricsTextArea->setText(lyrics);
}

} // namespace Fooyin
