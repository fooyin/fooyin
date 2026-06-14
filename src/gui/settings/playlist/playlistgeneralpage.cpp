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

#include "playlistgeneralpage.h"

#include "internalguisettings.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class PlaylistGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistGeneralPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QSpinBox* m_preloadCount;
    QComboBox* m_middleClick;
    QCheckBox* m_inlineTagEditing;
    QCheckBox* m_skipMissing;
    QCheckBox* m_ignoreFolderPlaylists;
    QCheckBox* m_preventDuplicates;
    QCheckBox* m_integratedSearch;
    QComboBox* m_searchMode;
    ScriptLineEdit* m_searchScript;
};

PlaylistGeneralPageWidget::PlaylistGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_preloadCount{new QSpinBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_inlineTagEditing{new QCheckBox(tr("Enable inline tag editing"), this)}
    , m_skipMissing{new QCheckBox(tr("Skip missing tracks"), this)}
    , m_ignoreFolderPlaylists{new QCheckBox(tr("Ignore playlist files when adding folders"), this)}
    , m_preventDuplicates{new QCheckBox(tr("Prevent duplicate tracks when loading playlists"), this)}
    , m_integratedSearch{new QCheckBox(tr("Open search bar instead of pop-up playlist search"), this)}
    , m_searchMode{new QComboBox(this)}
    , m_searchScript{new ScriptLineEdit(this)}
{
    auto* behaviour       = new QGroupBox(tr("Behaviour"), this);
    auto* behaviourLayout = new QGridLayout(behaviour);

    auto* preloadCountLabel = new QLabel(tr("Tracks to preload") + ":"_L1, this);
    const auto preloadTooltip
        = tr("Number of tracks used to preload the playlist before loading the rest of the playlist");
    preloadCountLabel->setToolTip(preloadTooltip);
    m_preloadCount->setToolTip(preloadTooltip);

    m_preloadCount->setMinimum(0);
    m_preloadCount->setMaximum(10000);
    m_inlineTagEditing->setToolTip(tr("Allow editing writable track tag columns directly from the playlist"));

    int row{0};
    behaviourLayout->addWidget(preloadCountLabel, row, 0);
    behaviourLayout->addWidget(m_preloadCount, row++, 1);
    behaviourLayout->addWidget(new QLabel(u"🛈 "_s + tr("Set to '0' to disable preloading."), this), row++, 0, 1, 2);
    behaviourLayout->addWidget(m_inlineTagEditing, row++, 0, 1, 2);
    behaviourLayout->setColumnStretch(behaviourLayout->columnCount(), 1);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    row = 0;
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + ":"_L1, this), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->setColumnStretch(clickBehaviourLayout->columnCount(), 1);

    m_skipMissing->setToolTip(tr("Skip unavailable tracks when loading playlists"));
    m_ignoreFolderPlaylists->setToolTip(
        tr("Only add media files from folders, without loading playlist files found inside"));
    m_preventDuplicates->setToolTip(tr("Skip playlist entries that are already present in the target playlist"));

    auto* loading       = new QGroupBox(tr("Loading"), this);
    auto* loadingLayout = new QGridLayout(loading);

    row = 0;
    loadingLayout->addWidget(m_skipMissing, row++, 0);
    loadingLayout->addWidget(m_ignoreFolderPlaylists, row++, 0);
    loadingLayout->addWidget(m_preventDuplicates, row++, 0);

    auto* search       = new QGroupBox(tr("Search"), this);
    auto* searchLayout = new QGridLayout(search);

    m_searchMode->addItem(tr("Match beginnings of words"), 0);
    m_searchMode->addItem(tr("Match anywhere"), 1);

    row = 0;
    searchLayout->addWidget(m_integratedSearch, row++, 0, 1, 2);
    searchLayout->addWidget(new QLabel(tr("Search mode") + ":"_L1, this), row, 0);
    searchLayout->addWidget(m_searchMode, row++, 1);
    searchLayout->addWidget(new QLabel(tr("Search script") + ":"_L1, this), row, 0);
    searchLayout->addWidget(m_searchScript, row++, 1);
    searchLayout->setColumnStretch(1, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(behaviour, row++, 0);
    mainLayout->addWidget(clickBehaviour, row++, 0);
    mainLayout->addWidget(loading, row++, 0);
    mainLayout->addWidget(search, row++, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);
}

void PlaylistGeneralPageWidget::load()
{
    m_preloadCount->setValue(m_settings->value<Settings::Gui::Internal::PlaylistTrackPreloadCount>());
    m_inlineTagEditing->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistInlineTagEditing>());

    m_middleClick->clear();
    TrackSelectionController::addAction(m_middleClick, tr("None"), TrackAction::None);
    TrackSelectionController::addStandardActions(m_middleClick, ActionGroup::Queue);
    TrackSelectionController::setCurrentAction(m_middleClick,
                                               m_settings->value<Settings::Gui::Internal::PlaylistMiddleClick>());

    m_skipMissing->setChecked(m_settings->value<Settings::Core::PlaylistSkipMissing>());
    m_ignoreFolderPlaylists->setChecked(m_settings->value<Settings::Core::AddFoldersIgnorePlaylists>());
    m_preventDuplicates->setChecked(m_settings->value<Settings::Core::PlaylistPreventDuplicates>());
    m_integratedSearch->setChecked(m_settings->value<Settings::Gui::PlaylistIntegratedSearch>());
    m_searchMode->setCurrentIndex(m_searchMode->findData(m_settings->value<Settings::Gui::PlaylistSearchMode>()));
    m_searchScript->setText(m_settings->value<Settings::Gui::PlaylistSearchScript>());
}

void PlaylistGeneralPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::PlaylistTrackPreloadCount>(m_preloadCount->value());
    m_settings->set<Settings::Gui::Internal::PlaylistInlineTagEditing>(m_inlineTagEditing->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistMiddleClick>(m_middleClick->currentData().toInt());
    m_settings->set<Settings::Core::PlaylistSkipMissing>(m_skipMissing->isChecked());
    m_settings->set<Settings::Core::AddFoldersIgnorePlaylists>(m_ignoreFolderPlaylists->isChecked());
    m_settings->set<Settings::Core::PlaylistPreventDuplicates>(m_preventDuplicates->isChecked());
    m_settings->set<Settings::Gui::PlaylistIntegratedSearch>(m_integratedSearch->isChecked());
    m_settings->set<Settings::Gui::PlaylistSearchMode>(m_searchMode->currentData().toInt());
    m_settings->set<Settings::Gui::PlaylistSearchScript>(m_searchScript->text());
}

void PlaylistGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::PlaylistTrackPreloadCount>();
    m_settings->reset<Settings::Gui::Internal::PlaylistInlineTagEditing>();
    m_settings->reset<Settings::Gui::Internal::PlaylistMiddleClick>();
    m_settings->reset<Settings::Core::PlaylistSkipMissing>();
    m_settings->reset<Settings::Core::AddFoldersIgnorePlaylists>();
    m_settings->reset<Settings::Core::PlaylistPreventDuplicates>();
    m_settings->reset<Settings::Gui::PlaylistIntegratedSearch>();
    m_settings->reset<Settings::Gui::PlaylistSearchMode>();
    m_settings->reset<Settings::Gui::PlaylistSearchScript>();
}

PlaylistGeneralPage::PlaylistGeneralPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaylistGeneral);
    setName(tr("General"));
    setCategory({tr("Playlist")});
    setWidgetCreator([settings] { return new PlaylistGeneralPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playlistgeneralpage.cpp"
#include "playlistgeneralpage.moc"
