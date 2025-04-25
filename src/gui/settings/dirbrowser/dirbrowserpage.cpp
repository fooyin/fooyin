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

#include "dirbrowserpage.h"

#include "dirbrowser/dirbrowser.h"
#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableView>

using namespace Qt::StringLiterals;

namespace Fooyin {
class DirBrowserPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DirBrowserPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QRadioButton* m_treeMode;
    QRadioButton* m_listMode;

    QCheckBox* m_showIcons;
    QCheckBox* m_indentList;
    QCheckBox* m_showHorizScrollbar;
    QCheckBox* m_showControls;
    QCheckBox* m_showLocation;

    QComboBox* m_doubleClick;
    QComboBox* m_middleClick;
    QCheckBox* m_playbackOnSend;
};

DirBrowserPageWidget::DirBrowserPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_treeMode{new QRadioButton(tr("Tree"), this)}
    , m_listMode{new QRadioButton(tr("List"), this)}
    , m_showIcons{new QCheckBox(tr("Show icons"), this)}
    , m_indentList{new QCheckBox(tr("Show indent"), this)}
    , m_showHorizScrollbar{new QCheckBox(tr("Show horizontal scrollbar"), this)}
    , m_showControls{new QCheckBox(tr("Show controls"), this)}
    , m_showLocation{new QCheckBox(tr("Show location"), this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback on send"), this)}
{
    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + ":"_L1, this), 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + ":"_L1, this), 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* browserMode       = new QGroupBox(tr("Browser Mode"), this);
    auto* browserModeLayout = new QVBoxLayout(browserMode);

    auto* modeGroup = new QButtonGroup(this);

    modeGroup->addButton(m_treeMode);
    browserModeLayout->addWidget(m_treeMode);
    modeGroup->addButton(m_listMode);
    browserModeLayout->addWidget(m_listMode);
    browserModeLayout->addStretch();

    auto* displayOptions       = new QGroupBox(tr("Display Options"), this);
    auto* displayOptionsLayout = new QGridLayout(displayOptions);

    displayOptionsLayout->addWidget(m_showIcons, 0, 0);
    displayOptionsLayout->addWidget(m_indentList, 1, 0);
    displayOptionsLayout->addWidget(m_showHorizScrollbar, 2, 0);
    displayOptionsLayout->addWidget(m_showControls, 3, 0);
    displayOptionsLayout->addWidget(m_showLocation, 4, 0);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(clickBehaviour, 0, 0);
    mainLayout->addWidget(browserMode, 1, 0);
    mainLayout->addWidget(displayOptions, 2, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    using ActionIndexMap = std::map<int, int>;
    ActionIndexMap doubleActions;
    ActionIndexMap middleActions;

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action, ActionIndexMap& actionMap) {
        const int actionValue = static_cast<int>(action);
        actionMap.emplace(actionValue, box->count());
        box->addItem(text, actionValue);
    };

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None, doubleActions);
    addTrackAction(m_doubleClick, tr("Expand/Collapse/Play"), TrackAction::Play, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to playback queue"), TrackAction::AddToQueue, doubleActions);
    addTrackAction(m_doubleClick, tr("Add to front of playback queue"), TrackAction::QueueNext, doubleActions);
    addTrackAction(m_doubleClick, tr("Send to playback queue"), TrackAction::SendToQueue, doubleActions);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None, middleActions);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist, middleActions);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue, middleActions);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext, middleActions);
    addTrackAction(m_middleClick, tr("Send to playback queue"), TrackAction::SendToQueue, middleActions);

    auto doubleAction = m_settings->value<Settings::Gui::Internal::DirBrowserDoubleClick>();
    if(doubleActions.contains(doubleAction)) {
        m_doubleClick->setCurrentIndex(doubleActions.at(doubleAction));
    }

    auto middleAction = m_settings->value<Settings::Gui::Internal::DirBrowserMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    QObject::connect(modeGroup, &QButtonGroup::buttonToggled, this, [this](QAbstractButton* button, bool checked) {
        if(button == m_treeMode) {
            m_indentList->setEnabled(!checked);
        }
    });
}

void DirBrowserPageWidget::load()
{
    m_showIcons->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserIcons>());
    m_indentList->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserListIndent>());
    m_showHorizScrollbar->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserShowHorizScroll>());
    m_showControls->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserControls>());
    m_showLocation->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserLocation>());

    const auto mode = static_cast<DirBrowser::Mode>(m_settings->value<Settings::Gui::Internal::DirBrowserMode>());
    if(mode == DirBrowser::Mode::List) {
        m_listMode->setChecked(true);
    }
    else {
        m_treeMode->setChecked(true);
    }

    m_playbackOnSend->setChecked(m_settings->value<Settings::Gui::Internal::DirBrowserSendPlayback>());
}

void DirBrowserPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::DirBrowserDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::DirBrowserMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::DirBrowserSendPlayback>(m_playbackOnSend->isChecked());
    m_settings->set<Settings::Gui::Internal::DirBrowserIcons>(m_showIcons->isChecked());
    m_settings->set<Settings::Gui::Internal::DirBrowserListIndent>(m_indentList->isChecked());
    m_settings->set<Settings::Gui::Internal::DirBrowserShowHorizScroll>(m_showHorizScrollbar->isChecked());
    m_settings->set<Settings::Gui::Internal::DirBrowserControls>(m_showControls->isChecked());
    m_settings->set<Settings::Gui::Internal::DirBrowserLocation>(m_showLocation->isChecked());

    if(m_listMode->isChecked()) {
        m_settings->set<Settings::Gui::Internal::DirBrowserMode>(static_cast<int>(DirBrowser::Mode::List));
    }
    else {
        m_settings->set<Settings::Gui::Internal::DirBrowserMode>(static_cast<int>(DirBrowser::Mode::Tree));
    }
}

void DirBrowserPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::DirBrowserDoubleClick>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserMiddleClick>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserSendPlayback>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserIcons>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserListIndent>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserShowHorizScroll>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserMode>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserControls>();
    m_settings->reset<Settings::Gui::Internal::DirBrowserLocation>();
}

DirBrowserPage::DirBrowserPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::DirBrowser);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Directory Browser")});
    setWidgetCreator([settings] { return new DirBrowserPageWidget(settings); });
}
} // namespace Fooyin

#include "dirbrowserpage.moc"
#include "moc_dirbrowserpage.cpp"
