/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcontrol.h"

#include "core/settings/settings.h"
#include "gui/widgets/clickablelabel.h"
#include "utils/enumhelper.h"
#include "utils/utils.h"

#include <QHBoxLayout>

struct PlaylistControl::Private
{
    QHBoxLayout* layout;

    ClickableLabel* repeat;
    ClickableLabel* shuffle;

    QPalette def;

    void setMode(Player::PlayMode mode) const
    {
        QPalette palette = repeat->palette();

        switch(mode) {
            case(Player::PlayMode::Repeat): {
                palette.setColor(repeat->foregroundRole(), palette.highlight().color());
                repeat->setPalette(palette);
                repeat->setText("(");
                break;
            }
            case(Player::PlayMode::RepeatAll): {
                shuffle->setPalette(def);
                palette.setColor(repeat->foregroundRole(), palette.highlight().color());
                repeat->setPalette(palette);
                repeat->setText("*");
                break;
            }
            case(Player::PlayMode::Shuffle): {
                palette.setColor(shuffle->foregroundRole(), palette.highlight().color());
                shuffle->setPalette(palette);
                repeat->setPalette(def);
                repeat->setText("*");
                break;
            }
            case(Player::PlayMode::Default): {
                repeat->setPalette(def);
                shuffle->setPalette(def);
                repeat->setText("*");
                break;
            }
            default:
                return;
        }
    }
};

PlaylistControl::PlaylistControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    p->def = palette();

    connect(p->repeat, &ClickableLabel::clicked, this, &PlaylistControl::repeatClicked);
    connect(p->shuffle, &ClickableLabel::clicked, this, &PlaylistControl::shuffleClicked);
}

PlaylistControl::~PlaylistControl() = default;

void PlaylistControl::setupUi()
{
    p->layout = new QHBoxLayout(this);

    p->layout->setSizeConstraint(QLayout::SetFixedSize);
    p->layout->setSpacing(10);
    p->layout->setContentsMargins(0, 0, 0, 0);

    p->repeat = new ClickableLabel(this);
    p->shuffle = new ClickableLabel(this);

    p->layout->addWidget(p->repeat, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->shuffle, 0, Qt::AlignVCenter);

    QFont font = QFont("Guifx v2 Transports", 12);
    setFont(font);

    p->repeat->setText("*");
    p->shuffle->setText("&");

    Util::setMinimumWidth(p->repeat, "(");

    auto* settings = Settings::instance();
    const auto mode = EnumHelper::fromString<Player::PlayMode>(settings->value(Settings::Setting::PlayMode).toString());
    p->setMode(mode);
}

void PlaylistControl::playModeChanged(Player::PlayMode mode)
{
    auto* settings = Settings::instance();
    settings->set(Settings::Setting::PlayMode, EnumHelper::toString(mode));

    p->setMode(mode);
}
