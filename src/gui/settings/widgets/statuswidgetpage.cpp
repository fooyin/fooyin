/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTextEdit>

namespace Fooyin {
class StatusWidgetPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit StatusWidgetPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_showIcon;
    QCheckBox* m_showSelection;
    QTextEdit* m_playingScript;
    QTextEdit* m_selectionScript;
};

StatusWidgetPageWidget::StatusWidgetPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showIcon{new QCheckBox(tr("Show Icon"), this)}
    , m_showSelection{new QCheckBox(tr("Show Track Selection"), this)}
    , m_playingScript{new QTextEdit(this)}
    , m_selectionScript{new QTextEdit(this)}
{
    m_playingScript->setFixedHeight(100);
    m_selectionScript->setFixedHeight(100);

    auto* playingScriptLabel   = new QLabel(tr("Playing Track") + ":", this);
    auto* selectionScriptLabel = new QLabel(tr("Track Selection") + ":", this);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    appearanceLayout->addWidget(m_showIcon, 0, 0);
    appearanceLayout->addWidget(m_showSelection, 1, 0);

    auto* scripts       = new QGroupBox(tr("Scripts"), this);
    auto* scriptsLayout = new QGridLayout(scripts);

    scriptsLayout->addWidget(playingScriptLabel, 0, 0);
    scriptsLayout->addWidget(m_playingScript, 1, 0);
    scriptsLayout->addWidget(selectionScriptLabel, 2, 0);
    scriptsLayout->addWidget(m_selectionScript, 3, 0);

    auto* layout = new QGridLayout(this);
    layout->addWidget(appearance, 0, 0);
    layout->addWidget(scripts, 1, 0);

    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);
}

void StatusWidgetPageWidget::load()
{
    m_showIcon->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    m_showSelection->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowSelection>());
    m_playingScript->setPlainText(m_settings->value<Settings::Gui::Internal::StatusPlayingScript>());
    m_selectionScript->setPlainText(m_settings->value<Settings::Gui::Internal::StatusSelectionScript>());
}

void StatusWidgetPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::StatusShowIcon>(m_showIcon->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusShowSelection>(m_showSelection->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusPlayingScript>(m_playingScript->toPlainText());
    m_settings->set<Settings::Gui::Internal::StatusSelectionScript>(m_selectionScript->toPlainText());
}

void StatusWidgetPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::StatusShowIcon>();
    m_settings->reset<Settings::Gui::Internal::StatusShowSelection>();
    m_settings->reset<Settings::Gui::Internal::StatusPlayingScript>();
    m_settings->reset<Settings::Gui::Internal::StatusSelectionScript>();
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

#include "moc_statuswidgetpage.cpp"
#include "statuswidgetpage.moc"
