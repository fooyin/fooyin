/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "scrobblertoggle.h"

#include "scrobblerconstants.h"
#include "settings/scrobblersettings.h"

#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>

using namespace Qt::StringLiterals;

constexpr auto ScrobbleIcon = "scrobble";

namespace Fooyin::Scrobbler {
ScrobblerToggle::ScrobblerToggle(ActionManager* actionManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_scrobbleButton{new ToolButton(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrobbleButton);

    if(auto* toggleCmd = m_actionManager->command("Scrobbler.Toggle")) {
        m_scrobbleButton->setDefaultAction(toggleCmd->action());
    }

    updateButton();

    m_settings->subscribe<Settings::Scrobbler::ScrobblingEnabled>(this, &ScrobblerToggle::scrobblingToggled);
    settings->subscribe<Settings::Gui::IconTheme>(this, &ScrobblerToggle::updateButton);
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, &ScrobblerToggle::updateButton);
}

QString ScrobblerToggle::name() const
{
    return tr("Scrobble Toggle");
}

QString ScrobblerToggle::layoutName() const
{
    return u"ScrobbleToggle"_s;
}

void ScrobblerToggle::changeEvent(QEvent* event)
{
    FyWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            updateButton();
            break;
        default:
            break;
    }
}

void ScrobblerToggle::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* scrobblingEnabled = new QAction(tr("Enable scrobbling"), menu);
    scrobblingEnabled->setCheckable(true);
    scrobblingEnabled->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    QObject::connect(scrobblingEnabled, &QAction::triggered, this,
                     [this](bool enabled) { m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(enabled); });
    menu->addAction(scrobblingEnabled);

    menu->addSeparator();

    auto* scrobblingSettings = new QAction(tr("Scrobbling settings…"), menu);
    QObject::connect(scrobblingSettings, &QAction::triggered, this, [this]() {
        if(m_settings && m_settings->settingsDialog()) {
            m_settings->settingsDialog()->openAtPage(Constants::Page::General);
        }
    });
    menu->addAction(scrobblingSettings);

    auto* scrobblingServices = new QAction(tr("Scrobbling services…"), menu);
    QObject::connect(scrobblingServices, &QAction::triggered, this, [this]() {
        if(m_settings && m_settings->settingsDialog()) {
            m_settings->settingsDialog()->openAtPage(Constants::Page::Services);
        }
    });
    menu->addAction(scrobblingServices);

    menu->popup(event->globalPos());
}

void ScrobblerToggle::updateButton()
{
    updateButtonStyle();
    updateButtonIcon();
}

void ScrobblerToggle::updateButtonIcon()
{
    QAction* action = m_scrobbleButton->defaultAction();
    if(!action) {
        return;
    }

    const bool enabled = m_settings->value<Settings::Scrobbler::ScrobblingEnabled>();
    if(enabled) {
        const QColor iconColour = palette().color(QPalette::Active, QPalette::Highlight);
        action->setIcon(Utils::changePixmapColour(Gui::iconFromTheme(ScrobbleIcon).pixmap({128, 128}), iconColour));
    }
    else {
        action->setIcon(Gui::iconFromTheme(ScrobbleIcon));
    }
}

void ScrobblerToggle::updateButtonStyle() const
{
    const auto options
        = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

    m_scrobbleButton->setStretchEnabled(options & Settings::Gui::Stretch);
    m_scrobbleButton->setAutoRaise(!(options & Settings::Gui::Raise));
}

void ScrobblerToggle::scrobblingToggled(bool /*enabled*/)
{
    updateButtonIcon();
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblertoggle.cpp"
