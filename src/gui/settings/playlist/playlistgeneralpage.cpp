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

#include "playlistgeneralpage.h"

#include "internalguisettings.h"

#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <core/playlist/playlistloader.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class PlaylistGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistGeneralPageWidget(const QStringList& playlistExtensions, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void browseExportPath();

    QStringList m_playlistExtensions;
    SettingsManager* m_settings;

    QSpinBox* m_preloadCount;

    QComboBox* m_middleClick;

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;

    QCheckBox* m_tabsExpand;
    QCheckBox* m_tabsAddButton;
    QCheckBox* m_tabsClearButton;
    QCheckBox* m_tabsCloseButton;
    QCheckBox* m_tabsMiddleClose;

    QSpinBox* m_imagePadding;
    QSpinBox* m_imagePaddingTop;

    QGroupBox* m_autoExporting;
    QComboBox* m_exportPathType;
    QCheckBox* m_exportMetadata;
    QComboBox* m_autoExportType;
    QLineEdit* m_autoExportPath;
};

PlaylistGeneralPageWidget::PlaylistGeneralPageWidget(const QStringList& playlistExtensions, SettingsManager* settings)
    : m_playlistExtensions{playlistExtensions}
    , m_settings{settings}
    , m_preloadCount{new QSpinBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_scrollBars{new QCheckBox(tr("Show scrollbar"), this)}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_altColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_tabsExpand{new QCheckBox(tr("Expand tabs to fill empty space"), this)}
    , m_tabsAddButton{new QCheckBox(tr("Show add button"), this)}
    , m_tabsClearButton{new QCheckBox(tr("Show clear button"), this)}
    , m_tabsCloseButton{new QCheckBox(tr("Show delete button on tabs"), this)}
    , m_tabsMiddleClose{new QCheckBox(tr("Delete playlists on middle click"), this)}
    , m_imagePadding{new QSpinBox(this)}
    , m_imagePaddingTop{new QSpinBox(this)}
    , m_autoExporting{new QGroupBox(tr("Auto-export"), this)}
    , m_exportPathType{new QComboBox(this)}
    , m_exportMetadata{new QCheckBox(tr("Write metadata"), this)}
    , m_autoExportType{new QComboBox(this)}
    , m_autoExportPath{new QLineEdit(this)}
{
    auto* behaviour       = new QGroupBox(tr("Behaviour"), this);
    auto* behaviourLayout = new QGridLayout(behaviour);

    auto* preloadCountLabel = new QLabel(tr("Preload count") + ":"_L1, this);
    const auto preloadTooltip
        = tr("Number of tracks used to preload the playlist before loading the rest of the playlist");
    preloadCountLabel->setToolTip(preloadTooltip);
    m_preloadCount->setToolTip(preloadTooltip);

    m_preloadCount->setMinimum(0);
    m_preloadCount->setMaximum(10000);
    m_preloadCount->setSuffix(tr(" tracks"));

    int row{0};
    behaviourLayout->addWidget(preloadCountLabel, row, 0);
    behaviourLayout->addWidget(m_preloadCount, row++, 1);
    behaviourLayout->addWidget(new QLabel(u"🛈 "_s + tr("Set to '0' to disable preloading."), this), row++, 0, 1, 2);
    behaviourLayout->setColumnStretch(behaviourLayout->columnCount(), 1);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    auto* middleClickLabel = new QLabel(tr("Middle-click") + ":"_L1, this);

    row = 0;
    clickBehaviourLayout->addWidget(middleClickLabel, row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->setColumnStretch(clickBehaviourLayout->columnCount(), 1);

    m_imagePadding->setMinimum(0);
    m_imagePadding->setMaximum(100);
    m_imagePadding->setSuffix(u"px"_s);

    m_imagePaddingTop->setMinimum(0);
    m_imagePaddingTop->setMaximum(100);
    m_imagePaddingTop->setSuffix(u"px"_s);

    auto* saving       = new QGroupBox(tr("Saving"), this);
    auto* savingLayout = new QGridLayout(saving);

    auto* pathTypeLabel = new QLabel(tr("Path type") + ":"_L1, this);

    row = 0;
    savingLayout->addWidget(pathTypeLabel, row, 0);
    savingLayout->addWidget(m_exportPathType, row++, 1);
    savingLayout->addWidget(m_exportMetadata, row++, 0, 1, 2);
    savingLayout->setColumnStretch(2, 1);

    auto* autoTypeLabel = new QLabel(tr("Format") + ":"_L1, this);
    auto* autoPathLabel = new QLabel(tr("Location") + ":"_L1, this);

    m_autoExporting->setToolTip(tr("Export and synchronise playlists in the specified format and location"));

    auto* browseAction = new QAction(Utils::iconFromTheme(Constants::Icons::Options), {}, this);
    QObject::connect(browseAction, &QAction::triggered, this, &PlaylistGeneralPageWidget::browseExportPath);
    m_autoExportPath->addAction(browseAction, QLineEdit::TrailingPosition);

    auto* autoExportLayout = new QGridLayout(m_autoExporting);
    m_autoExporting->setCheckable(true);

    row = 0;
    autoExportLayout->addWidget(autoTypeLabel, row, 0);
    autoExportLayout->addWidget(m_autoExportType, row++, 1);
    autoExportLayout->addWidget(autoPathLabel, row, 0);
    autoExportLayout->addWidget(m_autoExportPath, row++, 1, 1, 2);
    autoExportLayout->setColumnStretch(2, 1);

    auto* padding       = new QGroupBox(tr("Image Padding"), this);
    auto* paddingLayout = new QGridLayout(padding);

    row = 0;
    paddingLayout->addWidget(new QLabel(tr("Left/Right") + u":"_s, this), row, 0);
    paddingLayout->addWidget(m_imagePadding, row++, 1);
    paddingLayout->addWidget(new QLabel(tr("Top") + u":"_s, this), row, 0);
    paddingLayout->addWidget(m_imagePaddingTop, row++, 1);
    paddingLayout->setColumnStretch(2, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    row = 0;
    appearanceLayout->addWidget(m_scrollBars, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_header, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_altColours, row++, 0, 1, 2);
    appearanceLayout->addWidget(padding, row, 0, 1, 3);
    appearanceLayout->setColumnStretch(2, 1);
    appearanceLayout->setRowStretch(appearanceLayout->rowCount(), 1);

    auto* tabsGroup       = new QGroupBox(tr("Playlist Tabs"), this);
    auto* tabsGroupLayout = new QGridLayout(tabsGroup);

    row = 0;
    tabsGroupLayout->addWidget(m_tabsExpand, row++, 0);
    tabsGroupLayout->addWidget(m_tabsAddButton, row++, 0);
    tabsGroupLayout->addWidget(m_tabsClearButton, row++, 0);
    tabsGroupLayout->addWidget(m_tabsCloseButton, row++, 0);
    tabsGroupLayout->addWidget(m_tabsMiddleClose, row++, 0);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(behaviour, row++, 0);
    mainLayout->addWidget(clickBehaviour, row++, 0);
    mainLayout->addWidget(saving, row++, 0);
    mainLayout->addWidget(m_autoExporting, row++, 0);
    mainLayout->addWidget(appearance, row++, 0);
    mainLayout->addWidget(tabsGroup, row++, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    m_exportPathType->addItem(u"Auto"_s);
    m_exportPathType->addItem(u"Absolute"_s);
    m_exportPathType->addItem(u"Relative"_s);
}

void PlaylistGeneralPageWidget::load()
{
    m_preloadCount->setValue(m_settings->value<Settings::Gui::Internal::PlaylistTrackPreloadCount>());

    using ActionIndexMap = std::map<int, int>;
    ActionIndexMap middleActions;

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action, ActionIndexMap& actionMap) {
        const int actionValue = static_cast<int>(action);
        actionMap.emplace(actionValue, box->count());
        box->addItem(text, actionValue);
    };

    addTrackAction(m_middleClick, tr("None"), TrackAction::None, middleActions);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue, middleActions);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext, middleActions);
    addTrackAction(m_middleClick, tr("Send to playback queue"), TrackAction::SendToQueue, middleActions);

    auto middleAction = m_settings->value<Settings::Gui::Internal::PlaylistMiddleClick>();
    if(middleActions.contains(middleAction)) {
        m_middleClick->setCurrentIndex(middleActions.at(middleAction));
    }

    m_exportPathType->setCurrentIndex(m_settings->fileValue(Settings::Core::Internal::PlaylistSavePathType, 0).toInt());
    m_exportMetadata->setChecked(m_settings->fileValue(Settings::Core::Internal::PlaylistSaveMetadata, false).toBool());

    m_autoExportType->clear();
    for(const QString& ext : std::as_const(m_playlistExtensions)) {
        m_autoExportType->addItem(ext);
    }

    m_autoExporting->setChecked(m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylists, false).toBool());
    m_autoExportType->setCurrentText(
        m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylistsType, u"m3u8"_s).toString());
    m_autoExportPath->setText(
        m_settings->fileValue(Settings::Core::Internal::AutoExportPlaylistsPath, Core::playlistsPath()).toString());

    m_scrollBars->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistAltColours>());

    m_tabsExpand->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsExpand>());
    m_tabsAddButton->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsAddButton>());
    m_tabsClearButton->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsClearButton>());
    m_tabsCloseButton->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsCloseButton>());
    m_tabsMiddleClose->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistTabsMiddleClose>());

    m_imagePadding->setValue(m_settings->value<Settings::Gui::Internal::PlaylistImagePadding>());
    m_imagePaddingTop->setValue(m_settings->value<Settings::Gui::Internal::PlaylistImagePaddingTop>());
}

void PlaylistGeneralPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::PlaylistTrackPreloadCount>(m_preloadCount->value());

    m_settings->set<Settings::Gui::Internal::PlaylistMiddleClick>(m_middleClick->currentData().toInt());

    m_settings->fileSet(Settings::Core::Internal::PlaylistSavePathType, m_exportPathType->currentIndex());
    m_settings->fileSet(Settings::Core::Internal::PlaylistSaveMetadata, m_exportMetadata->isChecked());

    m_settings->fileSet(Settings::Core::Internal::AutoExportPlaylists, m_autoExporting->isChecked());
    m_settings->fileSet(Settings::Core::Internal::AutoExportPlaylistsType, m_autoExportType->currentText());
    m_settings->fileSet(Settings::Core::Internal::AutoExportPlaylistsPath, m_autoExportPath->text());

    m_settings->set<Settings::Gui::Internal::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistAltColours>(m_altColours->isChecked());

    m_settings->set<Settings::Gui::Internal::PlaylistTabsExpand>(m_tabsExpand->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistTabsAddButton>(m_tabsAddButton->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistTabsClearButton>(m_tabsClearButton->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistTabsCloseButton>(m_tabsCloseButton->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistTabsMiddleClose>(m_tabsMiddleClose->isChecked());

    m_settings->set<Settings::Gui::Internal::PlaylistImagePadding>(m_imagePadding->value());
    m_settings->set<Settings::Gui::Internal::PlaylistImagePaddingTop>(m_imagePaddingTop->value());
}

void PlaylistGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::PlaylistTrackPreloadCount>();

    m_settings->reset<Settings::Gui::Internal::PlaylistMiddleClick>();

    m_settings->fileRemove(Settings::Core::Internal::PlaylistSavePathType);
    m_settings->fileRemove(Settings::Core::Internal::PlaylistSaveMetadata);

    m_settings->fileRemove(Settings::Core::Internal::AutoExportPlaylists);
    m_settings->fileRemove(Settings::Core::Internal::AutoExportPlaylistsType);
    m_settings->fileRemove(Settings::Core::Internal::AutoExportPlaylistsPath);

    m_settings->reset<Settings::Gui::Internal::PlaylistScrollBar>();
    m_settings->reset<Settings::Gui::Internal::PlaylistHeader>();
    m_settings->reset<Settings::Gui::Internal::PlaylistAltColours>();

    m_settings->reset<Settings::Gui::Internal::PlaylistTabsExpand>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsAddButton>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsClearButton>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsCloseButton>();
    m_settings->reset<Settings::Gui::Internal::PlaylistTabsMiddleClose>();

    m_settings->reset<Settings::Gui::Internal::PlaylistImagePadding>();
    m_settings->reset<Settings::Gui::Internal::PlaylistImagePaddingTop>();
}

void PlaylistGeneralPageWidget::browseExportPath()
{
    const QString path = !m_autoExportPath->text().isEmpty() ? m_autoExportPath->text() : Core::playlistsPath();
    const QString dir
        = QFileDialog::getExistingDirectory(this, tr("Select Directory"), path, QFileDialog::DontResolveSymlinks);
    if(!dir.isEmpty()) {
        m_autoExportPath->setText(dir);
    }
}

PlaylistGeneralPage::PlaylistGeneralPage(const QStringList& playlistExtensions, SettingsManager* settings,
                                         QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaylistGeneral);
    setName(tr("General"));
    setCategory({tr("Playlist")});
    setWidgetCreator(
        [playlistExtensions, settings] { return new PlaylistGeneralPageWidget(playlistExtensions, settings); });
}
} // namespace Fooyin

#include "moc_playlistgeneralpage.cpp"
#include "playlistgeneralpage.moc"
