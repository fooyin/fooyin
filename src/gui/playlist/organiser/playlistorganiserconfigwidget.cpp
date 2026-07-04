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

#include "playlistorganiserconfigwidget.h"

#include <gui/widgets/colourbutton.h>
#include <gui/widgets/scriptlineedit.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPalette>

using namespace Qt::StringLiterals;

namespace {
QString normaliseColour(const QString& colour)
{
    const QColor parsedColour{colour};
    return parsedColour.isValid() ? parsedColour.name(QColor::HexArgb) : QString{};
}

QColor colourOrDefault(const QString& colour, const QColor& defaultColour)
{
    const QColor parsedColour{colour};
    return parsedColour.isValid() ? parsedColour : defaultColour;
}

QColor defaultPlayingBackgroundColour(const QWidget* widget)
{
    QColor colour = widget->palette().highlight().color();
    colour.setAlpha(90);
    return colour;
}
} // namespace

namespace Fooyin {
PlaylistOrganiserConfigDialog::PlaylistOrganiserConfigDialog(PlaylistOrganiser* organiser, QWidget* parent)
    : WidgetConfigDialog{organiser, tr("Playlist Organiser Settings"), parent}
    , m_leftScript{new ScriptLineEdit(this)}
    , m_rightScript{new ScriptLineEdit(this)}
    , m_playingTextColour{new ColourButton(tr("Playing text"), true, this)}
    , m_playingBackgroundColour{new ColourButton(tr("Playing background"), true, this)}
{
    auto* scriptGroup       = new QGroupBox(tr("Scripts"), this);
    auto* scriptGroupLayout = new QGridLayout(scriptGroup);
    auto* scriptHint        = new QLabel(
        u"🛈 "_s
            + tr("Available variables: <code>%node_name%</code>, <code>%is_group%</code>, <code>%count%</code>, "
                 "<code>%playlist_size%</code>, <code>%playlist_duration%</code>"),
        this);
    scriptHint->setTextFormat(Qt::RichText);
    scriptHint->setWordWrap(true);

    //: Refers to the left script field.
    scriptGroupLayout->addWidget(new QLabel(tr("Left") + u":"_s, this), 0, 0);
    scriptGroupLayout->addWidget(m_leftScript, 0, 1);
    //: Refers to the right script field.
    scriptGroupLayout->addWidget(new QLabel(tr("Right") + u":"_s, this), 1, 0);
    scriptGroupLayout->addWidget(m_rightScript, 1, 1);
    scriptGroupLayout->addWidget(scriptHint, 2, 0, 1, 2);
    scriptGroupLayout->setColumnStretch(1, 1);

    auto* appearanceGroup       = new QGroupBox(tr("Colours"), this);
    auto* appearanceGroupLayout = new QGridLayout(appearanceGroup);

    ColourButton::alignLabels({m_playingTextColour, m_playingBackgroundColour});

    appearanceGroupLayout->addWidget(m_playingTextColour, 0, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_playingBackgroundColour, 1, 0, 1, 2);
    appearanceGroupLayout->setColumnStretch(1, 1);

    auto* layout = contentLayout();

    int row{0};
    layout->addWidget(scriptGroup, row++, 0);
    layout->addWidget(appearanceGroup, row++, 0);
    layout->setRowStretch(row, 1);

    loadCurrentConfig();
}

PlaylistOrganiser::ConfigData PlaylistOrganiserConfigDialog::config() const
{
    return {
        .leftScript              = m_leftScript->text(),
        .rightScript             = m_rightScript->text(),
        .playingTextColour       = m_playingTextColour->isChecked()
                                     ? normaliseColour(m_playingTextColour->colour().name(QColor::HexArgb))
                                     : QString{},
        .playingBackgroundColour = m_playingBackgroundColour->isChecked()
                                     ? normaliseColour(m_playingBackgroundColour->colour().name(QColor::HexArgb))
                                     : QString{},
    };
}

void PlaylistOrganiserConfigDialog::setConfig(const PlaylistOrganiser::ConfigData& config)
{
    m_leftScript->setText(config.leftScript);
    m_rightScript->setText(config.rightScript);

    m_playingTextColour->setChecked(!config.playingTextColour.isEmpty());
    m_playingTextColour->setColour(
        colourOrDefault(config.playingTextColour, widget()->palette().color(QPalette::Text)));

    m_playingBackgroundColour->setChecked(!config.playingBackgroundColour.isEmpty());
    m_playingBackgroundColour->setColour(
        colourOrDefault(config.playingBackgroundColour, defaultPlayingBackgroundColour(widget())));
}
} // namespace Fooyin
