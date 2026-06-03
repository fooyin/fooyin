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

#include "lyricsplugin.h"

#include "lyricseditordialog.h"
#include "lyricsfinder.h"
#include "lyricspropertiestab.h"
#include "lyricssaver.h"
#include "lyricssearchdialog.h"
#include "lyricswidget.h"
#include "settings/lyricssavingpage.h"
#include "settings/lyricssearchingpage.h"
#include "settings/lyricssettings.h"
#include "settings/lyricssourcespage.h"

#include <core/engine/audioloader.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/propertiesdialog.h>
#include <gui/theme/themeregistry.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
using namespace Qt::StringLiterals;

constexpr auto LyricsMenuId = "Fooyin.Menu.Lyrics";

namespace Fooyin::Lyrics {
void LyricsPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_audioLoader      = context.audioLoader;
    m_networkAccess    = context.networkAccess;
    m_settings         = context.settingsManager;

    m_lyricsSettings = std::make_unique<LyricsSettings>(m_settings);
    m_lyricsFinder   = new LyricsFinder(context.networkAccess, m_settings, this);
    m_lyricsSaver    = new LyricsSaver(context.library, m_settings, this);

    m_lyricsFinder->restoreState();
}

void LyricsPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_trackSelection = context.trackSelection;
    m_widgetProvider = context.widgetProvider;

    context.propertiesDialog->addTab(tr("Lyrics"), [this](const TrackList& tracks) {
        return new LyricsPropertiesTab(tracks.empty() ? Track{} : tracks.front(), m_networkAccess, m_lyricsSaver,
                                       m_playerController, m_settings,
                                       [this](const Track& track) { return m_audioLoader->canWriteMetadata(track); });
    });

    m_widgetProvider->registerWidget(
        u"Lyrics"_s,
        [this]() {
            return new LyricsWidget(m_playerController, m_playlistHandler, m_lyricsFinder, m_lyricsSaver, m_settings);
        },
        tr("Lyrics"));
    context.themeRegistry->registerFontEntry(tr("Lyrics"), u"Fooyin::Lyrics::LyricsArea"_s);

    new LyricsSearchingPage(m_settings, this);
    new LyricsSourcesPage(m_lyricsFinder, m_settings, this);
    new LyricsSavingPage(m_settings, this);

    m_trackSelection->registerTrackContextSubmenu(this, TrackContextMenuArea::Track,
                                                  Constants::Menus::Context::TrackSelection, LyricsMenuId, tr("Lyrics"),
                                                  Constants::Menus::Context::Utilities);

    auto* editLyrics    = new QAction(tr("Edit lyrics"), this);
    auto* editLyricsCmd = m_actionManager->registerAction(editLyrics, "Lyrics.Edit");
    editLyricsCmd->setCategories({tr("Lyrics")});
    QObject::connect(editLyrics, &QAction::triggered, this, [this]() {
        if(const Track track = selectedTrack(); track.isValid() && !track.isInArchive()) {
            loadTrackLyricsAndOpenDialog(track);
        }
    });

    auto* searchLyrics    = new QAction(tr("Quicksearch for lyrics"), this);
    auto* searchLyricsCmd = m_actionManager->registerAction(searchLyrics, "Lyrics.SearchQuick");
    searchLyricsCmd->setCategories({tr("Lyrics")});
    QObject::connect(searchLyrics, &QAction::triggered, this, [this]() {
        if(const Track track = selectedTrack(); track.isValid()) {
            quickSearchLyrics(track);
        }
    });

    auto* searchLyricsManual    = new QAction(tr("Search for lyrics…"), this);
    auto* searchLyricsManualCmd = m_actionManager->registerAction(searchLyricsManual, "Lyrics.Search");
    searchLyricsManualCmd->setCategories({tr("Lyrics")});
    QObject::connect(searchLyricsManual, &QAction::triggered, this, [this]() {
        if(const Track track = selectedTrack(); track.isValid()) {
            openSearchDialog(track);
        }
    });

    const auto renderTrackAction
        = [](QAction* action, QMenu* menu, const TrackSelection& selection, bool allowArchive = false) {
              if(selection.tracks.size() != 1) {
                  return;
              }
              action->setEnabled(allowArchive || !selection.tracks.front().isInArchive());
              menu->addAction(action);
          };

    m_trackSelection->registerTrackContextAction(
        this, TrackContextMenuArea::Track, LyricsMenuId, "Lyrics.Edit", editLyrics->text(),
        [editLyrics, renderTrackAction](QMenu* menu, const TrackSelection& selection) {
            renderTrackAction(editLyrics, menu, selection);
        });
    m_trackSelection->registerTrackContextAction(
        this, TrackContextMenuArea::Track, LyricsMenuId, "Lyrics.SearchQuick", searchLyrics->text(),
        [searchLyrics, renderTrackAction](QMenu* menu, const TrackSelection& selection) {
            renderTrackAction(searchLyrics, menu, selection);
        });
    m_trackSelection->registerTrackContextAction(
        this, TrackContextMenuArea::Track, LyricsMenuId, "Lyrics.Search", searchLyricsManual->text(),
        [searchLyricsManual, renderTrackAction](QMenu* menu, const TrackSelection& selection) {
            renderTrackAction(searchLyricsManual, menu, selection);
        });
}

void LyricsPlugin::shutdown()
{
    m_lyricsFinder->saveState();
}

Track LyricsPlugin::selectedTrack() const
{
    const auto* selection = m_trackSelection->selectedSelection();
    if(!selection || selection->tracks.size() != 1) {
        return {};
    }

    return selection->tracks.front();
}

void LyricsPlugin::openSearchDialog(const Track& track) const
{
    if(!track.isValid() || track.isInArchive()) {
        return;
    }

    auto* dialog = new LyricsSearchDialog(track, m_networkAccess, m_lyricsSaver, m_settings, Utils::getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void LyricsPlugin::quickSearchLyrics(const Track& track)
{
    if(!track.isValid() || track.isInArchive()) {
        return;
    }

    auto* finder = new LyricsFinder(m_networkAccess, m_settings, this);

    QObject::connect(
        finder, &LyricsFinder::lyricsFound, this,
        [this, finder, track](const Track& foundTrack, const Lyrics& lyrics) {
            if(!foundTrack.sameIdentityAs(track)) {
                return;
            }

            QObject::disconnect(finder, &LyricsFinder::lyricsSearchFinished, this, nullptr);

            if(m_lyricsSaver) {
                m_lyricsSaver->saveLyrics(lyrics, track);
            }

            finder->deleteLater();
        },
        Qt::SingleShotConnection);

    QObject::connect(finder, &LyricsFinder::lyricsSearchFinished, this,
                     [finder](const Track&, const bool) { finder->deleteLater(); });

    finder->findLyrics(track);
}

void LyricsPlugin::openLyricsDialog(const Track& track, const Lyrics& lyrics)
{
    auto* dialog = new LyricsEditorDialog(track, lyrics, m_playerController, m_lyricsSaver, Utils::getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dialog, &QDialog::finished, dialog, &LyricsEditorDialog::saveState);
    dialog->show();
    dialog->restoreState();
}

void LyricsPlugin::loadTrackLyricsAndOpenDialog(const Track& track)
{
    if(!track.isValid()) {
        return;
    }

    auto* finder = new LyricsFinder(m_networkAccess, m_settings, this);

    QObject::connect(
        finder, &LyricsFinder::lyricsFound, this,
        [this, finder, track](const Track& foundTrack, const Lyrics& lyrics) {
            if(!foundTrack.sameIdentityAs(track)) {
                return;
            }

            QObject::disconnect(finder, &LyricsFinder::lyricsSearchFinished, this, nullptr);
            finder->deleteLater();
            openLyricsDialog(track, lyrics);
        },
        Qt::SingleShotConnection);

    QObject::connect(finder, &LyricsFinder::lyricsSearchFinished, this, [this, finder, track](const Track&) {
        finder->deleteLater();
        openLyricsDialog(track, {});
    });

    finder->findLocalLyrics(track);
}
} // namespace Fooyin::Lyrics

#include "moc_lyricsplugin.cpp"
