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

#include "lyricswidget.h"

#include "lyricscolours.h"
#include "lyricsconfigwidget.h"
#include "lyricsconstants.h"
#include "lyricsdelegate.h"
#include "lyricseditordialog.h"
#include "lyricsfinder.h"
#include "lyricsmodel.h"
#include "lyricssaver.h"
#include "lyricssearchdialog.h"
#include "lyricsview.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <gui/configdialog.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/scripting/scriptformatter.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMenu>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTimerEvent>

#include <algorithm>
#include <utility>

Q_LOGGING_CATEGORY(LYRICS_WIDGET, "fy.lyrics")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto ScrollTimeout = 100ms;
#else
constexpr auto ScrollTimeout = 100;
#endif

namespace {
bool hasSameTagLyrics(const Fooyin::Track& lhs, const Fooyin::Track& rhs, const Fooyin::SettingsManager* settings)
{
    const QStringList searchTags
        = settings->fileValue(Fooyin::Lyrics::Settings::SearchTags, Fooyin::Lyrics::Defaults::searchTags())
              .toStringList();
    return std::ranges::all_of(searchTags,
                               [&lhs, &rhs](const QString& tag) { return lhs.extraTag(tag) == rhs.extraTag(tag); });
}

QMargins clampedMargins(const QMargins& margins)
{
    return {
        std::clamp(margins.left(), 0, 100),
        std::clamp(margins.top(), 0, 100),
        std::clamp(margins.right(), 0, 100),
        std::clamp(margins.bottom(), 0, 100),
    };
}

int validatedAlignment(int alignment)
{
    switch(alignment) {
        case Qt::AlignLeft:
        case Qt::AlignRight:
        case Qt::AlignCenter:
            return alignment;
        default:
            return static_cast<int>(Qt::AlignCenter);
    }
}

QString validatedFontString(const QString& fontString)
{
    if(fontString.isEmpty()) {
        return {};
    }

    QFont font;
    return font.fromString(fontString) ? fontString : QString{};
}

int validatedEdgeFadeSize(int edgeFadeSize)
{
    return std::clamp(edgeFadeSize, 1, 50);
}

int validatedEdgeFadeMode(int edgeFadeMode)
{
    return std::clamp(edgeFadeMode, static_cast<int>(Fooyin::Lyrics::EdgeFadeMode::Off),
                      static_cast<int>(Fooyin::Lyrics::EdgeFadeMode::AllLyrics));
}
} // namespace

namespace Fooyin::Lyrics {
LyricsWidget::LyricsWidget(PlayerController* playerController, PlaylistHandler* playlistHandler,
                           LyricsFinder* lyricsFinder, LyricsSaver* lyricsSaver, SettingsManager* settings,
                           QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_settings{settings}
    , m_lyricsView{new LyricsView(this)}
    , m_model{new LyricsModel(this)}
    , m_delegate{new LyricsDelegate(this)}
    , m_lyricsFinder{lyricsFinder}
    , m_lyricsSaver{lyricsSaver}
    , m_currentTime{0}
    , m_currentLineStart{-1}
    , m_currentLineEnd{-1}
    , m_scrollMode{ScrollMode::Synced}
    , m_isUserScrolling{false}
{
    setObjectName(LyricsWidget::name());

    m_lyricsView->setModel(m_model);
    m_lyricsView->setItemDelegate(m_delegate);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_lyricsView);

    m_lyricsView->setMinimumWidth(150);

    m_config = defaultConfig();
    applyConfig(m_config);

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &LyricsWidget::playStateChanged);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { updateLyrics(track, false); });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                     [this](const Track& track) { updateLyrics(track, true); });
    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &LyricsWidget::setCurrentTime);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     qOverload<uint64_t>(&LyricsWidget::checkStartAutoScrollPos));
    QObject::connect(m_lyricsView, &LyricsView::viewportResized, this, &LyricsWidget::updateViewportPadding);
    QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsSearchFinished, this,
                     &LyricsWidget::handleLyricsSearchFinished);
    QObject::connect(m_lyricsSaver, &LyricsSaver::lyricsSaved, this, &LyricsWidget::handleSavedLyrics);

    QObject::connect(m_lyricsView, &LyricsView::lineClicked, this, &LyricsWidget::seekTo);
    QObject::connect(m_lyricsView, &LyricsView::lineDragSeekRequested, this, &LyricsWidget::seekTo);
    QObject::connect(m_lyricsView, &LyricsView::dragSeekingChanged, this, [this](bool active) {
        m_isUserScrolling = active;

        if(m_scrollAnim) {
            m_scrollAnim->stop();
        }

        if(active) {
            m_scrollTimer.stop();
        }
        else {
            m_scrollTimer.start(ScrollTimeout, this);
        }
    });
    QObject::connect(m_lyricsView->verticalScrollBar(), &QScrollBar::rangeChanged, this,
                     [this]() { checkStartAutoScroll(0); });

    QObject::connect(m_lyricsView->verticalScrollBar(), &QScrollBar::sliderPressed, this,
                     [this]() { m_isUserScrolling = true; });
    QObject::connect(m_lyricsView->verticalScrollBar(), &QScrollBar::sliderReleased, this,
                     [this]() { m_isUserScrolling = false; });
    QObject::connect(m_lyricsView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        if(m_isUserScrolling && m_scrollAnim) {
            m_scrollAnim->stop();
        }
    });
    QObject::connect(m_lyricsView, &LyricsView::userScrolling, this, [this]() {
        m_isUserScrolling = true;
        if(m_scrollAnim) {
            m_scrollAnim->stop();
        }
        if(m_lyricsView->isDragSeeking()) {
            m_scrollTimer.stop();
            return;
        }
        m_scrollTimer.start(ScrollTimeout, this);
    });

    const auto updateThemeDefaults = [this]() {
        applyConfig(m_config);
    };
    m_settings->subscribe<::Fooyin::Settings::Gui::Theme>(this, updateThemeDefaults);
    m_settings->subscribe<::Fooyin::Settings::Gui::Style>(this, updateThemeDefaults);

    updateLyrics(m_playerController->currentTrack());
}

QString LyricsWidget::defaultNoLyricsScript()
{
    return u"[%1: %artist%$crlf(2)][%2: %album%$crlf(2)]%3: %title%"_s.arg(tr("Artist"), tr("Album"), tr("Title"));
}

void LyricsWidget::updateLyrics(const Track& track, bool force)
{
    const Track previousTrack{m_currentTrack};

    const bool sameTrack = previousTrack.sameIdentityAs(track);
    const bool preserveLyrics
        = sameTrack && force && m_currentLyrics.isValid() && hasSameTagLyrics(previousTrack, track, m_settings);

    m_currentTrack = track;

    if(sameTrack && !force) {
        return;
    }

    if(preserveLyrics) {
        return;
    }

    QObject::disconnect(m_finderConnection);

    m_lyrics.clear();

    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }
    m_lyricsView->verticalScrollBar()->setValue(0);
    m_lyricsView->setEdgeFadeEnabled(false);

    m_currentLyrics    = {};
    m_currentLineStart = -1;
    m_currentLineEnd   = -1;
    m_model->setLyrics({});

    if(!track.isValid()) {
        return;
    }

    m_lyricsView->setDisplayString(noLyricsDisplayText(track));

    m_finderConnection = QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this,
                                          [this](const Track& /*track*/, const Lyrics& lyrics) { loadLyrics(lyrics); });

    if(!sameTrack && m_settings->fileValue(Settings::AutoSearch, false).toBool()) {
        m_lyricsFinder->findLyrics(track);
    }
    else {
        m_lyricsFinder->findLocalLyrics(track);
    }
}

QString LyricsWidget::name() const
{
    return tr("Lyrics");
}

QString LyricsWidget::layoutName() const
{
    return u"Lyrics"_s;
}

void LyricsWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void LyricsWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

LyricsWidget::ConfigData LyricsWidget::factoryConfig() const
{
    return {};
}

LyricsWidget::ConfigData LyricsWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.seekOnClick    = m_settings->fileValue(Settings::SeekOnClick, config.seekOnClick).toBool();
    config.noLyricsScript = m_settings->fileValue(Settings::NoLyricsScript, config.noLyricsScript).toString();
    config.scrollDuration = m_settings->fileValue(Settings::ScrollDuration, config.scrollDuration).toInt();
    config.scrollMode     = m_settings->fileValue(Settings::ScrollMode, config.scrollMode).toInt();

    config.edgeFadeMode = m_settings->fileValue(Settings::EdgeFadeMode, config.edgeFadeMode).toInt();
    config.edgeFadeSize = m_settings->fileValue(Settings::EdgeFadeSize, config.edgeFadeSize).toInt();

    config.showScrollbar = m_settings->fileValue(Settings::ShowScrollbar, config.showScrollbar).toBool();
    config.alignment     = m_settings->fileValue(Settings::Alignment, config.alignment).toInt();
    config.lineSpacing   = m_settings->fileValue(Settings::LineSpacing, config.lineSpacing).toInt();
    config.centreFirstSyncedLine
        = m_settings->fileValue(Settings::CentreFirstLine, config.centreFirstSyncedLine).toBool();
    config.centreLastSyncedLine = m_settings->fileValue(Settings::CentreLastLine, config.centreLastSyncedLine).toBool();

    const QVariant margins = m_settings->fileValue(Settings::Margins);
    if(margins.isValid() && margins.canConvert<QMargins>()) {
        config.margins = margins.value<QMargins>();
    }

    config.colours      = m_settings->fileValue(Settings::Colours, config.colours);
    config.baseFont     = m_settings->fileValue(Settings::BaseFont, config.baseFont).toString();
    config.lineFont     = m_settings->fileValue(Settings::LineFont, config.lineFont).toString();
    config.wordLineFont = m_settings->fileValue(Settings::WordLineFont, config.wordLineFont).toString();
    config.wordFont     = m_settings->fileValue(Settings::WordFont, config.wordFont).toString();

    return config;
}

const LyricsWidget::ConfigData& LyricsWidget::currentConfig() const
{
    return m_config;
}

void LyricsWidget::saveDefaults(const ConfigData& config) const
{
    auto validated{config};

    validated.scrollDuration = std::clamp(validated.scrollDuration, 0, 2000);
    validated.scrollMode     = std::clamp(validated.scrollMode, static_cast<int>(ScrollMode::Manual),
                                          static_cast<int>(ScrollMode::Automatic));
    validated.edgeFadeMode   = validatedEdgeFadeMode(validated.edgeFadeMode);
    validated.edgeFadeSize   = validatedEdgeFadeSize(validated.edgeFadeSize);
    validated.alignment      = validatedAlignment(validated.alignment);
    validated.lineSpacing    = std::clamp(validated.lineSpacing, 0, 100);
    validated.margins        = clampedMargins(validated.margins);

    if(!validated.colours.canConvert<Colours>()) {
        validated.colours = QVariant{};
    }

    validated.baseFont     = validatedFontString(validated.baseFont);
    validated.lineFont     = validatedFontString(validated.lineFont);
    validated.wordLineFont = validatedFontString(validated.wordLineFont);
    validated.wordFont     = validatedFontString(validated.wordFont);

    m_settings->fileSet(Settings::SeekOnClick, validated.seekOnClick);
    m_settings->fileSet(Settings::NoLyricsScript, validated.noLyricsScript);
    m_settings->fileSet(Settings::ScrollDuration, validated.scrollDuration);
    m_settings->fileSet(Settings::ScrollMode, validated.scrollMode);
    m_settings->fileSet(Settings::EdgeFadeMode, validated.edgeFadeMode);
    m_settings->fileSet(Settings::EdgeFadeSize, validated.edgeFadeSize);
    m_settings->fileSet(Settings::ShowScrollbar, validated.showScrollbar);
    m_settings->fileSet(Settings::Alignment, validated.alignment);
    m_settings->fileSet(Settings::LineSpacing, validated.lineSpacing);
    m_settings->fileSet(Settings::CentreFirstLine, validated.centreFirstSyncedLine);
    m_settings->fileSet(Settings::CentreLastLine, validated.centreLastSyncedLine);
    m_settings->fileSet(Settings::Margins, QVariant::fromValue(validated.margins));
    m_settings->fileSet(Settings::Colours, validated.colours);
    m_settings->fileSet(Settings::BaseFont, validated.baseFont);
    m_settings->fileSet(Settings::LineFont, validated.lineFont);
    m_settings->fileSet(Settings::WordLineFont, validated.wordLineFont);
    m_settings->fileSet(Settings::WordFont, validated.wordFont);
}

void LyricsWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(Settings::SeekOnClick);
    m_settings->fileRemove(Settings::NoLyricsScript);
    m_settings->fileRemove(Settings::ScrollDuration);
    m_settings->fileRemove(Settings::ScrollMode);
    m_settings->fileRemove(Settings::EdgeFadeMode);
    m_settings->fileRemove(Settings::EdgeFadeSize);
    m_settings->fileRemove(Settings::ShowScrollbar);
    m_settings->fileRemove(Settings::Alignment);
    m_settings->fileRemove(Settings::LineSpacing);
    m_settings->fileRemove(Settings::CentreFirstLine);
    m_settings->fileRemove(Settings::CentreLastLine);
    m_settings->fileRemove(Settings::Margins);
    m_settings->fileRemove(Settings::Colours);
    m_settings->fileRemove(Settings::BaseFont);
    m_settings->fileRemove(Settings::LineFont);
    m_settings->fileRemove(Settings::WordLineFont);
    m_settings->fileRemove(Settings::WordFont);
}

void LyricsWidget::applyConfig(const ConfigData& config)
{
    auto validated           = config;
    validated.scrollDuration = std::clamp(validated.scrollDuration, 0, 2000);
    validated.scrollMode     = std::clamp(validated.scrollMode, static_cast<int>(ScrollMode::Manual),
                                          static_cast<int>(ScrollMode::Automatic));
    validated.edgeFadeMode   = validatedEdgeFadeMode(validated.edgeFadeMode);
    validated.edgeFadeSize   = validatedEdgeFadeSize(validated.edgeFadeSize);
    validated.alignment      = validatedAlignment(validated.alignment);
    validated.lineSpacing    = std::clamp(validated.lineSpacing, 0, 100);
    validated.margins        = clampedMargins(validated.margins);
    if(!validated.colours.canConvert<Colours>()) {
        validated.colours = QVariant{};
    }
    validated.baseFont     = validatedFontString(validated.baseFont);
    validated.lineFont     = validatedFontString(validated.lineFont);
    validated.wordLineFont = validatedFontString(validated.wordLineFont);
    validated.wordFont     = validatedFontString(validated.wordFont);

    m_config = validated;

    m_lyricsView->setVerticalScrollBarPolicy(m_config.showScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_lyricsView->setDisplayAlignment(static_cast<Qt::Alignment>(m_config.alignment));
    m_lyricsView->setEdgeFadeSizePercent(m_config.edgeFadeSize);
    updateEdgeFadeState();
    m_lyricsView->setDisplayMargins(m_config.margins);

    m_model->setLineSpacing(m_config.lineSpacing);
    m_model->setAlignment(static_cast<Qt::Alignment>(m_config.alignment));
    m_model->setColours(m_config.colours.isValid() ? m_config.colours.value<Colours>() : Colours{});
    m_model->setFonts(m_config.baseFont, m_config.lineFont, m_config.wordLineFont, m_config.wordFont);

    updateViewportPadding();

    updateScrollMode(static_cast<ScrollMode>(m_config.scrollMode));

    if(!m_currentLyrics.isValid() && m_currentTrack.isValid()) {
        m_lyricsView->setDisplayString(noLyricsDisplayText(m_currentTrack));
    }
}

void LyricsWidget::changeEvent(QEvent* event)
{
    FyWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            applyConfig(m_config);
            break;
        default:
            break;
    }
}

void LyricsWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_scrollTimer.timerId()) {
        m_scrollTimer.stop();
        m_isUserScrolling = false;
        checkStartAutoScroll(m_lyricsView->verticalScrollBar()->value());
    }
    FyWidget::timerEvent(event);
}

void LyricsWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(!m_lyrics.empty()) {
        auto* selectLyrics = new QMenu(tr("Select lyrics"), menu);
        for(const auto& lyric : m_lyrics) {
            const auto actionTitle = u"%1 - %2 (%3)"_s.arg(lyric.metadata.artist, lyric.metadata.title, lyric.source);
            auto* lyricAction      = new QAction(actionTitle, selectLyrics);
            QObject::connect(lyricAction, &QAction::triggered, this, [this, lyric]() { changeLyrics(lyric); });
            selectLyrics->addAction(lyricAction);
        }
        menu->addMenu(selectLyrics);
    }

    auto* searchLyrics = new QAction(tr("Auto-search for lyrics"), menu);
    searchLyrics->setStatusTip(tr("Search for lyrics for the current track"));
    QObject::connect(searchLyrics, &QAction::triggered, this, [this]() {
        QObject::disconnect(m_finderConnection);
        m_finderConnection
            = QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this,
                               [this](const Track& /*track*/, const Lyrics& lyrics) { loadLyrics(lyrics); });
        m_lyrics.clear();
        m_lyricsFinder->findLyrics(m_currentTrack);
    });
    menu->addAction(searchLyrics);

    auto* searchLyricsDialog = new QAction(tr("Search for lyrics…"), menu);
    searchLyricsDialog->setStatusTip(tr("Open the lyrics search dialog for the current track"));
    QObject::connect(searchLyricsDialog, &QAction::triggered, this, &LyricsWidget::openSearchDialog);
    searchLyricsDialog->setEnabled(m_currentTrack.isValid() && !m_currentTrack.isInArchive());
    menu->addAction(searchLyricsDialog);

    auto* editLyrics = new QAction(tr("Edit lyrics"), menu);
    editLyrics->setStatusTip(tr("Open editor for the current lyrics"));
    QObject::connect(editLyrics, &QAction::triggered, this, [this]() { openEditor(m_currentLyrics); });
    editLyrics->setEnabled(!m_currentTrack.isInArchive());
    menu->addAction(editLyrics);

    if(m_currentLyrics.isValid()) {
        auto* saveLyrics = new QAction(tr("Save lyrics"), menu);
        saveLyrics->setStatusTip(tr("Save lyrics using current settings"));
        QObject::connect(saveLyrics, &QAction::triggered, this,
                         [this]() { m_lyricsSaver->saveLyrics(m_currentLyrics, m_currentTrack); });
        saveLyrics->setEnabled(!m_currentTrack.isInArchive());
        menu->addAction(saveLyrics);
    }

    menu->addSeparator();

    auto* showScrollbar = new QAction(tr("Show scrollbar"), menu);
    showScrollbar->setCheckable(true);
    showScrollbar->setChecked(m_config.showScrollbar);
    QObject::connect(showScrollbar, &QAction::triggered, this, [this](const bool checked) {
        auto config          = m_config;
        config.showScrollbar = checked;
        applyConfig(config);
    });

    auto* alignMenu      = new QMenu(tr("Text-align"), menu);
    auto* alignmentGroup = new QActionGroup(menu);

    auto* alignCenter = new QAction(tr("Align to centre"), alignmentGroup);
    auto* alignLeft   = new QAction(tr("Align to left"), alignmentGroup);
    auto* alignRight  = new QAction(tr("Align to right"), alignmentGroup);

    alignCenter->setCheckable(true);
    alignLeft->setCheckable(true);
    alignRight->setCheckable(true);

    const auto currentAlignment = static_cast<Qt::Alignment>(m_config.alignment);
    alignCenter->setChecked(currentAlignment == Qt::AlignCenter);
    alignLeft->setChecked(currentAlignment == Qt::AlignLeft);
    alignRight->setChecked(currentAlignment == Qt::AlignRight);

    auto changeAlignment = [this](Qt::Alignment alignment) {
        auto config      = m_config;
        config.alignment = static_cast<int>(alignment);
        applyConfig(config);
    };

    QObject::connect(alignCenter, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignCenter); });
    QObject::connect(alignLeft, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignLeft); });
    QObject::connect(alignRight, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignRight); });

    alignMenu->addAction(alignCenter);
    alignMenu->addAction(alignLeft);
    alignMenu->addAction(alignRight);

    menu->addAction(showScrollbar);
    menu->addMenu(alignMenu);
    menu->addSeparator();

    auto* lyricsSettings = new QAction(tr("Lyrics settings…"), menu);
    QObject::connect(lyricsSettings, &QAction::triggered, this, [this]() {
        if(m_settings && m_settings->settingsDialog()) {
            m_settings->settingsDialog()->openAtPage(Constants::Page::LyricsSearching);
        }
    });
    menu->addAction(lyricsSettings);

    addConfigureAction(menu, false);

    menu->popup(event->globalPos());
}

void LyricsWidget::openConfigDialog()
{
    showConfigDialog(new LyricsConfigDialog(this, this));
}

void LyricsWidget::loadLyrics(const Lyrics& lyrics)
{
    const bool first = m_lyrics.empty();
    m_lyrics.push_back(lyrics);

    if(first) {
        changeLyrics(lyrics);
    }
}

void LyricsWidget::handleLyricsSearchFinished(const Track& track, bool foundAny)
{
    if(foundAny || !track.sameIdentityAs(m_currentTrack)) {
        return;
    }

    m_currentLyrics    = {};
    m_currentLineStart = -1;
    m_currentLineEnd   = -1;
    m_lyrics.clear();
    m_model->setLyrics({});
    m_lyricsView->setEdgeFadeEnabled(false);

    if(track.isValid()) {
        m_lyricsView->setDisplayString(noLyricsDisplayText(track));
    }
    else {
        m_lyricsView->setDisplayString({});
    }

    updateViewportPadding();
}

void LyricsWidget::handleSavedLyrics(const Track& track, const Lyrics& lyrics)
{
    if(!track.sameIdentityAs(m_currentTrack)) {
        return;
    }

    m_currentTrack = track;

    std::erase_if(m_lyrics, [](const Lyrics& existingLyrics) { return existingLyrics.isLocal; });

    if(lyrics.isValid()) {
        m_lyrics.insert(m_lyrics.begin(), lyrics);
    }

    changeLyrics(lyrics);
}

void LyricsWidget::changeLyrics(const Lyrics& lyrics, const Track* sourceTrack)
{
    if(sourceTrack && !m_currentTrack.sameIdentityAs(*sourceTrack)) {
        return;
    }

    m_currentLyrics = lyrics;

    if(!lyrics.isLocal) {
        m_lyricsSaver->autoSaveLyrics(lyrics, m_currentTrack);
    }

    if(lyrics.isValid()) {
        m_lyricsView->setDisplayString({});
        m_model->setLyrics(m_currentLyrics);
        updateEdgeFadeState();
    }
    else {
        m_lyricsView->setDisplayString(noLyricsDisplayText(m_currentTrack));
        m_model->setLyrics({});
        m_lyricsView->setEdgeFadeEnabled(false);
    }

    updateViewportPadding();
}

void LyricsWidget::openEditor(const Lyrics& lyrics)
{
    auto* dlg
        = new LyricsEditorDialog(m_currentTrack, lyrics, m_playerController, m_lyricsSaver, Utils::getMainWindow());
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dlg, &QDialog::finished, dlg, &LyricsEditorDialog::saveState);
    QObject::connect(dlg, &LyricsEditorDialog::lyricsEdited, this,
                     [this, editingTrack = m_currentTrack](const Lyrics& updatedLyrics) {
                         if(m_currentTrack.sameIdentityAs(editingTrack)) {
                             std::erase_if(m_lyrics,
                                           [](const Lyrics& existingLyrics) { return existingLyrics.isLocal; });
                             if(updatedLyrics.isValid()) {
                                 m_lyrics.insert(m_lyrics.begin(), updatedLyrics);
                             }
                         }
                         changeLyrics(updatedLyrics, &editingTrack);
                     });

    dlg->show();
    dlg->restoreState();
}

void LyricsWidget::openSearchDialog()
{
    if(!m_currentTrack.isValid() || m_currentTrack.isInArchive()) {
        return;
    }

    auto* dialog = new LyricsSearchDialog(m_currentTrack, m_lyricsFinder->networkManager(), m_lyricsSaver, m_settings,
                                          Utils::getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

LyricsWidget::ConfigData LyricsWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("SeekOnClick"_L1)) {
        config.seekOnClick = layout.value("SeekOnClick"_L1).toBool();
    }
    if(layout.contains("NoLyricsScript"_L1)) {
        config.noLyricsScript = layout.value("NoLyricsScript"_L1).toString();
    }
    if(layout.contains("ScrollDuration"_L1)) {
        config.scrollDuration = layout.value("ScrollDuration"_L1).toInt();
    }
    if(layout.contains("ScrollMode"_L1)) {
        config.scrollMode = layout.value("ScrollMode"_L1).toInt();
    }
    if(layout.contains("EdgeFadeMode"_L1)) {
        config.edgeFadeMode = layout.value("EdgeFadeMode"_L1).toInt();
    }
    if(layout.contains("EdgeFadeSize"_L1)) {
        config.edgeFadeSize = layout.value("EdgeFadeSize"_L1).toInt();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollbar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("Alignment"_L1)) {
        config.alignment = layout.value("Alignment"_L1).toInt();
    }
    if(layout.contains("LineSpacing"_L1)) {
        config.lineSpacing = layout.value("LineSpacing"_L1).toInt();
    }
    if(layout.contains("CentreFirstSyncedLine"_L1)) {
        config.centreFirstSyncedLine = layout.value("CentreFirstSyncedLine"_L1).toBool();
    }
    if(layout.contains("CentreLastSyncedLine"_L1)) {
        config.centreLastSyncedLine = layout.value("CentreLastSyncedLine"_L1).toBool();
    }

    QMargins margins{config.margins};
    if(layout.contains("LeftMargin"_L1)) {
        margins.setLeft(layout.value("LeftMargin"_L1).toInt());
    }
    if(layout.contains("TopMargin"_L1)) {
        margins.setTop(layout.value("TopMargin"_L1).toInt());
    }
    if(layout.contains("RightMargin"_L1)) {
        margins.setRight(layout.value("RightMargin"_L1).toInt());
    }
    if(layout.contains("BottomMargin"_L1)) {
        margins.setBottom(layout.value("BottomMargin"_L1).toInt());
    }
    config.margins = margins;

    if(layout.contains("UseCustomColours"_L1)) {
        if(layout.value("UseCustomColours"_L1).toBool()) {
            Colours colours;

            auto setColour = [&layout, &colours](const QString& key, Colours::Type type) {
                if(!layout.contains(key)) {
                    return;
                }

                const QColor colour{layout.value(key).toString()};
                if(colour.isValid()) {
                    colours.setColour(type, colour);
                }
            };

            setColour(u"BackgroundColour"_s, Colours::Type::Background);
            setColour(u"UnsyncedLineColour"_s, Colours::Type::LineUnsynced);
            setColour(u"UnplayedLineColour"_s, Colours::Type::LineUnplayed);
            setColour(u"PlayedLineColour"_s, Colours::Type::LinePlayed);
            setColour(u"CurrentLineColour"_s, Colours::Type::LineSynced);
            setColour(u"CurrentWordLineColour"_s, Colours::Type::WordLineSynced);
            setColour(u"CurrentWordColour"_s, Colours::Type::WordSynced);
            config.colours = QVariant::fromValue(colours);
        }
        else {
            config.colours = QVariant{};
        }
    }

    if(layout.contains("LineFont"_L1)) {
        config.lineFont = layout.value("LineFont"_L1).toString();
    }
    if(layout.contains("BaseFont"_L1)) {
        config.baseFont = layout.value("BaseFont"_L1).toString();
    }
    if(layout.contains("WordLineFont"_L1)) {
        config.wordLineFont = layout.value("WordLineFont"_L1).toString();
    }
    if(layout.contains("WordFont"_L1)) {
        config.wordFont = layout.value("WordFont"_L1).toString();
    }

    return config;
}

void LyricsWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["SeekOnClick"_L1]           = config.seekOnClick;
    layout["NoLyricsScript"_L1]        = config.noLyricsScript;
    layout["ScrollDuration"_L1]        = config.scrollDuration;
    layout["ScrollMode"_L1]            = config.scrollMode;
    layout["EdgeFadeMode"_L1]          = config.edgeFadeMode;
    layout["EdgeFadeSize"_L1]          = config.edgeFadeSize;
    layout["ShowScrollbar"_L1]         = config.showScrollbar;
    layout["Alignment"_L1]             = config.alignment;
    layout["LineSpacing"_L1]           = config.lineSpacing;
    layout["CentreFirstSyncedLine"_L1] = config.centreFirstSyncedLine;
    layout["CentreLastSyncedLine"_L1]  = config.centreLastSyncedLine;
    layout["LeftMargin"_L1]            = config.margins.left();
    layout["TopMargin"_L1]             = config.margins.top();
    layout["RightMargin"_L1]           = config.margins.right();
    layout["BottomMargin"_L1]          = config.margins.bottom();

    const bool customColours      = config.colours.isValid() && config.colours.canConvert<Colours>()
                                 && !config.colours.value<Colours>().isEmpty();
    layout["UseCustomColours"_L1] = customColours;

    const auto saveColour = [&layout](const Colours& colours, const QString& key, Colours::Type type) {
        if(colours.hasOverride(type)) {
            layout[key] = colours.lyricsColours.value(type).name(QColor::HexArgb);
        }
        else {
            layout.remove(key);
        }
    };

    if(customColours) {
        const Colours colours = config.colours.value<Colours>();
        saveColour(colours, u"BackgroundColour"_s, Colours::Type::Background);
        saveColour(colours, u"UnsyncedLineColour"_s, Colours::Type::LineUnsynced);
        saveColour(colours, u"UnplayedLineColour"_s, Colours::Type::LineUnplayed);
        saveColour(colours, u"PlayedLineColour"_s, Colours::Type::LinePlayed);
        saveColour(colours, u"CurrentLineColour"_s, Colours::Type::LineSynced);
        saveColour(colours, u"CurrentWordLineColour"_s, Colours::Type::WordLineSynced);
        saveColour(colours, u"CurrentWordColour"_s, Colours::Type::WordSynced);
    }
    else {
        layout.remove("BackgroundColour"_L1);
        layout.remove("UnsyncedLineColour"_L1);
        layout.remove("UnplayedLineColour"_L1);
        layout.remove("PlayedLineColour"_L1);
        layout.remove("CurrentLineColour"_L1);
        layout.remove("CurrentWordLineColour"_L1);
        layout.remove("CurrentWordColour"_L1);
    }

    layout["BaseFont"_L1]     = config.baseFont;
    layout["LineFont"_L1]     = config.lineFont;
    layout["WordLineFont"_L1] = config.wordLineFont;
    layout["WordFont"_L1]     = config.wordFont;
}

void LyricsWidget::playStateChanged(Player::PlayState state)
{
    const auto stopScrolling = [this]() {
        if(m_scrollMode == ScrollMode::Automatic && m_scrollAnim) {
            m_scrollAnim->stop();
        }
    };

    switch(state) {
        case Player::PlayState::Paused:
            stopScrolling();
            break;
        case Player::PlayState::Stopped:
            stopScrolling();
            scrollToCurrentLine(0);
            break;
        case Player::PlayState::Playing:
            updateScrollMode(m_scrollMode);
            break;
    }
}

void LyricsWidget::setCurrentTime(uint64_t time)
{
    m_currentTime = time;
    m_model->setCurrentTime(time);
    highlightCurrentLine();
}

RichText LyricsWidget::noLyricsDisplayText(const Track& track)
{
    auto contextData = makePlaybackScriptContext(
        m_playerController, m_playlistHandler ? m_playlistHandler->activePlaylist() : nullptr,
        TrackListContextPolicy::Fallback, {}, true, false, Gui::ratingStarSymbols(*m_settings));
    const QString displayText = m_parser.evaluate(m_config.noLyricsScript, track, contextData.context);
    if(displayText.isEmpty()) {
        return {};
    }

    ScriptFormatter formatter;

    QFont baseFont;
    if(m_config.baseFont.isEmpty() || !baseFont.fromString(m_config.baseFont)) {
        baseFont = Lyrics::defaultFont();
    }
    formatter.setBaseFont(baseFont);

    const Colours colours = m_config.colours.isValid() ? m_config.colours.value<Colours>() : Colours{};
    formatter.setBaseColour(colours.colour(Colours::Type::LineUnsynced, m_lyricsView->palette()));

    return formatter.evaluate(displayText);
}

void LyricsWidget::updateViewportPadding()
{
    QMargins margins{m_config.margins};
    int topPadding{0};
    int bottomPadding{0};

    if(m_currentLyrics.isSynced()) {
        if(m_config.centreFirstSyncedLine) {
            margins.setTop(0);
            topPadding = m_lyricsView->viewport()->height() / 2;
        }
        if(m_config.centreLastSyncedLine) {
            margins.setBottom(0);
            bottomPadding = m_lyricsView->viewport()->height() / 2;
        }
    }

    m_model->setMargins(margins);
    m_model->setViewportPadding(topPadding, bottomPadding);

    if(m_currentLyrics.isSynced()) {
        m_currentLineStart = -1;
        m_currentLineEnd   = -1;
        highlightCurrentLine();
    }
}

void LyricsWidget::seekTo(const QModelIndex& index, const QPoint& pos)
{
    if(!index.isValid()) {
        return;
    }

    if(!m_currentLyrics.isSynced() || !m_config.seekOnClick) {
        return;
    }

    const auto timestamp = index.data(LyricsModel::TimestampRole).value<uint64_t>();

    if(!m_currentLyrics.isSyncedWords()) {
        m_playerController->seek(timestamp);
        return;
    }

    // Seek to word
    const int wordIndex = m_delegate->wordIndexAt(index, pos, m_lyricsView->visualRect(index));
    if(wordIndex >= 0) {
        const auto words = index.data(LyricsModel::WordsRole).value<std::vector<ParsedWord>>();
        if(std::cmp_less(wordIndex, words.size())) {
            m_playerController->seek(words[wordIndex].timestamp);
            return;
        }
    }

    m_playerController->seek(timestamp);
}

void LyricsWidget::highlightCurrentLine()
{
    if(!m_currentLyrics.isSynced()) {
        return;
    }

    const int newLineStart  = m_model->currentLineIndex();
    const int newLineEnd    = m_model->currentLineLastIndex();
    const int prevLineStart = std::exchange(m_currentLineStart, newLineStart);
    const int prevLineEnd   = std::exchange(m_currentLineEnd, newLineEnd);
    const bool lineChanged  = m_currentLineStart != prevLineStart || m_currentLineEnd != prevLineEnd;

    if(!lineChanged) {
        return;
    }

    if(m_currentLineStart <= 0) {
        scrollToCurrentLine(0);
        return;
    }

    if(m_currentLineStart >= 0) {
        const QModelIndex firstIndex = m_model->index(m_currentLineStart, 0);
        const QModelIndex lastIndex  = m_model->index(m_currentLineEnd, 0);

        if(firstIndex.isValid() && lastIndex.isValid()) {
            const QRect firstRect = m_lyricsView->visualRect(firstIndex);
            const QRect lastRect  = m_lyricsView->visualRect(lastIndex);
            if(firstRect.isValid() && lastRect.isValid()) {
                const int scrollOffset = m_lyricsView->verticalScrollBar()->value();
                const int absoluteY    = m_currentLineStart == m_currentLineEnd
                                           ? firstRect.center().y() + scrollOffset
                                           : (firstRect.top() + lastRect.bottom()) / 2 + scrollOffset;
                scrollToCurrentLine(absoluteY);
            }
            else {
                // Item not visible
                int topY{0};
                for(int i{0}; i < m_currentLineStart; ++i) {
                    const QModelIndex idx = m_model->index(i, 0);
                    topY += m_lyricsView->sizeHintForIndex(idx).height();
                }

                if(m_currentLineStart == m_currentLineEnd) {
                    const QModelIndex idx = m_model->index(m_currentLineStart, 0);
                    scrollToCurrentLine(topY + (m_lyricsView->sizeHintForIndex(idx).height() / 2));
                    return;
                }

                int groupHeight{0};
                for(int i{m_currentLineStart}; i <= m_currentLineEnd; ++i) {
                    const QModelIndex idx = m_model->index(i, 0);
                    groupHeight += m_lyricsView->sizeHintForIndex(idx).height();
                }
                scrollToCurrentLine(topY + (groupHeight / 2));
            }
        }
    }
}

void LyricsWidget::scrollToCurrentLine(int scrollValue)
{
    if(m_isUserScrolling || m_scrollMode == ScrollMode::Manual) {
        return;
    }

    auto* scrollbar       = m_lyricsView->verticalScrollBar();
    const int targetValue = std::clamp(scrollValue - (m_lyricsView->height() / 2), 0, scrollbar->maximum());

    if(m_config.scrollDuration == 0) {
        scrollbar->setValue(targetValue);
        return;
    }

    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }

    m_scrollAnim = new QPropertyAnimation(scrollbar, "value", this);
    m_scrollAnim->setDuration(m_config.scrollDuration);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnim->setStartValue(scrollbar->value());
    m_scrollAnim->setEndValue(targetValue);

    m_scrollAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LyricsWidget::updateScrollMode(ScrollMode mode)
{
    m_scrollMode = mode;
    if(m_scrollMode != ScrollMode::Automatic && m_scrollAnim) {
        m_scrollAnim->stop();
    }
    else if(m_scrollMode == ScrollMode::Automatic) {
        checkStartAutoScroll(m_lyricsView->verticalScrollBar()->value());
    }

    updateEdgeFadeState();
}

void LyricsWidget::checkStartAutoScrollPos(uint64_t pos)
{
    if(m_isUserScrolling) {
        return;
    }

    if(!m_currentLyrics.isSynced() && m_scrollMode == ScrollMode::Automatic) {
        const int maxScroll   = m_lyricsView->verticalScrollBar()->maximum();
        const auto duration   = static_cast<int>(m_playerController->currentTrack().duration());
        const auto startValue = static_cast<int>((static_cast<double>(pos) / duration) * maxScroll);
        updateAutoScroll(startValue);
    }
}

void LyricsWidget::checkStartAutoScroll(int startValue)
{
    if(m_isUserScrolling) {
        return;
    }

    if(!m_currentLyrics.isSynced() && m_scrollMode == ScrollMode::Automatic) {
        updateAutoScroll(startValue);
    }
}

void LyricsWidget::updateAutoScroll(int startValue)
{
    if(m_isUserScrolling) {
        return;
    }

    auto* scrollbar = m_lyricsView->verticalScrollBar();

    scrollbar->setValue(startValue >= 0 ? startValue : scrollbar->value());

    const int maxScroll = scrollbar->maximum();
    if(scrollbar->value() == maxScroll) {
        return;
    }

    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }

    m_scrollAnim = new QPropertyAnimation(scrollbar, "value", this);
    m_scrollAnim->setDuration(
        static_cast<int>(m_playerController->currentTrack().duration() - m_playerController->currentPosition()));
    m_scrollAnim->setEasingCurve(QEasingCurve::Linear);
    m_scrollAnim->setStartValue(scrollbar->value());
    m_scrollAnim->setEndValue(maxScroll);

    m_scrollAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LyricsWidget::updateEdgeFadeState()
{
    m_lyricsView->setEdgeFadeEnabled(shouldEnableEdgeFade());
}

bool LyricsWidget::shouldEnableEdgeFade() const
{
    if(!m_currentLyrics.isValid()) {
        return false;
    }

    const auto fadeMode = static_cast<EdgeFadeMode>(validatedEdgeFadeMode(m_config.edgeFadeMode));
    switch(fadeMode) {
        case EdgeFadeMode::Off:
            return false;
        case EdgeFadeMode::SyncedOnly:
            return m_currentLyrics.isSynced();
        case EdgeFadeMode::ScrollingLyrics:
            if(m_currentLyrics.isSynced()) {
                return m_scrollMode != ScrollMode::Manual;
            }
            return m_scrollMode == ScrollMode::Automatic;
        case EdgeFadeMode::AllLyrics:
            return true;
    }

    return false;
}

} // namespace Fooyin::Lyrics

#include "moc_lyricswidget.cpp"
