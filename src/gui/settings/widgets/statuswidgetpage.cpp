/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "statuswidgetpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>

namespace Fooyin {
class StatusWidgetPageWidget : public SettingsPageWidget
{
public:
    explicit StatusWidgetPageWidget(SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QTextEdit* m_playingScript;
};

StatusWidgetPageWidget::StatusWidgetPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_playingScript{new QTextEdit(this)}
{
    m_playingScript->setFixedHeight(100);

    auto* playingScriptLabel = new QLabel(tr("Playing Script:"), this);

    auto* layout = new QGridLayout(this);
    layout->addWidget(playingScriptLabel, 0, 0, Qt::AlignTop);
    layout->addWidget(m_playingScript, 0, 1);

    layout->setColumnStretch(1, 1);
    layout->setRowStretch(1, 1);

    m_playingScript->setText(m_settings->value<Settings::Gui::Internal::StatusPlayingScript>());
}

void StatusWidgetPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::StatusPlayingScript>(m_playingScript->toPlainText());
}

void StatusWidgetPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::StatusPlayingScript>();
    m_playingScript->setText(m_settings->value<Settings::Gui::Internal::StatusPlayingScript>());
}

StatusWidgetPage::StatusWidgetPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::StatusWidget);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Status Widget")});
    setWidgetCreator([settings] { return new StatusWidgetPageWidget(settings); });
}
} // namespace Fooyin
