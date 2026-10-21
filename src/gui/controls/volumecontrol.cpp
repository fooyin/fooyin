/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/widgets/slider.h>
#include <gui/widgets/toolbutton.h>
#include <gui/widgets/tooltip.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/audioutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QResizeEvent>
#include <QStyle>
#include <QToolTip>
#include <QWheelEvent>

#include <chrono>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr double MinLogVolume      = 0.01;
constexpr double LogPositionFactor = 100.0;
constexpr int VolumeSteps          = 1000;

namespace Fooyin {
enum class VolumeCurve : uint8_t
{
    LinearDb = 0,
    LinearAmplitude,
    Quadratic,
    Cubic,
    LogarithmicDb,
};

class VolumeControlPrivate
{
public:
    VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings);

    void changeDisplay(VolumeControl::Options options, bool init = false);
    void setCurve(VolumeCurve curve);
    void updateSlider(double volume) const;

    [[nodiscard]] double volumeAtPosition(int value) const;

    void showVolumeMenu() const;
    void updateDisplay(double volume) const;
    void updateToolTip(int value);
    void updateOrientation();

    VolumeControl* m_self;
    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    VolumeControl::Options m_options{VolumeControl::Default};
    QHBoxLayout* m_layout;
    QPointer<ToolButton> m_volumeIcon;
    QPointer<HoverMenu> m_volumeMenu;
    QPointer<QVBoxLayout> m_menuLayout;
    Slider* m_volumeSlider;
    QPointer<ToolTip> m_toolTip;
    Qt::Orientation m_orientation{Qt::Horizontal};
    bool m_autoOrientation{true};
    VolumeCurve m_curve{VolumeCurve::LogarithmicDb};
};

VolumeControlPrivate::VolumeControlPrivate(VolumeControl* self, ActionManager* actionManager, SettingsManager* settings)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(m_self)}
    , m_volumeSlider{new Slider(Qt::Horizontal, m_self)}
{
    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    setCurve(m_curve);
    QObject::connect(m_volumeSlider, &Slider::valueChanged, m_volumeSlider,
                     [this](int value) { m_settings->set<Settings::Core::OutputVolume>(volumeAtPosition(value)); });
    m_settings->subscribe<Settings::Core::OutputVolume>(m_volumeSlider,
                                                        [this](double volume) { updateSlider(volume); });

    changeDisplay(m_options, true);
}

void VolumeControlPrivate::changeDisplay(VolumeControl::Options options, bool init)
{
    if(std::exchange(m_options, options) == options && !init) {
        return;
    }

    if(m_volumeIcon) {
        // Exclude the old icon from the layout's size hint while it awaits deletion
        m_volumeIcon->hide();
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
        m_volumeIcon = new ToolButton(m_settings, m_self);
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
    }

    updateOrientation();

    if(options & VolumeControl::Tooltip) {
        QObject::connect(m_volumeSlider, &Slider::sliderMoved, m_volumeSlider,
                         [this](int value) { updateToolTip(value); });
        QObject::connect(m_volumeSlider, &Slider::sliderReleased, m_volumeSlider, [this]() {
            if(m_toolTip) {
                m_toolTip->deleteLater();
            }
        });
    }
}

void VolumeControlPrivate::setCurve(VolumeCurve curve)
{
    const QSignalBlocker blocker{m_volumeSlider};

    m_curve = curve;
    if(m_curve == VolumeCurve::LinearDb) {
        m_volumeSlider->setRange(static_cast<int>(std::log10(MinLogVolume) * LogPositionFactor), 0);
    }
    else {
        m_volumeSlider->setRange(0, VolumeSteps);
    }

    updateSlider(m_settings->value<Settings::Core::OutputVolume>());
}

void VolumeControlPrivate::updateSlider(double volume) const
{
    const QSignalBlocker blocker{m_volumeSlider};

    volume = std::clamp(volume, 0.0, 1.0);

    double value{0.0};
    switch(m_curve) {
        case VolumeCurve::LinearDb:
            value = std::log10(std::max(volume, MinLogVolume)) * LogPositionFactor;
            break;
        case VolumeCurve::LinearAmplitude:
            value = volume * VolumeSteps;
            break;
        case VolumeCurve::Quadratic:
            value = std::sqrt(volume) * VolumeSteps;
            break;
        case VolumeCurve::Cubic:
            value = std::cbrt(volume) * VolumeSteps;
            break;
        case VolumeCurve::LogarithmicDb:
            value = std::exp2(Audio::volumeToDb(volume) / 10.0) * VolumeSteps;
            break;
    }

    m_volumeSlider->setValue(static_cast<int>(std::round(value)));
}

double VolumeControlPrivate::volumeAtPosition(int value) const
{
    if(value == m_volumeSlider->minimum()) {
        return 0.0;
    }

    const double position = static_cast<double>(value) / VolumeSteps;
    switch(m_curve) {
        case VolumeCurve::LinearDb:
            return std::pow(10.0, value / LogPositionFactor);
        case VolumeCurve::LinearAmplitude:
            return position;
        case VolumeCurve::Quadratic:
            return position * position;
        case VolumeCurve::Cubic:
            return position * position * position;
        case VolumeCurve::LogarithmicDb:
            // Each halving of the slider position reduces the gain by 10 dB
            return Audio::dbToVolume(10.0 * std::log2(position));
    }

    return 0.0;
}

void VolumeControlPrivate::updateOrientation()
{
    auto orientation{m_orientation};

    if(m_autoOrientation) {
        orientation = m_self->automaticOrientation();
        if((m_options & VolumeControl::SliderMode) && !m_self->findToolbar()) {
            const QSize minSize       = m_layout->minimumSize();
            const int minLength       = std::max(minSize.width(), minSize.height());
            const bool horizontalFits = m_self->width() >= minLength;
            const bool verticalFits   = m_self->height() >= minLength;

            if(horizontalFits != verticalFits) {
                orientation = verticalFits ? Qt::Vertical : Qt::Horizontal;
            }
            else if(!horizontalFits && m_self->parentWidget()) {
                const auto parentSize = m_self->parentWidget()->size();
                orientation           = parentSize.height() > parentSize.width() ? Qt::Vertical : Qt::Horizontal;
            }
        }
    }

    const auto direction = orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if(m_layout->direction() != direction) {
        m_layout->setDirection(direction);
    }

    const auto alignment = direction == QBoxLayout::LeftToRight ? Qt::AlignVCenter : Qt::AlignHCenter;
    for(int i{0}; i < m_layout->count(); ++i) {
        if(auto* widget = m_layout->itemAt(i)->widget()) {
            m_layout->setAlignment(widget, alignment);
        }
    }

    if(m_options & VolumeControl::SliderMode) {
        m_volumeSlider->setOrientation(orientation);
        m_volumeSlider->setMinimumSize(orientation == Qt::Vertical ? QSize{0, 75} : QSize{75, 0});
    }
    else {
        // The slider in the hover menu is always vertical
        m_volumeSlider->setOrientation(Qt::Vertical);
        m_volumeSlider->setMinimumSize(0, 100);
    }

    m_self->updateOrientationConstraints(m_autoOrientation, m_layout->minimumSize());
}

void VolumeControlPrivate::showVolumeMenu() const
{
    if(!m_volumeMenu) {
        return;
    }

    const QSize menuSize = m_volumeMenu->sizeHint();
    const QRect iconRect{m_volumeIcon->mapToGlobal(QPoint{}), m_volumeIcon->size()};
    const QRect windowRect{m_self->window()->mapToGlobal(QPoint{}), m_self->window()->size()};

    static constexpr auto gap = 10;

    const bool canPlaceRight = iconRect.right() + gap + menuSize.width() <= windowRect.right();
    const bool canPlaceLeft  = iconRect.left() - gap - menuSize.width() >= windowRect.left();
    const bool placeRight
        = canPlaceRight
       || (!canPlaceLeft && windowRect.right() - iconRect.right() >= iconRect.left() - windowRect.left());

    const int x    = placeRight ? iconRect.right() + gap : iconRect.left() - gap - menuSize.width();
    const int minY = windowRect.top();
    const int maxY = std::max(minY, windowRect.bottom() - menuSize.height());
    const int y    = std::clamp(iconRect.center().y() - (menuSize.height() / 2), minY, maxY);

    m_volumeMenu->move({x, y});
    m_volumeMenu->show();
    m_volumeMenu->setFocus(Qt::ActiveWindowFocusReason);

    m_volumeMenu->start(1s);
}

void VolumeControlPrivate::updateDisplay(double volume) const
{
    if(!m_volumeIcon) {
        return;
    }

    if(volume <= 1.0 && volume >= 0.40) {
        m_volumeIcon->setIcon(Gui::iconFromTheme(Constants::Icons::VolumeHigh));
    }
    else if(volume < 0.40 && volume >= 0.20) {
        m_volumeIcon->setIcon(Gui::iconFromTheme(Constants::Icons::VolumeMed));
    }
    else if(volume < 0.20 && volume > 0.0) {
        m_volumeIcon->setIcon(Gui::iconFromTheme(Constants::Icons::VolumeLow));
    }
    else {
        m_volumeIcon->setIcon(Gui::iconFromTheme(Constants::Icons::VolumeMute));
    }
}

void VolumeControlPrivate::updateToolTip(int value)
{
    if(!m_toolTip) {
        m_toolTip = new ToolTip(m_self->window());
        m_toolTip->show();
    }

    const double volume = volumeAtPosition(value);
    if(volume > 0.0) {
        const auto volumeDb = 20 * std::log10(volume);
        m_toolTip->setText(u"%1 dB"_s.arg(volumeDb, 0, 'f', 1));
    }
    else {
        m_toolTip->setText(u"−∞ dB"_s);
    }

    QPoint toolTipPos        = m_volumeSlider->mapFromGlobal(QCursor::pos());
    const QPoint posToWindow = m_volumeSlider->mapToGlobal(QPoint{0, 0});
    Qt::Alignment alignment{Qt::AlignLeft};

    const auto* window = m_self->window();

    if(m_volumeSlider->orientation() == Qt::Horizontal) {
        const bool displayAbove = (posToWindow.y() - (m_volumeSlider->height() + m_toolTip->height())) > 0;

        toolTipPos.rx() -= m_toolTip->width() / 2;
        toolTipPos.rx() = std::clamp(toolTipPos.x(), 0, m_self->width() - m_toolTip->width());

        if(displayAbove) {
            toolTipPos.setY(m_volumeSlider->rect().top() - (m_toolTip->height() / 4));
        }
        else {
            toolTipPos.setY(m_volumeSlider->rect().bottom() + (m_volumeSlider->height() + m_toolTip->height()));
        }
    }
    else {
        const QPoint sliderTopLeft = window->mapFromGlobal(posToWindow);
        const QRect sliderRect{sliderTopLeft, m_volumeSlider->size()};
        const QPoint cursorPos = window->mapFromGlobal(QCursor::pos());

        static constexpr auto gap = 10;

        const bool canPlaceRight = sliderRect.right() + gap + m_toolTip->width() <= window->rect().right();
        const bool canPlaceLeft  = sliderRect.left() - gap - m_toolTip->width() >= window->rect().left();
        const bool placeRight
            = canPlaceRight
           || (!canPlaceLeft
               && window->rect().right() - sliderRect.right() >= sliderRect.left() - window->rect().left());

        const int tooltipY
            = std::clamp(cursorPos.y() + (m_toolTip->height() / 2), m_toolTip->height(), window->height());

        if(placeRight) {
            toolTipPos = {sliderRect.right() + gap, tooltipY};
        }
        else {
            alignment  = Qt::AlignRight;
            toolTipPos = {sliderRect.left() - gap, tooltipY};
        }

        m_toolTip->setPosition(toolTipPos, alignment);
        return;
    }

    const QPoint tooltipWindowPos = window->mapFromGlobal(m_volumeSlider->mapToGlobal(toolTipPos));
    m_toolTip->setPosition(tooltipWindowPos, alignment);
}

VolumeControl::VolumeControl(ActionManager* actionManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<VolumeControlPrivate>(this, actionManager, settings)}
{
    settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { p->updateDisplay(volume); });
    settings->subscribe<Settings::Gui::IconTheme>(
        this, [this]() { p->updateDisplay(p->m_settings->value<Settings::Core::OutputVolume>()); });
}

VolumeControl::~VolumeControl() = default;

QString VolumeControl::name() const
{
    return tr("Volume Controls");
}

QString VolumeControl::layoutName() const
{
    return u"VolumeControls"_s;
}

void VolumeControl::saveLayoutData(QJsonObject& layout)
{
    layout["Mode"_L1]  = static_cast<int>(p->m_options);
    layout["Curve"_L1] = static_cast<int>(p->m_curve);

    if(!p->m_autoOrientation) {
        layout["Orientation"_L1] = p->m_orientation;
    }
}

void VolumeControl::loadLayoutData(const QJsonObject& layout)
{
    const int curve = layout.value("Curve"_L1).toInt(static_cast<int>(VolumeCurve::LogarithmicDb));
    p->setCurve(curve >= static_cast<int>(VolumeCurve::LinearDb)
                        && curve <= static_cast<int>(VolumeCurve::LogarithmicDb)
                    ? static_cast<VolumeCurve>(curve)
                    : VolumeCurve::LogarithmicDb);

    if(layout.contains("Mode"_L1)) {
        // Support old format (0=Icon)
        auto options = static_cast<Options>(std::max(1, layout.value("Mode"_L1).toInt()));
        p->changeDisplay(options);
    }
    if(layout.contains("Orientation"_L1)) {
        const auto orientation = static_cast<Qt::Orientation>(layout.value("Orientation"_L1).toInt());
        if(orientation == Qt::Horizontal || orientation == Qt::Vertical) {
            p->m_orientation     = orientation;
            p->m_autoOrientation = false;
            p->updateOrientation();
        }
    }
}

void VolumeControl::populateContextMenu(QMenu* menu)
{
    auto* iconMode   = new QAction(tr("Icon"), menu);
    auto* sliderMode = new QAction(tr("Slider"), menu);
    auto* toolTip    = new QAction(tr("Tooltip"), menu);

    iconMode->setCheckable(true);
    sliderMode->setCheckable(true);
    toolTip->setCheckable(true);

    iconMode->setChecked(p->m_options & IconMode);
    sliderMode->setChecked(p->m_options & SliderMode);
    toolTip->setChecked(p->m_options & Tooltip);

    auto toggleOption = [this](QAction* action, Option option) {
        Options newOptions = p->m_options;
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

    auto* orientationGroup = new QActionGroup(menu);
    auto* automatic        = new QAction(tr("Automatic"), orientationGroup);
    auto* horizontal       = new QAction(tr("Horizontal"), orientationGroup);
    auto* vertical         = new QAction(tr("Vertical"), orientationGroup);

    auto* orientationMenu = new QMenu(tr("Orientation"), menu);
    orientationMenu->addAction(automatic);
    orientationMenu->addAction(horizontal);
    orientationMenu->addAction(vertical);

    automatic->setCheckable(true);
    horizontal->setCheckable(true);
    vertical->setCheckable(true);

    automatic->setChecked(p->m_autoOrientation);
    horizontal->setChecked(!p->m_autoOrientation && p->m_orientation == Qt::Horizontal);
    vertical->setChecked(!p->m_autoOrientation && p->m_orientation == Qt::Vertical);

    QObject::connect(automatic, &QAction::triggered, this, [this]() {
        p->m_autoOrientation = true;
        p->updateOrientation();
    });
    QObject::connect(horizontal, &QAction::triggered, this, [this]() {
        p->m_autoOrientation = false;
        p->m_orientation     = Qt::Horizontal;
        p->updateOrientation();
    });
    QObject::connect(vertical, &QAction::triggered, this, [this]() {
        p->m_autoOrientation = false;
        p->m_orientation     = Qt::Vertical;
        p->updateOrientation();
    });

    auto* curveMenu  = new QMenu(tr("Curve"), menu);
    auto* curveGroup = new QActionGroup(curveMenu);

    const auto addCurve = [this, curveMenu, curveGroup](const QString& name, VolumeCurve curve) {
        auto* action = new QAction(name, curveGroup);
        action->setCheckable(true);
        action->setChecked(p->m_curve == curve);
        curveMenu->addAction(action);
        QObject::connect(action, &QAction::triggered, this, [this, curve]() { p->setCurve(curve); });
    };

    addCurve(tr("Logarithmic dB"), VolumeCurve::LogarithmicDb);
    addCurve(tr("Linear dB"), VolumeCurve::LinearDb);
    addCurve(tr("Linear amplitude"), VolumeCurve::LinearAmplitude);
    addCurve(tr("Quadratic"), VolumeCurve::Quadratic);
    addCurve(tr("Cubic"), VolumeCurve::Cubic);

    menu->addAction(iconMode);
    menu->addAction(sliderMode);
    menu->addSeparator();
    menu->addAction(toolTip);
    menu->addSeparator();
    menu->addMenu(curveMenu);
    menu->addMenu(orientationMenu);
}

void VolumeControl::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    populateContextMenu(menu);
    menu->popup(event->globalPos());
}

void VolumeControl::resizeEvent(QResizeEvent* event)
{
    p->updateOrientation();
    FyWidget::resizeEvent(event);
}

void VolumeControl::wheelEvent(QWheelEvent* event)
{
    QApplication::sendEvent(p->m_volumeSlider, event);
}
} // namespace Fooyin

#include "moc_volumecontrol.cpp"
