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
class VolumeControlPrivate
{
public:
    VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings);

    void changeDisplay(VolumeControl::Options options, bool init = false);

    void updateButtonStyle() const;
    void showVolumeMenu() const;
    void volumeChanged(double volume) const;
    void updateDisplay(double volume) const;

    VolumeControl* m_self;
    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    VolumeControl::Options m_options{VolumeControl::All};
    QHBoxLayout* m_layout;
    QPointer<ToolButton> m_volumeIcon;
    QPointer<HoverMenu> m_volumeMenu;
    QPointer<QVBoxLayout> m_menuLayout;
    LogSlider* m_volumeSlider;
};

VolumeControlPrivate::VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(m_self)}
    , m_volumeSlider{new LogSlider(Qt::Horizontal, m_self)}
{
    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    m_volumeSlider->setRange(MinVolume, 1.0);
    m_volumeSlider->setNaturalValue(m_settings->value<Settings::Core::OutputVolume>());
    QObject::connect(m_volumeSlider, &LogSlider::logValueChanged, m_volumeSlider,
                     [this](double volume) { volumeChanged(volume); });
    m_settings->subscribe<Settings::Core::OutputVolume>(m_volumeSlider, [this](const double volume) {
        const QSignalBlocker blocker{m_volumeSlider};
        m_volumeSlider->setNaturalValue(volume);
    });

    changeDisplay(m_options, true);
}

void VolumeControlPrivate::changeDisplay(VolumeControl::Options options, bool init)
{
    if(std::exchange(m_options, options) == options && !init) {
        return;
    }

    if(m_volumeIcon) {
        m_volumeIcon->deleteLater();
    }
    if(m_volumeMenu) {
        m_volumeMenu->deleteLater();
    }
    if(m_menuLayout) {
        m_menuLayout->deleteLater();
    }

    if(options & VolumeControl::Slider) {
        m_volumeSlider->setOrientation(Qt::Horizontal);
        m_volumeSlider->setMinimumSize(75, 0);
        m_layout->addWidget(m_volumeSlider);
    }

    if(options & VolumeControl::Icon) {
        m_volumeIcon = new ToolButton(m_self);
        if(auto* muteCmd = m_actionManager->command(Constants::Actions::Mute)) {
            m_volumeIcon->setDefaultAction(muteCmd->action());
        }

        // If both icon and slider are enabled, avoid hover menu and just add the icon
        if(!(options & VolumeControl::Slider)) {
            m_volumeMenu = new HoverMenu(m_self);
            m_menuLayout = new QVBoxLayout(m_volumeMenu);

            QObject::connect(m_volumeIcon, &ToolButton::entered, m_volumeMenu, [this]() { showVolumeMenu(); });

            m_volumeSlider->setOrientation(Qt::Vertical);
            m_volumeSlider->setMinimumSize(0, 100);
            m_menuLayout->addWidget(m_volumeSlider);

            m_volumeMenu->hide();
        }

        m_layout->addWidget(m_volumeIcon);
        updateDisplay(m_settings->value<Settings::Core::OutputVolume>());
        updateButtonStyle();
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

    const int menuHeight = m_volumeMenu->sizeHint().height();

    const int yPosToWindow = m_self->mapToGlobal(QPoint{0, 0}).y();

    // Only display volume slider above icon if it won't clip above the main window.
    const bool displayAbove = (yPosToWindow - menuHeight) > 0;

    static constexpr int x = -15;
    const int y            = displayAbove ? (-menuHeight - 10) : (m_self->height() + 10);

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
    layout[u"Mode"] = static_cast<int>(p->m_options);
}

void VolumeControl::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Mode")) {
        // Support old format (0=Icon)
        const int mode = std::max(1, layout.value(u"Mode").toInt());
        p->changeDisplay(static_cast<Option>(mode));
    }
}

void VolumeControl::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* iconMode   = new QAction(tr("Icon"), menu);
    auto* sliderMode = new QAction(tr("Slider"), menu);

    iconMode->setCheckable(true);
    sliderMode->setCheckable(true);

    iconMode->setChecked(p->m_options & Icon);
    sliderMode->setChecked(p->m_options & Slider);

    auto toggleOption = [this](QAction* action, VolumeControl::Option option) {
        VolumeControl::Options newOptions = p->m_options;
        if(action->isChecked()) {
            newOptions |= option;
        }
        else {
            newOptions &= ~option;
        }

        // Ensure at least one option is always selected
        if(!(newOptions & Icon) && !(newOptions & Slider)) {
            action->setChecked(true);
        }
        else {
            p->changeDisplay(newOptions);
        }
    };

    QObject::connect(iconMode, &QAction::triggered, this, [toggleOption, iconMode]() { toggleOption(iconMode, Icon); });
    QObject::connect(sliderMode, &QAction::triggered, this,
                     [toggleOption, sliderMode]() { toggleOption(sliderMode, Slider); });

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
