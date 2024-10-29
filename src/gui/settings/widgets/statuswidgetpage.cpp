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
#include <gui/guisettings.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

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
    QCheckBox* m_showStatusTips;
    ScriptLineEdit* m_playingScript;
    ScriptLineEdit* m_selectionScript;
};

StatusWidgetPageWidget::StatusWidgetPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showIcon{new QCheckBox(tr("Show icon"), this)}
    , m_showSelection{new QCheckBox(tr("Show track selection"), this)}
    , m_showStatusTips{new QCheckBox(tr("Show action tips"), this)}
    , m_playingScript{new ScriptLineEdit(this)}
    , m_selectionScript{new ScriptLineEdit(this)}
{
    auto* displayGroup  = new QGroupBox(tr("Display"), this);
    auto* displayLayout = new QGridLayout(displayGroup);

    displayLayout->addWidget(m_showIcon, 0, 0);
    displayLayout->addWidget(m_showSelection, 1, 0);
    displayLayout->addWidget(m_showStatusTips, 2, 0);

    auto* scriptsGroup  = new QGroupBox(tr("Scripts"), this);
    auto* scriptsLayout = new QGridLayout(scriptsGroup);

    scriptsLayout->addWidget(new QLabel(tr("Playing track") + QStringLiteral(":"), this), 0, 0);
    scriptsLayout->addWidget(m_playingScript, 1, 0);
    scriptsLayout->addWidget(new QLabel(tr("Track selection") + QStringLiteral(":"), this), 2, 0);
    scriptsLayout->addWidget(m_selectionScript, 3, 0);

    auto* layout = new QGridLayout(this);
    layout->addWidget(displayGroup, 0, 0);
    layout->addWidget(scriptsGroup, 1, 0);

    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);
}

void StatusWidgetPageWidget::load()
{
    m_showIcon->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    m_showSelection->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowSelection>());
    m_playingScript->setText(m_settings->value<Settings::Gui::Internal::StatusPlayingScript>());
    m_selectionScript->setText(m_settings->value<Settings::Gui::Internal::StatusSelectionScript>());
    m_showStatusTips->setChecked(m_settings->value<Settings::Gui::ShowStatusTips>());
}

void StatusWidgetPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::StatusShowIcon>(m_showIcon->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusShowSelection>(m_showSelection->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusPlayingScript>(m_playingScript->text());
    m_settings->set<Settings::Gui::Internal::StatusSelectionScript>(m_selectionScript->text());
    m_settings->set<Settings::Gui::ShowStatusTips>(m_showStatusTips->isChecked());
}

void StatusWidgetPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::StatusShowIcon>();
    m_settings->reset<Settings::Gui::Internal::StatusShowSelection>();
    m_settings->reset<Settings::Gui::Internal::StatusPlayingScript>();
    m_settings->reset<Settings::Gui::Internal::StatusSelectionScript>();
    m_settings->reset<Settings::Gui::ShowStatusTips>();
}

StatusWidgetPage::StatusWidgetPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::StatusWidget);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Status Widget")});
    setWidgetCreator([settings] { return new StatusWidgetPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_statuswidgetpage.cpp"
#include "statuswidgetpage.moc"
