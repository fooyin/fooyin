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

#include "librarytreepage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class LibraryTreePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryTreePageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QCheckBox* m_playbackOnSend;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QCheckBox* m_keepAlive;
    QLineEdit* m_playlistName;

    QCheckBox* m_restoreState;

    QCheckBox* m_animated;
    QCheckBox* m_header;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_altColours;
    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
};

LibraryTreePageWidget::LibraryTreePageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback on send"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_keepAlive{new QCheckBox(tr("Keep alive"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_restoreState{new QCheckBox(tr("Restore state on startup"), this)}
    , m_animated{new QCheckBox(tr("Expand/collapse animation"), this)}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_altColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
{
    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* doubleClickLabel = new QLabel(tr("Double-click") + u":"_s, this);
    auto* middleClickLabel = new QLabel(tr("Middle-click") + u":"_s, this);

    clickBehaviourLayout->addWidget(doubleClickLabel, 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(middleClickLabel, 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(clickBehaviourLayout->columnCount(), 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Library Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    auto* playlistNameLabel = new QLabel(tr("Name") + u":"_s, this);

    m_keepAlive->setToolTip(tr("If this is the active playlist, keep it alive when changing selection"));

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_keepAlive, 2, 0, 1, 3);
    selectionPlaylistLayout->addWidget(playlistNameLabel, 3, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, 3, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    generalGroupLayout->addWidget(m_restoreState);

    auto* appearanceGroup       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceGroupLayout = new QGridLayout(appearanceGroup);

    auto* iconGroup       = new QGroupBox(tr("Icon"), this);
    auto* iconGroupLayout = new QGridLayout(iconGroup);

    m_iconWidth->setSuffix(u"px"_s);
    m_iconHeight->setSuffix(u"px"_s);

    m_iconWidth->setMaximum(512);
    m_iconHeight->setMaximum(512);

    m_iconWidth->setSingleStep(5);
    m_iconHeight->setSingleStep(5);

    auto* iconSizeHint = new QLabel(
        u"🛈 "_s + tr("Size can also be changed using %1 in the widget.").arg(u"<b>Ctrl+Scroll</b>"_s), this);

    int row{0};
    iconGroupLayout->addWidget(new QLabel(tr("Width") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconWidth, row++, 1);
    iconGroupLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconHeight, row++, 1);
    iconGroupLayout->addWidget(iconSizeHint, row, 0, 1, 4);
    iconGroupLayout->setColumnStretch(2, 1);

    m_rowHeight->setMinimum(1);

    row = 0;
    appearanceGroupLayout->addWidget(m_animated, row++, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_header, row++, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_showScrollbar, row++, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_altColours, row++, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_overrideRowHeight, row, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_rowHeight, row++, 2);
    appearanceGroupLayout->addWidget(iconGroup, row++, 0, 1, 2);
    appearanceGroupLayout->setColumnStretch(appearanceGroupLayout->columnCount(), 1);
    appearanceGroupLayout->setRowStretch(appearanceGroupLayout->rowCount(), 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(generalGroup, 0, 0);
    mainLayout->addWidget(clickBehaviour, 1, 0);
    mainLayout->addWidget(selectionPlaylist, 2, 0);
    mainLayout->addWidget(appearanceGroup, 3, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_playlistEnabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
        m_keepAlive->setEnabled(checked);
    });
}

void LibraryTreePageWidget::load()
{
    m_restoreState->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeRestoreState>());

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

    auto doubleAction = m_settings->value<Settings::Gui::Internal::LibTreeDoubleClick>();
    if(doubleActions.contains(doubleAction)) {
        m_doubleClick->setCurrentIndex(doubleActions.at(doubleAction));
    }

    auto middleAction = m_settings->value<Settings::Gui::Internal::LibTreeMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    m_playbackOnSend->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeSendPlayback>());

    m_playlistEnabled->setChecked(m_settings->value<Settings::Gui::Internal::LibTreePlaylistEnabled>());
    m_autoSwitch->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeAutoSwitch>());
    m_keepAlive->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeKeepAlive>());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());
    m_keepAlive->setEnabled(m_playlistEnabled->isChecked());

    m_playlistName->setText(m_settings->value<Settings::Gui::Internal::LibTreeAutoPlaylist>());

    m_animated->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeAnimated>());
    m_header->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeHeader>());
    m_showScrollbar->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeScrollBar>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeAltColours>());
    m_rowHeight->setValue(m_settings->value<Settings::Gui::Internal::LibTreeRowHeight>());

    m_overrideRowHeight->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeRowHeight>() > 0);
    m_rowHeight->setValue(m_settings->value<Settings::Gui::Internal::LibTreeRowHeight>());
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());

    const auto iconSize = m_settings->value<Settings::Gui::Internal::LibTreeIconSize>().toSize();
    m_iconWidth->setValue(iconSize.width());
    m_iconHeight->setValue(iconSize.height());
}

void LibraryTreePageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::LibTreeRestoreState>(m_restoreState->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeDoubleClick>(m_doubleClick->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::LibTreeMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::LibTreeSendPlayback>(m_playbackOnSend->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreePlaylistEnabled>(m_playlistEnabled->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeAutoSwitch>(m_autoSwitch->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeKeepAlive>(m_keepAlive->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeAutoPlaylist>(m_playlistName->text());

    m_settings->set<Settings::Gui::Internal::LibTreeAnimated>(m_animated->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeHeader>(m_header->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeScrollBar>(m_showScrollbar->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeAltColours>(m_altColours->isChecked());

    if(m_overrideRowHeight->isChecked()) {
        m_settings->set<Settings::Gui::Internal::LibTreeRowHeight>(m_rowHeight->value());
    }
    else {
        m_settings->reset<Settings::Gui::Internal::LibTreeRowHeight>();
    }

    const QSize iconSize{m_iconWidth->value(), m_iconHeight->value()};
    m_settings->set<Settings::Gui::Internal::LibTreeIconSize>(iconSize);
}

void LibraryTreePageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::LibTreeRestoreState>();
    m_settings->reset<Settings::Gui::Internal::LibTreeDoubleClick>();
    m_settings->reset<Settings::Gui::Internal::LibTreeMiddleClick>();
    m_settings->reset<Settings::Gui::Internal::LibTreeSendPlayback>();
    m_settings->reset<Settings::Gui::Internal::LibTreePlaylistEnabled>();
    m_settings->reset<Settings::Gui::Internal::LibTreeAutoSwitch>();
    m_settings->reset<Settings::Gui::Internal::LibTreeKeepAlive>();
    m_settings->reset<Settings::Gui::Internal::LibTreeAutoPlaylist>();

    m_settings->reset<Settings::Gui::Internal::LibTreeAnimated>();
    m_settings->reset<Settings::Gui::Internal::LibTreeHeader>();
    m_settings->reset<Settings::Gui::Internal::LibTreeScrollBar>();
    m_settings->reset<Settings::Gui::Internal::LibTreeAltColours>();
    m_settings->reset<Settings::Gui::Internal::LibTreeRowHeight>();
    m_settings->reset<Settings::Gui::Internal::LibTreeIconSize>();
}

LibraryTreePage::LibraryTreePage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibraryTreeGeneral);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Library Tree")});
    setWidgetCreator([settings] { return new LibraryTreePageWidget(settings); });
}
} // namespace Fooyin

#include "librarytreepage.moc"
#include "moc_librarytreepage.cpp"
