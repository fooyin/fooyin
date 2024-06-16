/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "volumecontrol.h"

#include "widgets/hovermenu.h"
#include "widgets/logslider.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QWheelEvent>

#include <chrono>

using namespace std::chrono_literals;

constexpr double MinVolume = 0.01;

namespace Fooyin {
struct VolumeControl::Private
{
    VolumeControl* m_self;

    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    ToolButton* m_volumeIcon;
    HoverMenu* m_volumeMenu;
    LogSlider* m_volumeSlider;

    Private(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings)
        : m_self{self}
        , m_actionManager{actionManager}
        , m_settings{settings}
        , m_volumeIcon{new ToolButton(m_self)}
        , m_volumeMenu{new HoverMenu(m_self)}
        , m_volumeSlider{new LogSlider(Qt::Vertical, m_self)}
    {
        auto* volumeLayout = new QVBoxLayout(m_volumeMenu);
        volumeLayout->addWidget(m_volumeSlider);

        if(auto* muteCmd = m_actionManager->command(Constants::Actions::Mute)) {
            m_volumeIcon->setDefaultAction(muteCmd->action());
        }

        m_volumeSlider->setMinimumHeight(100);
        m_volumeSlider->setRange(MinVolume, 1.0);
        m_volumeSlider->setNaturalValue(m_settings->value<Settings::Core::OutputVolume>());

        m_volumeMenu->hide();

        updateDisplay(m_settings->value<Settings::Core::OutputVolume>());
        updateButtonStyle();
    }

    void updateButtonStyle() const
    {
        const auto options
            = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

        m_volumeIcon->setStretchEnabled(options & Settings::Gui::Stretch);
        m_volumeIcon->setAutoRaise(!(options & Settings::Gui::Raise));
    }

    void showVolumeMenu() const
    {
        const int menuWidth  = m_volumeMenu->sizeHint().width();
        const int menuHeight = m_volumeMenu->sizeHint().height();

        const int yPosToWindow = m_self->mapToGlobal(QPoint{0, 0}).y();

        // Only display volume slider above icon if it won't clip above the main window.
        const bool displayAbove = (yPosToWindow - menuHeight) > 0;

        const int x = !menuWidth - 15;
        const int y = displayAbove ? (!m_self->height() - menuHeight - 10) : (m_self->height() + 10);

        const QPoint pos(m_self->mapToGlobal(QPoint(x, y)));
        m_volumeMenu->move(pos);
        m_volumeMenu->show();
        m_volumeMenu->setFocus(Qt::ActiveWindowFocusReason);

        m_volumeMenu->start(1s);
    }

    void volumeChanged(double volume) const
    {
        if(volume == MinVolume) {
            volume = 0;
        }

        m_settings->set<Settings::Core::OutputVolume>(volume);
    }

    void updateDisplay(double volume) const
    {
        if(!m_volumeIcon) {
            return;
        }

        if(volume <= 1.0 && volume >= 0.40) {
            m_volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeHigh));
        }
        else if(volume < 0.40 && volume >= 0.20) {
            m_volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeMed));
        }
        else if(volume < 0.20 && volume >= MinVolume) {
            m_volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeLow));
        }
        else {
            m_volumeIcon->setIcon(Utils::iconFromTheme(Constants::Icons::VolumeMute));
        }
    }
};

VolumeControl::VolumeControl(ActionManager* actionManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(p->m_volumeIcon);

    QObject::connect(p->m_volumeIcon, &ToolButton::entered, this, [this]() { p->showVolumeMenu(); });

    QObject::connect(p->m_volumeSlider, &LogSlider::logValueChanged, this,
                     [this](double volume) { p->volumeChanged(volume); });

    settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { p->updateDisplay(volume); });
    settings->subscribe<Settings::Gui::IconTheme>(
        this, [this]() { p->updateDisplay(p->m_settings->value<Settings::Core::OutputVolume>()); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, [this]() { p->updateButtonStyle(); });
}

VolumeControl::~VolumeControl() = default;

QString VolumeControl::name() const
{
    return tr("Volume Controls");
}

QString VolumeControl::layoutName() const
{
    return QStringLiteral("VolumeControls");
}

void VolumeControl::wheelEvent(QWheelEvent* event)
{
    QApplication::sendEvent(p->m_volumeSlider, event);
}
} // namespace Fooyin

#include "moc_volumecontrol.cpp"
