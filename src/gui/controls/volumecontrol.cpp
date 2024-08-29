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

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QWheelEvent>

#include <chrono>

using namespace std::chrono_literals;

constexpr double MinVolume = 0.01;

namespace Fooyin {
enum class Mode
{
    Icon,
    Slider
};

class VolumeControlPrivate
{
public:
    VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings);

    void setMode(Mode mode, bool init = false);

    void updateButtonStyle() const;
    void showVolumeMenu() const;
    void volumeChanged(double volume) const;
    void updateDisplay(double volume) const;

    VolumeControl* m_self;
    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    Mode m_mode{Mode::Slider};
    QVBoxLayout* m_layout;
    QPointer<ToolButton> m_volumeIcon;
    QPointer<HoverMenu> m_volumeMenu;
    QPointer<QVBoxLayout> m_menuLayout;
    LogSlider* m_volumeSlider;
};

VolumeControlPrivate::VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(m_self)}
    , m_volumeSlider{new LogSlider(Qt::Horizontal, m_self)}
{
    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    m_volumeSlider->setRange(MinVolume, 1.0);
    m_volumeSlider->setNaturalValue(m_settings->value<Settings::Core::OutputVolume>());
    QObject::connect(m_volumeSlider, &LogSlider::logValueChanged, m_volumeSlider,
                     [this](double volume) { volumeChanged(volume); });

    setMode(m_mode, true);
}

void VolumeControlPrivate::setMode(Mode mode, bool init)
{
    if(std::exchange(m_mode, mode) == mode && !init) {
        return;
    }

    if(m_mode == Mode::Icon) {
        m_volumeMenu = new HoverMenu(m_self);
        m_menuLayout = new QVBoxLayout(m_volumeMenu);
        m_volumeIcon = new ToolButton(m_self);

        QObject::connect(m_volumeIcon, &ToolButton::entered, m_volumeMenu, [this]() { showVolumeMenu(); });
        if(auto* muteCmd = m_actionManager->command(Constants::Actions::Mute)) {
            m_volumeIcon->setDefaultAction(muteCmd->action());
        }

        m_volumeSlider->setOrientation(Qt::Vertical);
        m_volumeSlider->setMinimumSize(0, 100);
        m_menuLayout->addWidget(m_volumeSlider);

        m_layout->addWidget(m_volumeIcon);
        m_volumeMenu->hide();

        updateDisplay(m_settings->value<Settings::Core::OutputVolume>());
        updateButtonStyle();
    }
    else {
        if(m_volumeIcon) {
            m_volumeIcon->deleteLater();
        }
        if(m_volumeMenu) {
            m_volumeMenu->deleteLater();
        }
        if(m_menuLayout) {
            m_menuLayout->deleteLater();
        }

        m_volumeSlider->setOrientation(Qt::Horizontal);
        m_volumeSlider->setMinimumSize(75, 0);
        m_layout->addWidget(m_volumeSlider);
    }
}

void VolumeControlPrivate::updateButtonStyle() const
{
    if(!m_volumeIcon) {
        return;
    }

    const auto options
        = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

    m_volumeIcon->setStretchEnabled(options & Settings::Gui::Stretch);
    m_volumeIcon->setAutoRaise(!(options & Settings::Gui::Raise));
}

void VolumeControlPrivate::showVolumeMenu() const
{
    if(!m_volumeMenu) {
        return;
    }

    const int menuWidth  = m_volumeMenu->sizeHint().width();
    const int menuHeight = m_volumeMenu->sizeHint().height();

    const int yPosToWindow = m_self->mapToGlobal(QPoint{0, 0}).y();

    // Only display volume slider above icon if it won't clip above the main window.
    const bool displayAbove = (yPosToWindow - menuHeight) > 0;

    const int x = !menuWidth - 15;
    const int y = displayAbove ? (!m_self->height() - menuHeight - 10) : (m_self->height() + 10);

    const QPoint pos(m_self->mapToGlobal(QPoint{x, y}));
    m_volumeMenu->move(pos);
    m_volumeMenu->show();
    m_volumeMenu->setFocus(Qt::ActiveWindowFocusReason);

    m_volumeMenu->start(1s);
}

void VolumeControlPrivate::volumeChanged(double volume) const
{
    if(volume == MinVolume) {
        volume = 0;
    }

    m_settings->set<Settings::Core::OutputVolume>(volume);
}

void VolumeControlPrivate::updateDisplay(double volume) const
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

VolumeControl::VolumeControl(ActionManager* actionManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<VolumeControlPrivate>(this, actionManager, settings)}
{
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

void VolumeControl::saveLayoutData(QJsonObject& layout)
{
    layout[u"Mode"] = static_cast<int>(p->m_mode);
}

void VolumeControl::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Mode")) {
        p->setMode(static_cast<Mode>(layout.value(u"Mode").toInt()));
    }
}

void VolumeControl::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* modeGroup = new QActionGroup(this);

    auto* iconMode   = new QAction(tr("Icon"), modeGroup);
    auto* sliderMode = new QAction(tr("Slider"), modeGroup);

    iconMode->setCheckable(true);
    sliderMode->setCheckable(true);

    iconMode->setChecked(p->m_mode == Mode::Icon);
    sliderMode->setChecked(p->m_mode == Mode::Slider);

    QObject::connect(iconMode, &QAction::triggered, this, [this]() { p->setMode(Mode::Icon); });
    QObject::connect(sliderMode, &QAction::triggered, this, [this]() { p->setMode(Mode::Slider); });

    menu->addAction(iconMode);
    menu->addAction(sliderMode);

    menu->popup(event->globalPos());
}

void VolumeControl::wheelEvent(QWheelEvent* event)
{
    QApplication::sendEvent(p->m_volumeSlider, event);
}
} // namespace Fooyin

#include "moc_volumecontrol.cpp"
