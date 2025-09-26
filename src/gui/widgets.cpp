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

#include "widgets.h"

#include "artwork/artworkdialog.h"
#include "artwork/artworkfinder.h"
#include "artwork/artworkproperties.h"
#include "contextmenuids.h"
#include "controls/commandbutton.h"
#include "controls/outputselector.h"
#include "controls/playercontrol.h"
#include "controls/playlistcontrol.h"
#include "controls/seekbar.h"
#include "controls/volumecontrol.h"
#include "dirbrowser/dirbrowser.h"
#include "dsp/dsppresetregistry.h"
#include "dsp/dspsettingscontroller.h"
#include "dsp/dspsettingsregistry.h"
#include "dsp/resamplersettingswidget.h"
#include "dsp/skipsilencesettingswidget.h"
#include "gui/editablelayout.h"
#include "gui/plugins/guiplugincontext.h"
#include "guiapplication.h"
#include "internalguisettings.h"
#include "librarytree/librarytreecontroller.h"
#include "librarytree/librarytreewidget.h"
#include "mainwindow.h"
#include "nowplayingoutput/nowplayingoutputpage.h"
#include "nowplayingoutput/nowplayingoutputservice.h"
#include "output/outputprofilemanager.h"
#include "playlist/manager/playlistmanagerwidget.h"
#include "playlist/organiser/playlistorganiser.h"
#include "playlist/playlistbox.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlisttabs.h"
#include "playlist/playlistwidget.h"
#include "queueviewer/queueviewer.h"
#include "replaygain/replaygainwidget.h"
#include "scriptdisplay/scriptdisplay.h"
#include "search/searchwidget.h"
#include "selectioninfo/infowidget.h"
#include "settings/advanced/advancedpage.h"
#include "settings/artwork/artworkdownloadpage.h"
#include "settings/artwork/artworkgeneralpage.h"
#include "settings/artwork/artworksearchingpage.h"
#include "settings/artwork/artworksourcespage.h"
#include "settings/context/trackcontextmenupage.h"
#include "settings/generalpage.h"
#include "settings/guidisplaypage.h"
#include "settings/guigeneralpage.h"
#include "settings/guilayoutpage.h"
#include "settings/guithemespage.h"
#include "settings/guitrackdisplaypage.h"
#include "settings/library/librarygeneralpage.h"
#include "settings/library/librarymetadatapage.h"
#include "settings/library/libraryratingspage.h"
#include "settings/library/librarysortingpage.h"
#include "settings/networkpage.h"
#include "settings/playback/decoderpage.h"
#include "settings/playback/devicepage.h"
#include "settings/playback/dspmanagerpage.h"
#include "settings/playback/fadingpage.h"
#include "settings/playback/outputpage.h"
#include "settings/playback/playbackpage.h"
#include "settings/playback/replaygainpage.h"
#include "settings/playlist/playlistcolumnpage.h"
#include "settings/playlist/playlistgeneralpage.h"
#include "settings/playlist/playlistguipage.h"
#include "settings/playlist/playlistpresetspage.h"
#include "settings/playlist/playlistsavingpage.h"
#include "settings/playlist/playlisttabspage.h"
#include "settings/plugins/pluginspage.h"
#include "settings/searchpage.h"
#include "settings/shellintegrationpage.h"
#include "settings/shortcuts/shortcutspage.h"
#include "settings/widgets/statuswidgetpage.h"
#include "splitters/splitterwidget.h"
#include "splitters/tabstackwidget.h"
#include "widgets/coverwidget.h"
#include "widgets/dummy.h"
#include "widgets/spacer.h"
#include "widgets/statuswidget.h"

#include <core/application.h>
#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistloader.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/settings/context/staticcontextmenupage.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgetprovider.h>
#include <utils/settings/advancedsettingsregistry.h>

using namespace Qt::StringLiterals;

namespace {
QStringList normaliseExtensionList(QStringList extensions)
{
    for(QString& extension : extensions) {
        extension = extension.trimmed().toLower();
    }
    extensions.removeAll(QString{});
    extensions.removeDuplicates();
    return extensions;
}
} // namespace

namespace Fooyin {
Widgets::Widgets(Application* core, GuiApplication* gui, const GuiPluginContext& guiPluginContext,
                 MainWindow* mainWindow, PlaylistInteractor* playlistInteractor, QObject* parent)
    : QObject{parent}
    , m_core{core}
    , m_gui{gui}
    , m_window{mainWindow}
    , m_settings{m_core->settingsManager()}
    , m_coverRepository{guiPluginContext.coverRepository}
    , m_artworkFinder{new ArtworkFinder(m_core->networkManager(), m_settings, this)}
    , m_coverProvider{new CoverProvider(m_coverRepository, this)}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{playlistInteractor->playlistController()}
    , m_libraryTreeController{new LibraryTreeController(m_settings, this)}
    , m_dspPresetRegistry{new DspPresetRegistry(m_settings, this)}
    , m_outputProfileManager{new OutputProfileManager(m_core->engine(), m_core->dspChainStore(), m_dspPresetRegistry,
                                                      m_settings, this)}
    , m_nowPlayingOutputService{new NowPlayingOutputService(m_core->playerController(), m_settings, this)}
    , m_dspSettingsRegistry{std::make_unique<DspSettingsRegistry>()}
    , m_dspSettingsController{std::make_unique<DspSettingsController>(m_core->dspChainStore(),
                                                                      m_dspSettingsRegistry.get(), this)}
    , m_pluginSettingsRegistry{std::make_unique<PluginSettingsRegistry>()}
    , m_scriptCommandHandler{guiPluginContext.scriptCommandHandler}
{
    m_outputProfileManager->reapplyCurrentProfile();
}

Widgets::~Widgets() = default;

void Widgets::registerWidgets()
{
    auto* provider = m_gui->widgetProvider();

    provider->registerWidget(u"Dummy"_s, [this]() { return new Dummy(m_settings, m_window); }, tr("Dummy"));
    provider->setIsHidden(u"Dummy"_s, true);

    provider->registerWidget(
        u"SplitterVertical"_s,
        [this, provider]() { return new VerticalSplitterWidget(provider, m_settings, m_window); },
        tr("Splitter (Top/Bottom)"));
    provider->setSubMenus(u"SplitterVertical"_s, {tr("Splitters")});

    provider->registerWidget(
        u"SplitterHorizontal"_s,
        [this]() { return new HorizontalSplitterWidget(m_gui->widgetProvider(), m_settings, m_window); },
        tr("Splitter (Left/Right)"));
    provider->setSubMenus(u"SplitterHorizontal"_s, {tr("Splitters")});

    provider->registerWidget(
        u"PlaylistSwitcher"_s,
        [this]() { return new PlaylistBox(m_gui->actionManager(), m_playlistController, m_window); },
        tr("Playlist Switcher"));

    provider->registerWidget(
        u"PlaylistTabs"_s,
        [this]() {
            auto* playlistTabs = new PlaylistTabs(m_gui->widgetProvider(), m_playlistController, m_settings, m_window);
            QObject::connect(playlistTabs, &PlaylistTabs::filesDropped, m_playlistInteractor,
                             &PlaylistInteractor::filesToPlaylist);
            QObject::connect(playlistTabs, &PlaylistTabs::tracksDropped, m_playlistInteractor,
                             &PlaylistInteractor::trackIdsToPlaylist);
            QObject::connect(playlistTabs, &PlaylistTabs::trackListDropped, m_playlistInteractor,
                             &PlaylistInteractor::tracksToPlaylist);
            QObject::connect(playlistTabs, &PlaylistTabs::savePlaylistRequested, m_gui, &GuiApplication::savePlaylist);
            return playlistTabs;
        },
        tr("Playlist Tabs"));
    provider->setSubMenus(u"PlaylistTabs"_s, {tr("Splitters")});

    provider->registerWidget(
        u"PlaylistOrganiser"_s,
        [this]() { return new PlaylistOrganiser(m_gui->actionManager(), m_playlistInteractor, m_settings, m_window); },
        tr("Playlist Organiser"));

    provider->registerWidget(
        u"PlaylistManager"_s,
        [this]() {
            return new PlaylistManagerWidget(m_gui->actionManager(), m_playlistController, m_playlistInteractor,
                                             m_settings, m_window);
        },
        tr("Playlist Manager"));

    provider->registerWidget(
        u"PlaybackQueue"_s,
        [this]() {
            return new QueueViewer(m_gui->actionManager(), m_playlistInteractor, m_coverRepository, m_settings,
                                   m_window);
        },
        tr("Playback Queue"));

    provider->registerWidget(
        u"TabStack"_s, [this, provider]() { return new TabStackWidget(provider, m_settings, m_window); },
        tr("Tab Stack"));
    provider->setSubMenus(u"TabStack"_s, {tr("Splitters")});

    provider->registerWidget(
        u"LibraryTree"_s,
        [this]() {
            return new LibraryTreeWidget(m_gui->actionManager(), m_playlistController, m_libraryTreeController, m_core,
                                         m_coverRepository, m_window);
        },
        tr("Library Tree"));

    provider->registerWidget(
        u"CommandButton"_s,
        [this]() {
            return new CommandButton(m_gui->actionManager(), m_core->playerController(), m_scriptCommandHandler,
                                     m_settings, m_window);
        },
        tr("Command Button"));
    provider->setSubMenus(u"CommandButton"_s, {tr("Controls")});

    provider->registerWidget(
        u"PlayerControls"_s,
        [this]() {
            return new PlayerControl(m_gui->actionManager(), m_core->playerController(), m_settings, m_window);
        },
        tr("Player Controls"));
    provider->setSubMenus(u"PlayerControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"PlaylistControls"_s,
        [this]() { return new PlaylistControl(m_core->playerController(), m_settings, m_window); },
        tr("Playlist Controls"));
    provider->setSubMenus(u"PlaylistControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"VolumeControls"_s, [this]() { return new VolumeControl(m_gui->actionManager(), m_settings, m_window); },
        tr("Volume Controls"));
    provider->setSubMenus(u"VolumeControls"_s, {tr("Controls")});

    provider->registerWidget(
        u"SeekBar"_s, [this]() { return new SeekBar(m_core->playerController(), m_settings, m_window); },
        tr("Seekbar"));
    provider->setSubMenus(u"SeekBar"_s, {tr("Controls")});

    provider->registerWidget(
        u"OutputSelector"_s, [this]() { return new OutputSelector(m_outputProfileManager, m_settings, m_window); },
        tr("Output Selector"));
    provider->setSubMenus(u"OutputSelector"_s, {tr("Controls")});

    provider->registerWidget(
        u"SelectionInfo"_s,
        [this]() { return new InfoWidget(m_core, m_gui->actionManager(), m_gui->trackSelection(), m_window); },
        tr("Selection Info"));

    provider->registerWidget(
        u"ArtworkPanel"_s,
        [this]() {
            auto* coverWidget
                = new CoverWidget(m_core->playerController(), m_core->playlistHandler(), m_gui->trackSelection(),
                                  m_core->audioLoader(), m_coverRepository, m_settings, m_window);
            QObject::connect(m_core->library(), &MusicLibrary::tracksMetadataChanged, coverWidget,
                             &CoverWidget::reloadCover);
            QObject::connect(coverWidget, &CoverWidget::requestArtworkSearch, this, &Widgets::showArtworkDialog);
            QObject::connect(coverWidget, &CoverWidget::requestArtworkRemoval, this, &Widgets::removeArtwork);
            return coverWidget;
        },
        tr("Artwork Panel"));

    provider->registerWidget(
        u"Playlist"_s,
        [this]() {
            return PlaylistWidget::createMainPlaylist(m_gui->actionManager(), m_playlistInteractor, m_coverProvider,
                                                      m_core, m_window);
        },
        tr("Playlist"));
    provider->setLimit(u"Playlist"_s, 1);

    provider->registerWidget(u"Spacer"_s, [this]() { return new Spacer(m_window); }, tr("Spacer"));

    provider->registerWidget(
        u"StatusBar"_s,
        [this]() {
            auto* statusWidget
                = new StatusWidget(m_core->engine(), m_core->playerController(), m_core->playlistHandler(),
                                   m_playlistController, m_gui->trackSelection(), m_settings, m_window);
            m_window->installStatusWidget(statusWidget);
            return statusWidget;
        },
        tr("Status Bar"));
    provider->setLimit(u"StatusBar"_s, 1);

    provider->registerWidget(
        u"SearchBar"_s,
        [this]() {
            return new SearchWidget(m_gui->searchController(), m_gui->playlistController(), m_core->library(),
                                    m_settings, m_window);
        },
        tr("Search Bar"));

    provider->registerWidget(u"DirectoryBrowser"_s, [this]() { return createDirBrowser(); }, tr("Directory Browser"));

    provider->registerWidget(
        u"ScriptDisplay"_s,
        [this]() {
            return new ScriptDisplay(m_core->playerController(), m_core->playlistHandler(), m_scriptCommandHandler,
                                     m_gui->actionManager(), m_settings, m_window);
        },
        tr("Script Display"));
}

void Widgets::registerPages()
{
    new GeneralPage(m_settings, this);
    new GuiGeneralPage(m_gui->layoutProvider(), m_gui->editableLayout(), m_settings, this);
    new GuiDisplayPage(m_settings, this);
    new GuiTrackDisplayPage(m_settings, this);
    new GuiLayoutPage(m_gui->layoutProvider(), m_gui->editableLayout(), m_gui->widgetProvider(), m_settings, this);
    new GuiThemesPage(m_gui->themeRegistry(), m_settings, this);
    new TrackContextMenuPage(m_gui->trackSelection(), m_settings, this);

    new StaticContextMenuPage(
        m_settings,
        makeStaticContextMenuDescriptor<Settings::Gui::Internal::ContextMenuLibraryTreeDisabledSections,
                                        Settings::Gui::Internal::ContextMenuLibraryTreeLayout>(
            Constants::Page::InterfaceContextMenuLibraryTree,
            {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Library Tree")},
            {.context    = "LibraryTreeWidget",
             .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget",
                                             "Unchecked items will be hidden from the library tree context menu.")},
            ContextMenuIds::LibraryTree::DefaultItems, m_settings),
        this);
    new StaticContextMenuPage(
        m_settings,
        makeStaticContextMenuDescriptor<Settings::Gui::Internal::ContextMenuPlaylistDisabledSections,
                                        Settings::Gui::Internal::ContextMenuPlaylistLayout>(
            Constants::Page::InterfaceContextMenuPlaylistWidget,
            {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Playlist")},
            {.context = "PlaylistWidget",
             .sourceText
             = QT_TRANSLATE_NOOP("PlaylistWidget", "Unchecked items will be hidden from the playlist context menu.")},
            ContextMenuIds::Playlist::DefaultItems, m_settings),
        this);
    new StaticContextMenuPage(
        m_settings,
        makeStaticContextMenuDescriptor<Settings::Gui::Internal::ContextMenuDirBrowserDisabledSections,
                                        Settings::Gui::Internal::ContextMenuDirBrowserLayout>(
            Constants::Page::InterfaceContextMenuDirBrowser,
            {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Directory Browser")},
            {.context    = "DirBrowser",
             .sourceText = QT_TRANSLATE_NOOP(
                 "DirBrowser", "Unchecked items will be hidden from the directory browser context menu.")},
            ContextMenuIds::DirBrowser::DefaultItems, m_settings),
        this);
    new StaticContextMenuPage(
        m_settings,
        makeStaticContextMenuDescriptor<Settings::Gui::Internal::ContextMenuLayoutEditingDisabledSections,
                                        Settings::Gui::Internal::ContextMenuLayoutEditingLayout>(
            Constants::Page::InterfaceContextMenuLayoutEditing,
            {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Layout Editing")},
            {.context    = "EditableLayout",
             .sourceText = QT_TRANSLATE_NOOP("EditableLayout",
                                             "Unchecked items will be hidden from the layout editing context menu.")},
            ContextMenuIds::LayoutEditing::DefaultItems, m_settings),
        this);

    new ArtworkGeneralPage(m_settings, m_coverRepository, this);
    new ArtworkSearchingPage(m_settings, this);
    new ArtworkSourcesPage(m_artworkFinder, m_settings, this);
    new ArtworkDownloadPage(m_settings, this);
    new LibraryGeneralPage(m_core->libraryManager(), m_core->library(), m_settings, this);
    new LibraryMetadataPage(m_settings, this);
    new LibraryRatingsPage(m_settings, this);
    new LibrarySortingPage(m_core->sortingRegistry(), m_settings, this);
    new PlaybackPage(m_settings, this);
    new NowPlayingOutputPage(m_core->playerController(), m_settings, this);
    new DspManagerPage(m_core->dspChainStore(), m_dspPresetRegistry, m_dspSettingsRegistry.get(), m_settings, this);
    new FadingPage(m_settings, this);
    new PlaylistGeneralPage(m_settings, this);
    new PlaylistGuiPage(m_settings, this);
    new PlaylistTabsPage(m_settings, this);
    new PlaylistSavingPage(m_core->playlistLoader()->supportedSaveExtensions(), m_settings, this);
    new PlaylistColumnPage(m_playlistController->columnRegistry(), m_settings, this);
    new PlaylistPresetsPage(m_playlistController->presetRegistry(), m_settings, this);
    new PluginPage(m_core->pluginManager(), m_pluginSettingsRegistry.get(), m_settings, this);
    new ShellIntegrationPage(m_settings, this);
    new ShortcutsPage(m_gui->actionManager(), m_settings, this);
    new OutputPage(m_core->engine(), m_settings, this);
    new DevicePage(m_outputProfileManager, m_dspPresetRegistry, m_settings, this);
    new DecoderPage(m_core->audioLoader().get(), m_core->pluginManager(), m_pluginSettingsRegistry.get(), m_settings,
                    this);
    new ReplayGainPage(m_settings, this);
    new NetworkPage(m_settings, this);
    new SearchPage(m_settings, this);
    new StatusWidgetPage(m_settings, this);
    new AdvancedPage(m_gui->advancedSettingsRegistry(), m_settings->settingsDialog(), this);
}

void Widgets::registerAdvancedSettings()
{
    auto* advancedSettingsRegistry = m_gui->advancedSettingsRegistry();

    advancedSettingsRegistry->add<Settings::Gui::Internal::ImageAllocationLimit>(
        {.category    = {tr("Interface"), GuiApplication::tr("Display")},
         .label       = tr("Image allocation limit"),
         .description = tr("Maximum image allocation size in MB. Set to 0 to disable the limit."),
         .editor      = AdvancedSettingSpinBox{.minimum          = 0,
                                               .maximum          = 1024,
                                               .singleStep       = 1,
                                               .suffix           = u" MB"_s,
                                               .specialValueText = {}},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add<Settings::Gui::Internal::EditingMenuLevels>(
        {.category    = {tr("Interface"), tr("Layout Editing")},
         .label       = tr("Menu levels"),
         .description = tr("Number of widget levels shown in the layout editing context menu."),
         .editor
         = AdvancedSettingSpinBox{.minimum = 1, .maximum = 4, .singleStep = 1, .suffix = {}, .specialValueText = {}},
         .normalise = {},
         .validate  = {}});
    advancedSettingsRegistry->add<Settings::Gui::SeekBarMouseFocus>(
        {.category    = {tr("Interface"), tr("Seeking")},
         .label       = tr("Focus seekbars when clicked"),
         .description = tr("Give seekbars keyboard focus after clicking them."),
         .editor      = AdvancedSettingCheckBox{},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add<Settings::Core::Internal::VBRUpdateInterval>(
        {.category    = {tr("Playback"), tr("Decoding")},
         .label       = tr("VBR update interval"),
         .description = tr("Interval used to refresh VBR playback information. Set to 0 to disable."),
         .editor      = AdvancedSettingSpinBox{.minimum          = 0,
                                               .maximum          = 300000,
                                               .singleStep       = 100,
                                               .suffix           = u" ms"_s,
                                               .specialValueText = {}},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add<Settings::Core::Internal::RemoteReadAheadKb>(
        {.category    = {tr("Playback"), tr("Buffering")},
         .label       = tr("Read-ahead for remote streams"),
         .description = tr("Maximum network data buffered for remote streams. Changes apply to newly opened "
                           "streams."),
         .editor      = AdvancedSettingSpinBox{.minimum          = 0,
                                               .maximum          = 1048576,
                                               .singleStep       = 256,
                                               .suffix           = u" kB"_s,
                                               .specialValueText = {}},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add<Settings::Gui::Internal::OutputDeviceRefreshMs>(
        {.category    = {tr("Playback"), tr("Output")},
         .label       = tr("Device refresh interval"),
         .description = tr("Interval used to refresh the list of available output devices. Set to 0 to disable."),
         .editor      = AdvancedSettingSpinBox{.minimum          = 0,
                                               .maximum          = 60000,
                                               .singleStep       = 500,
                                               .suffix           = u" ms"_s,
                                               .specialValueText = tr("Disabled")},
         .normalise =
             [](const QVariant& value) {
                 const int interval = value.toInt();
                 return interval <= 0 ? 0 : std::max(interval, 1000);
             },
         .validate = {}});
    advancedSettingsRegistry->add<Settings::Core::PreserveTimestamps>(
        {.category    = {tr("Tagging")},
         .label       = tr("Preserve timestamps"),
         .description = tr("Preserve file access and modification timestamps when updating tags."),
         .editor      = AdvancedSettingCheckBox{},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add(
        Settings::Core::Internal::SplitId3v23SemicolonSeparatedTags, true,
        {.category    = {tr("Tagging"), u"ID3"_s},
         .label       = tr("Split ID3v2.3 semicolon-separated tags"),
         .description = tr("Split ID3v2.3 values with non-standard \";\" separators when reading tags."),
         .editor      = AdvancedSettingCheckBox{},
         .normalise   = {},
         .validate    = {}});
    advancedSettingsRegistry->add(
        {.id           = QString::fromLatin1(Settings::Core::Internal::FFmpegAllExtensions),
         .category     = {tr("Playback"), tr("Decoding"), u"FFmpeg"_s},
         .label        = tr("Enable all supported extensions"),
         .description  = tr("Enabled all extensions supported by the FFmpeg input."),
         .defaultValue = false,
         .editor       = AdvancedSettingCheckBox{},
         .read = [this] { return m_settings->fileValue(Settings::Core::Internal::FFmpegAllExtensions).toBool(); },
         .write =
             [this](const QVariant& value) {
                 if(m_settings->fileSet(Settings::Core::Internal::FFmpegAllExtensions, value.toBool())) {
                     m_core->audioLoader()->reloadDecoderExtensions(u"FFmpeg"_s);
                     m_core->audioLoader()->reloadReaderExtensions(u"FFmpeg"_s);
                 }
                 return true;
             },
         .normalise = {},
         .validate  = {}});
    advancedSettingsRegistry->add(
        Settings::Core::Internal::FFmpegPriorityExtensions, Settings::Core::Internal::defaultFFmpegPriorityExtensions(),
        {.category    = {tr("Playback"), tr("Decoding"), u"FFmpeg"_s},
         .label       = tr("Prefer FFmpeg for extensions"),
         .description = tr("Semicolon-separated extensions where FFmpeg is tried first."),
         .editor      = AdvancedSettingStringListLineEdit{.separator = u';'},
         .normalise   = [](const QVariant& value) { return normaliseExtensionList(value.toStringList()); },
         .validate    = {}});
    advancedSettingsRegistry->add<Settings::Core::Internal::OpusHeaderWriteMode>(
        {.category    = {tr("Playback"), tr("ReplayGain")},
         .label       = tr("Opus header gain"),
         .description = tr("ReplayGain value written to the Opus header when updating metadata."),
         .editor      = AdvancedSettingRadioButtons{.options = {{.value = static_cast<int>(OpusRGWriteMode::Track),
                                                                 .label = tr("Use Track Gain")},
                                                                {.value = static_cast<int>(OpusRGWriteMode::Album),
                                                                 .label = tr("Use Album Gain")},
                                                                {.value = static_cast<int>(OpusRGWriteMode::LeaveNull),
                                                                 .label = tr("Leave null")}}},
         .normalise   = {},
         .validate    = {}});
}

void Widgets::registerDspSettings()
{
    m_dspSettingsRegistry->registerProvider(std::make_unique<SkipSilenceSettingsProvider>());
    m_dspSettingsRegistry->registerProvider(std::make_unique<ResamplerSettingsProvider>());
}

void Widgets::registerDspWidgets()
{
    auto* widgetProvider = m_gui->widgetProvider();

    const auto providers = m_dspSettingsRegistry->providers();
    for(auto* settingsProvider : providers) {
        if(!settingsProvider || !settingsProvider->showAsLayoutWidget()) {
            continue;
        }

        const QString dspId = settingsProvider->id();
        const QString key   = DspSettingsRegistry::layoutWidgetKey(dspId);
        if(widgetProvider->widgetExists(key)) {
            continue;
        }

        widgetProvider->registerWidget(
            key, [this, dspId]() { return m_dspSettingsController->createLayoutWidget(dspId); },
            settingsProvider->displayName());
        widgetProvider->setSubMenus(key, {tr("DSP")});
        widgetProvider->setIsVisibleWhen(key, [this, dspId]() { return m_dspSettingsController->hasDsp(dspId); });
    }
}

void Widgets::registerPropertiesTabs()
{
    m_gui->propertiesDialog()->addTab(tr("Details"), [this](const TrackList& tracks) {
        return new InfoPropertiesTab(tracks, m_core->libraryManager(), m_gui->actionManager(), m_window);
    });
    m_gui->propertiesDialog()->addTab(tr("ReplayGain"), [this](const TrackList& tracks) {
        const bool canWrite = std::ranges::all_of(tracks, [this](const Track& track) {
            return !track.hasCue() && !track.isInArchive() && m_core->audioLoader()->canWriteMetadata(track);
        });
        return new ReplayGainWidget(m_core->library(), tracks, !canWrite, m_settings, m_window);
    });
    m_gui->propertiesDialog()->addTab(tr("Artwork"), [this](const TrackList& tracks) {
        const bool canWrite = std::ranges::all_of(tracks, [this](const Track& track) {
            return !track.hasCue() && !track.isInArchive() && m_core->audioLoader()->canWriteMetadata(track);
        });
        return new ArtworkProperties(m_core->audioLoader().get(), m_core->library(), m_coverRepository, m_settings,
                                     tracks, !canWrite, m_window);
    });
}

void Widgets::registerFontEntries() const
{
    auto* themeReg = m_gui->themeRegistry();

    themeReg->registerFontEntry(tr("Default"), {});
    themeReg->registerFontEntry(tr("Lists"), u"QAbstractItemView"_s);
    themeReg->registerFontEntry(tr("Library Tree"), u"Fooyin::LibraryTreeView"_s);
    themeReg->registerFontEntry(tr("Playlist"), u"Fooyin::PlaylistView"_s);
    themeReg->registerFontEntry(tr("Status bar"), u"Fooyin::StatusLabel"_s);
    themeReg->registerFontEntry(tr("Tabs"), u"Fooyin::EditableTabBar"_s);
}

DspSettingsRegistry* Widgets::dspSettingsRegistry() const
{
    return m_dspSettingsRegistry.get();
}

DspSettingsController* Widgets::dspSettingsController() const
{
    return m_dspSettingsController.get();
}

PluginSettingsRegistry* Widgets::pluginSettingsRegistry() const
{
    return m_pluginSettingsRegistry.get();
}

void Widgets::showArtworkDialog(const TrackList& tracks, Track::Cover type, bool quick)
{
    m_gui->searchForArtwork(tracks, type, quick);
}

void Widgets::removeArtwork(const TrackList& tracks, Track::Cover type)
{
    m_gui->removeArtwork(tracks, {type});
}

void Widgets::refreshCoverWidgets()
{
    const auto coverWidgets = m_gui->editableLayout()->findWidgetsByType<CoverWidget>();
    for(auto* coverWidget : coverWidgets) {
        coverWidget->reloadCover();
    }
}

FyWidget* Widgets::createDirBrowser()
{
    auto* browser = new DirBrowser(m_core->audioLoader()->supportedFileExtensions(), m_gui->actionManager(),
                                   m_playlistInteractor, m_settings, m_window);

    browser->playstateChanged(m_core->playerController()->playState());
    browser->activePlaylistChanged(m_core->playlistHandler()->activePlaylist());
    browser->playlistTrackChanged(m_core->playerController()->currentPlaylistTrack());

    QObject::connect(m_core->playerController(), &PlayerController::playStateChanged, browser,
                     &DirBrowser::playstateChanged);
    QObject::connect(m_core->playerController(), &PlayerController::playlistTrackChanged, browser,
                     &DirBrowser::playlistTrackChanged);
    QObject::connect(m_core->playlistHandler(), &PlaylistHandler::activePlaylistChanged, browser,
                     &DirBrowser::activePlaylistChanged);

    return browser;
}

} // namespace Fooyin
