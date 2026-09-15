/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include "widgets/statuswidget.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

using namespace Qt::StringLiterals;

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
    QCheckBox* m_showPlaylist;
    QCheckBox* m_showStatusTips;
    QComboBox* m_doubleClick;
    QComboBox* m_middleClick;
    ScriptLineEdit* m_playingScript;
    ScriptLineEdit* m_selectionScript;
    ScriptLineEdit* m_playlistScript;
};

StatusWidgetPageWidget::StatusWidgetPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showIcon{new QCheckBox(tr("Show icon"), this)}
    , m_showSelection{new QCheckBox(tr("Show selection info"), this)}
    , m_showPlaylist{new QCheckBox(tr("Show current playlist info"), this)}
    , m_showStatusTips{new QCheckBox(tr("Show action tips"), this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_playingScript{new ScriptLineEdit(this)}
    , m_selectionScript{new ScriptLineEdit(this)}
    , m_playlistScript{new ScriptLineEdit(this)}
{
    auto* displayGroup  = new QGroupBox(tr("Display"), this);
    auto* displayLayout = new QGridLayout(displayGroup);

    displayLayout->addWidget(m_showIcon, 0, 0);
    displayLayout->addWidget(m_showSelection, 1, 0);
    displayLayout->addWidget(m_showPlaylist, 2, 0);
    displayLayout->addWidget(m_showStatusTips, 3, 0);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    const auto addActions = [](QComboBox* box) {
        box->addItem(tr("None"), static_cast<int>(StatusAction::None));
        box->addItem(tr("Show track"), static_cast<int>(StatusAction::ShowTrack));
        box->addItem(tr("Open containing folder"), static_cast<int>(StatusAction::OpenContainingFolder));
        box->addItem(tr("Open properties"), static_cast<int>(StatusAction::OpenProperties));
    };
    addActions(m_doubleClick);
    addActions(m_middleClick);

    int row{0};
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, clickBehaviour), row, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, row++, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, clickBehaviour), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->setColumnStretch(row, 1);

    auto* scriptsGroup  = new QGroupBox(tr("Scripts"), this);
    auto* scriptsLayout = new QGridLayout(scriptsGroup);

    auto* playlistHint = new QLabel(
        u"🛈 "_s + tr("Shown in the status bar when no tracks are selected, or when selection info is disabled."), this);
    playlistHint->setWordWrap(true);

    scriptsLayout->addWidget(new QLabel(tr("Playing track") + u":"_s, this), 0, 0);
    scriptsLayout->addWidget(m_playingScript, 1, 0);
    scriptsLayout->addWidget(new QLabel(tr("Track selection") + u":"_s, this), 2, 0);
    scriptsLayout->addWidget(m_selectionScript, 3, 0);
    scriptsLayout->addWidget(new QLabel(tr("Current playlist") + u":"_s, this), 4, 0);
    scriptsLayout->addWidget(m_playlistScript, 5, 0);
    scriptsLayout->addWidget(playlistHint, 6, 0);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(displayGroup, row++, 0);
    layout->addWidget(clickBehaviour, row++, 0);
    layout->addWidget(scriptsGroup, row++, 0);
    layout->setRowStretch(row, 1);
    layout->setColumnStretch(0, 1);

    m_settings->subscribe<Settings::Gui::Internal::StatusShowIcon>(m_showIcon, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Gui::Internal::StatusShowSelection>(m_showSelection, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Gui::Internal::StatusShowPlaylist>(m_showPlaylist, &QCheckBox::setChecked);
    m_settings->subscribe<Settings::Gui::ShowStatusTips>(m_showStatusTips, &QCheckBox::setChecked);
}

void StatusWidgetPageWidget::load()
{
    m_showIcon->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowIcon>());
    m_showSelection->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowSelection>());
    m_showPlaylist->setChecked(m_settings->value<Settings::Gui::Internal::StatusShowPlaylist>());
    m_playingScript->setText(m_settings->value<Settings::Gui::Internal::StatusPlayingScript>());
    m_selectionScript->setText(m_settings->value<Settings::Gui::Internal::StatusSelectionScript>());
    m_playlistScript->setText(m_settings->value<Settings::Gui::Internal::StatusPlaylistScript>());
    m_showStatusTips->setChecked(m_settings->value<Settings::Gui::ShowStatusTips>());
    m_doubleClick->setCurrentIndex(
        m_doubleClick->findData(m_settings->value<Settings::Gui::Internal::StatusDoubleClick>()));
    m_middleClick->setCurrentIndex(
        m_middleClick->findData(m_settings->value<Settings::Gui::Internal::StatusMiddleClick>()));
}

void StatusWidgetPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::StatusShowIcon>(m_showIcon->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusShowSelection>(m_showSelection->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusShowPlaylist>(m_showPlaylist->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusPlayingScript>(m_playingScript->text());
    m_settings->set<Settings::Gui::Internal::StatusSelectionScript>(m_selectionScript->text());
    m_settings->set<Settings::Gui::Internal::StatusPlaylistScript>(m_playlistScript->text());
    m_settings->set<Settings::Gui::ShowStatusTips>(m_showStatusTips->isChecked());
    m_settings->set<Settings::Gui::Internal::StatusDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::StatusMiddleClick>(m_middleClick->currentData().toInt());
}

void StatusWidgetPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::StatusShowIcon>();
    m_settings->reset<Settings::Gui::Internal::StatusShowSelection>();
    m_settings->reset<Settings::Gui::Internal::StatusShowPlaylist>();
    m_settings->reset<Settings::Gui::Internal::StatusPlayingScript>();
    m_settings->reset<Settings::Gui::Internal::StatusSelectionScript>();
    m_settings->reset<Settings::Gui::Internal::StatusPlaylistScript>();
    m_settings->reset<Settings::Gui::ShowStatusTips>();
    m_settings->reset<Settings::Gui::Internal::StatusDoubleClick>();
    m_settings->reset<Settings::Gui::Internal::StatusMiddleClick>();
}

StatusWidgetPage::StatusWidgetPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::StatusWidget);
    setName(tr("General"));
    setCategory({tr("Interface"), tr("Status Bar")});
    setWidgetCreator([settings] { return new StatusWidgetPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_statuswidgetpage.cpp"
#include "statuswidgetpage.moc"
