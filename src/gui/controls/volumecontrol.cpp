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
#include <gui/widgets/tooltip.h>
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
#include <QStyle>
#include <QToolTip>
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
    void updateToolTip(int value);

    VolumeControl* m_self;
    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    VolumeControl::Options m_options{VolumeControl::Default};
    QHBoxLayout* m_layout;
    QPointer<ToolButton> m_volumeIcon;
    QPointer<HoverMenu> m_volumeMenu;
    QPointer<QVBoxLayout> m_menuLayout;
    LogSlider* m_volumeSlider;
    QPointer<ToolTip> m_toolTip;
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

    if(options & VolumeControl::SliderMode) {
        m_volumeSlider->setOrientation(Qt::Horizontal);
        m_volumeSlider->setMinimumSize(75, 0);
        m_layout->addWidget(m_volumeSlider);
    }

    if(options & VolumeControl::IconMode) {
        m_volumeIcon = new ToolButton(m_self);
        if(auto* muteCmd = m_actionManager->command(Constants::Actions::Mute)) {
            m_volumeIcon->setDefaultAction(muteCmd->action());
        }

        // If both icon and slider are enabled, avoid hover menu and just add the icon
        if(!(options & VolumeControl::SliderMode)) {
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

    if(options & VolumeControl::Tooltip) {
        QObject::connect(m_volumeSlider, &LogSlider::sliderMoved, m_volumeSlider,
                         [this](int value) { updateToolTip(value); });
        QObject::connect(m_volumeSlider, &LogSlider::sliderReleased, m_volumeSlider, [this]() {
            if(m_toolTip) {
                m_toolTip->deleteLater();
            }
        });
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

    const int x = (m_self->width() / 2) - (m_volumeMenu->sizeHint().width() / 2);
    const int y = displayAbove ? (-menuHeight - 10) : (m_self->height() + 10);

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

void VolumeControlPrivate::updateToolTip(int value)
{
    if(!m_toolTip) {
        m_toolTip = new ToolTip(m_self->window());
        m_toolTip->show();
    }

    static const auto MinVolumeLog10 = std::log10(MinVolume);
    if(value > (MinVolumeLog10 * m_volumeSlider->scale())) {
        m_toolTip->setText(QStringLiteral("%1 dB").arg(20 * (value / m_volumeSlider->scale()), 0, 'f', 1));
    }
    else {
        m_toolTip->setText(QStringLiteral("-∞ dB"));
    }

    QPoint toolTipPos        = m_volumeSlider->mapFrom(m_volumeSlider->window(), QCursor::pos());
    const QPoint posToWindow = m_volumeSlider->mapToGlobal(QPoint{0, 0});

    if(m_volumeSlider->orientation() == Qt::Horizontal) {
        const bool displayAbove = (posToWindow.y() - (m_volumeSlider->height() + m_toolTip->height())) > 0;

        toolTipPos.setX(toolTipPos.x() - m_toolTip->width());
        toolTipPos.setX(std::clamp(toolTipPos.x(), -m_toolTip->width(), m_volumeSlider->width() - m_toolTip->width()));

        if(displayAbove) {
            toolTipPos.setY(m_volumeSlider->rect().top() - m_toolTip->height() / 4);
        }
        else {
            toolTipPos.setY(m_volumeSlider->rect().bottom() + (m_volumeSlider->height() + m_toolTip->height()));
        }
    }
    else {
        const bool displayLeft = (posToWindow.x() - (m_volumeSlider->width() + m_toolTip->width())) > 0;
        const auto pos         = m_volumeSlider->mapToGlobal(m_volumeSlider->pos());

        toolTipPos.ry() += m_toolTip->height() / 2;

        if(displayLeft) {
            toolTipPos.setX(pos.x() - (m_toolTip->width() * 2 + m_volumeSlider->width()));
        }
        else {
            toolTipPos.setX(pos.x() - m_volumeSlider->width());
        }
    }

    m_toolTip->setPosition(m_volumeSlider->mapTo(m_volumeSlider->window(), toolTipPos));
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
        auto options = static_cast<Options>(std::max(1, layout.value(u"Mode").toInt()));
        p->changeDisplay(options);
    }
}

void VolumeControl::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* iconMode   = new QAction(tr("Icon"), menu);
    auto* sliderMode = new QAction(tr("Slider"), menu);
    auto* toolTip    = new QAction(tr("Tooltip"), menu);

    iconMode->setCheckable(true);
    sliderMode->setCheckable(true);
    toolTip->setCheckable(true);

    iconMode->setChecked(p->m_options & IconMode);
    sliderMode->setChecked(p->m_options & SliderMode);
    toolTip->setChecked(p->m_options & Tooltip);

    auto toggleOption = [this](QAction* action, VolumeControl::Option option) {
        VolumeControl::Options newOptions = p->m_options;
        if(action->isChecked()) {
            newOptions |= option;
        }
        else {
            newOptions &= ~option;
        }

        // Ensure at least one mode option is always selected
        if(!(newOptions & (IconMode | SliderMode))) {
            action->setChecked(true);
        }
        else {
            p->changeDisplay(newOptions);
        }
    };

    QObject::connect(iconMode, &QAction::triggered, this,
                     [toggleOption, iconMode]() { toggleOption(iconMode, IconMode); });
    QObject::connect(sliderMode, &QAction::triggered, this,
                     [toggleOption, sliderMode]() { toggleOption(sliderMode, SliderMode); });
    QObject::connect(toolTip, &QAction::triggered, this, [toggleOption, toolTip]() { toggleOption(toolTip, Tooltip); });

    menu->addAction(iconMode);
    menu->addAction(sliderMode);
    menu->addSeparator();
    menu->addAction(toolTip);

    menu->popup(event->globalPos());
}

void VolumeControl::wheelEvent(QWheelEvent* event)
{
    QApplication::sendEvent(p->m_volumeSlider, event);
}
} // namespace Fooyin

#include "moc_volumecontrol.cpp"
