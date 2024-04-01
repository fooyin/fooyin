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

#include "filtersgeneralpage.h"

#include "constants.h"
#include "filterfwd.h"
#include "filtersettings.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/colourbutton.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Fooyin::Filters {
class FiltersGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit FiltersGeneralPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_filterHeaders;
    QCheckBox* m_filterScrollBars;
    QCheckBox* m_altRowColours;

    QPushButton* m_fontButton;
    QFont m_font;
    bool m_fontChanged{false};
    ColourButton* m_colourButton;
    QSpinBox* m_rowHeight;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QLineEdit* m_playlistName;
};

FiltersGeneralPageWidget::FiltersGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_filterHeaders{new QCheckBox(tr("Show Headers"), this)}
    , m_filterScrollBars{new QCheckBox(tr("Show Scrollbars"), this)}
    , m_altRowColours{new QCheckBox(tr("Alternating Row Colours"), this)}
    , m_fontButton{new QPushButton(Utils::iconFromTheme(Fooyin::Constants::Icons::Font), QStringLiteral(""), this)}
    , m_colourButton{new ColourButton(this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_playlistName{new QLineEdit(this)}
{
    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    auto* rowHeightLabel = new QLabel(tr("Row Height") + QStringLiteral(":"), this);
    auto* fontLabel      = new QLabel(tr("Font") + QStringLiteral(":"), this);
    auto* colourLabel    = new QLabel(tr("Colour") + QStringLiteral(":"), this);

    int row{0};
    appearanceLayout->addWidget(m_filterHeaders, row, 0);
    appearanceLayout->addWidget(rowHeightLabel, row, 1);
    appearanceLayout->addWidget(m_rowHeight, row++, 2);
    appearanceLayout->addWidget(m_filterScrollBars, row, 0);
    appearanceLayout->addWidget(fontLabel, row, 1);
    appearanceLayout->addWidget(m_fontButton, row++, 2);
    appearanceLayout->addWidget(m_altRowColours, row, 0);
    appearanceLayout->addWidget(colourLabel, row, 1);
    appearanceLayout->addWidget(m_colourButton, row++, 2);
    appearanceLayout->setColumnStretch(appearanceLayout->columnCount(), 1);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* doubleClickLabel = new QLabel(tr("Double-click") + QStringLiteral(":"), this);
    auto* middleClickLabel = new QLabel(tr("Middle-click") + QStringLiteral(":"), this);

    clickBehaviourLayout->addWidget(doubleClickLabel, 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(middleClickLabel, 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Filter Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    auto* playlistNameLabel = new QLabel(tr("Name") + QStringLiteral(":"), this);

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(playlistNameLabel, 2, 0, 1, 1);
    selectionPlaylistLayout->addWidget(m_playlistName, 2, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(clickBehaviour, 0, 0);
    mainLayout->addWidget(appearance, 1, 0);
    mainLayout->addWidget(selectionPlaylist, 2, 0);
    // mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(m_fontButton, &QPushButton::pressed, this, [this]() {
        bool ok;
        const QFont chosenFont = QFontDialog::getFont(&ok, m_font, this, tr("Select Font"));
        if(ok && chosenFont != m_font) {
            m_fontChanged = true;
            m_font        = chosenFont;
        }
    });
}

void FiltersGeneralPageWidget::load()
{
    using ActionIndexMap = std::map<int, int>;
    ActionIndexMap doubleActions;
    ActionIndexMap middleActions;

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action, ActionIndexMap& actionMap) {
        const int actionValue = static_cast<int>(action);
        actionMap.emplace(actionValue, box->count());
        box->addItem(text, actionValue);
    };

    m_doubleClick->clear();
    m_middleClick->clear();

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, doubleActions);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None, middleActions);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, middleActions);

    auto doubleAction = m_settings->value<Settings::Filters::FilterDoubleClick>();
    if(doubleActions.contains(doubleAction)) {
        m_doubleClick->setCurrentIndex(doubleActions.at(doubleAction));
    }

    auto middleAction = m_settings->value<Settings::Filters::FilterMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    QObject::connect(m_playlistEnabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
    });

    m_filterHeaders->setChecked(m_settings->value<Settings::Filters::FilterHeader>());
    m_filterScrollBars->setChecked(m_settings->value<Settings::Filters::FilterScrollBar>());
    m_altRowColours->setChecked(m_settings->value<Settings::Filters::FilterAltColours>());

    const QString font = m_settings->value<Settings::Filters::FilterFont>();
    m_font.fromString(font);
    m_colourButton->setColour(m_settings->value<Settings::Filters::FilterColour>());
    m_rowHeight->setValue(m_settings->value<Settings::Filters::FilterRowHeight>());

    m_fontButton->setText(QStringLiteral("%1 (%2)").arg(m_font.family()).arg(m_font.pointSize()));

    m_playlistEnabled->setChecked(m_settings->value<Settings::Filters::FilterPlaylistEnabled>());
    m_autoSwitch->setChecked(m_settings->value<Settings::Filters::FilterAutoSwitch>());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());

    m_playlistName->setText(m_settings->value<Settings::Filters::FilterAutoPlaylist>());
}

void FiltersGeneralPageWidget::apply()
{
    m_settings->set<Settings::Filters::FilterHeader>(m_filterHeaders->isChecked());
    m_settings->set<Settings::Filters::FilterScrollBar>(m_filterScrollBars->isChecked());
    m_settings->set<Settings::Filters::FilterAltColours>(m_altRowColours->isChecked());

    m_settings->set<Settings::Filters::FilterFont>(m_font.toString());
    m_settings->set<Settings::Filters::FilterColour>(m_colourButton->colour().name());
    m_settings->set<Settings::Filters::FilterRowHeight>(m_rowHeight->value());

    m_settings->set<Settings::Filters::FilterDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::Filters::FilterMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::Filters::FilterPlaylistEnabled>(m_playlistEnabled->isChecked());
    m_settings->set<Settings::Filters::FilterAutoSwitch>(m_autoSwitch->isChecked());
    m_settings->set<Settings::Filters::FilterAutoPlaylist>(m_playlistName->text());
}

void FiltersGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Filters::FilterHeader>();
    m_settings->reset<Settings::Filters::FilterScrollBar>();
    m_settings->reset<Settings::Filters::FilterAltColours>();

    m_settings->reset<Settings::Filters::FilterFont>();
    m_settings->reset<Settings::Filters::FilterColour>();
    m_settings->reset<Settings::Filters::FilterRowHeight>();

    m_settings->reset<Settings::Filters::FilterDoubleClick>();
    m_settings->reset<Settings::Filters::FilterMiddleClick>();
    m_settings->reset<Settings::Filters::FilterPlaylistEnabled>();
    m_settings->reset<Settings::Filters::FilterAutoSwitch>();
    m_settings->reset<Settings::Filters::FilterAutoPlaylist>();
}

FiltersGeneralPage::FiltersGeneralPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersGeneral);
    setName(tr("General"));
    setCategory({tr("Plugins"), tr("Filters")});
    setWidgetCreator([settings] { return new FiltersGeneralPageWidget(settings); });
}
} // namespace Fooyin::Filters

#include "filtersgeneralpage.moc"
#include "moc_filtersgeneralpage.cpp"
