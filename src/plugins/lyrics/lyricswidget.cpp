/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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
#include "lyricsdelegate.h"
#include "lyricseditor.h"
#include "lyricsfinder.h"
#include "lyricsmodel.h"
#include "lyricssaver.h"
#include "lyricsview.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <core/scripting/scriptparser.h>
#include <gui/configdialog.h>
#include <gui/guisettings.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
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
        case(Qt::AlignLeft):
        case(Qt::AlignRight):
        case(Qt::AlignCenter):
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
} // namespace

namespace Fooyin::Lyrics {
LyricsWidget::LyricsWidget(PlayerController* playerController, EngineController* engine, LyricsFinder* lyricsFinder,
                           LyricsSaver* lyricsSaver, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_engine{engine}
    , m_settings{settings}
    , m_lyricsView{new LyricsView(this)}
    , m_model{new LyricsModel(this)}
    , m_delegate{new LyricsDelegate(this)}
    , m_lyricsFinder{lyricsFinder}
    , m_lyricsSaver{lyricsSaver}
    , m_currentTime{0}
    , m_currentLine{-1}
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

    QObject::connect(m_engine, &EngineController::engineStateChanged, this, &LyricsWidget::playStateChanged);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { updateLyrics(track, false); });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                     [this](const Track& track) { updateLyrics(track, true); });
    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &LyricsWidget::setCurrentTime);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     qOverload<uint64_t>(&LyricsWidget::checkStartAutoScrollPos));

    QObject::connect(m_lyricsView, &LyricsView::lineClicked, this, &LyricsWidget::seekTo);
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
        m_scrollTimer.start(ScrollTimeout, this);
    });

    auto updateThemeDefaults = [this]() {
        if(m_config.colours.isValid() && !m_config.lineFont.isEmpty() && !m_config.wordLineFont.isEmpty()
           && !m_config.wordFont.isEmpty()) {
            return;
        }

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
    QObject::disconnect(m_finderConnection);

    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }
    m_lyricsView->verticalScrollBar()->setValue(0);

    if(std::exchange(m_currentTrack, track) == track && !force) {
        return;
    }

    m_lyrics.clear();
    m_model->setLyrics({});

    if(!track.isValid()) {
        return;
    }

    m_lyricsView->setDisplayString(m_parser.evaluate(m_config.noLyricsScript, track));

    m_finderConnection = QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this, &LyricsWidget::loadLyrics);

    if(m_settings->fileValue(Settings::AutoSearch, true).toBool()) {
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

LyricsWidget::ConfigData LyricsWidget::defaultConfig() const
{
    return {
        .seekOnClick    = m_settings->fileValue(Settings::SeekOnClick, true).toBool(),
        .noLyricsScript = m_settings->fileValue(Settings::NoLyricsScript, defaultNoLyricsScript()).toString(),
        .scrollDuration = m_settings->fileValue(Settings::ScrollDuration, 500).toInt(),
        .scrollMode     = m_settings->fileValue(Settings::ScrollMode, static_cast<int>(ScrollMode::Synced)).toInt(),
        .showScrollbar  = m_settings->fileValue(Settings::ShowScrollbar, true).toBool(),
        .alignment      = m_settings->fileValue(Settings::Alignment, static_cast<int>(Qt::AlignCenter)).toInt(),
        .lineSpacing    = m_settings->fileValue(Settings::LineSpacing, 5).toInt(),
        .margins = m_settings->fileValue(Settings::Margins, QVariant::fromValue(Defaults::margins())).value<QMargins>(),
        .colours = m_settings->fileValue(Settings::Colours),
        .lineFont     = m_settings->fileValue(Settings::LineFont).toString(),
        .wordLineFont = m_settings->fileValue(Settings::WordLineFont).toString(),
        .wordFont     = m_settings->fileValue(Settings::WordFont).toString(),
    };
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
    validated.alignment      = validatedAlignment(validated.alignment);
    validated.lineSpacing    = std::clamp(validated.lineSpacing, 0, 100);
    validated.margins        = clampedMargins(validated.margins);

    if(!validated.colours.canConvert<Colours>()) {
        validated.colours = QVariant{};
    }

    validated.lineFont     = validatedFontString(validated.lineFont);
    validated.wordLineFont = validatedFontString(validated.wordLineFont);
    validated.wordFont     = validatedFontString(validated.wordFont);

    m_settings->fileSet(Settings::SeekOnClick, validated.seekOnClick);
    m_settings->fileSet(Settings::NoLyricsScript, validated.noLyricsScript);
    m_settings->fileSet(Settings::ScrollDuration, validated.scrollDuration);
    m_settings->fileSet(Settings::ScrollMode, validated.scrollMode);
    m_settings->fileSet(Settings::ShowScrollbar, validated.showScrollbar);
    m_settings->fileSet(Settings::Alignment, validated.alignment);
    m_settings->fileSet(Settings::LineSpacing, validated.lineSpacing);
    m_settings->fileSet(Settings::Margins, QVariant::fromValue(validated.margins));
    m_settings->fileSet(Settings::Colours, validated.colours);
    m_settings->fileSet(Settings::LineFont, validated.lineFont);
    m_settings->fileSet(Settings::WordLineFont, validated.wordLineFont);
    m_settings->fileSet(Settings::WordFont, validated.wordFont);
}

void LyricsWidget::applyConfig(const ConfigData& config)
{
    auto validated           = config;
    validated.scrollDuration = std::clamp(validated.scrollDuration, 0, 2000);
    validated.scrollMode     = std::clamp(validated.scrollMode, static_cast<int>(ScrollMode::Manual),
                                          static_cast<int>(ScrollMode::Automatic));
    validated.alignment      = validatedAlignment(validated.alignment);
    validated.lineSpacing    = std::clamp(validated.lineSpacing, 0, 100);
    validated.margins        = clampedMargins(validated.margins);
    if(!validated.colours.canConvert<Colours>()) {
        validated.colours = QVariant{};
    }
    validated.lineFont     = validatedFontString(validated.lineFont);
    validated.wordLineFont = validatedFontString(validated.wordLineFont);
    validated.wordFont     = validatedFontString(validated.wordFont);

    m_config = validated;

    m_lyricsView->setVerticalScrollBarPolicy(m_config.showScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_model->setMargins(m_config.margins);
    m_model->setLineSpacing(m_config.lineSpacing);
    m_model->setAlignment(static_cast<Qt::Alignment>(m_config.alignment));
    m_model->setColours(m_config.colours.isValid() ? m_config.colours.value<Colours>() : Colours{});
    m_model->setFonts(m_config.lineFont, m_config.wordLineFont, m_config.wordFont);

    updateScrollMode(static_cast<ScrollMode>(m_config.scrollMode));

    if(!m_currentLyrics.isValid() && m_currentTrack.isValid()) {
        m_lyricsView->setDisplayString(m_parser.evaluate(m_config.noLyricsScript, m_currentTrack));
    }
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
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollbar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("Alignment"_L1)) {
        config.alignment = layout.value("Alignment"_L1).toInt();
    }
    if(layout.contains("LineSpacing"_L1)) {
        config.lineSpacing = layout.value("LineSpacing"_L1).toInt();
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
    layout["SeekOnClick"_L1]    = config.seekOnClick;
    layout["NoLyricsScript"_L1] = config.noLyricsScript;
    layout["ScrollDuration"_L1] = config.scrollDuration;
    layout["ScrollMode"_L1]     = config.scrollMode;
    layout["ShowScrollbar"_L1]  = config.showScrollbar;
    layout["Alignment"_L1]      = config.alignment;
    layout["LineSpacing"_L1]    = config.lineSpacing;
    layout["LeftMargin"_L1]     = config.margins.left();
    layout["TopMargin"_L1]      = config.margins.top();
    layout["RightMargin"_L1]    = config.margins.right();
    layout["BottomMargin"_L1]   = config.margins.bottom();

    const bool customColours      = config.colours.isValid() && config.colours.canConvert<Colours>();
    layout["UseCustomColours"_L1] = customColours;
    if(customColours) {
        const Colours colours              = config.colours.value<Colours>();
        layout["BackgroundColour"_L1]      = colours.colour(Colours::Type::Background).name(QColor::HexArgb);
        layout["UnsyncedLineColour"_L1]    = colours.colour(Colours::Type::LineUnsynced).name(QColor::HexArgb);
        layout["UnplayedLineColour"_L1]    = colours.colour(Colours::Type::LineUnplayed).name(QColor::HexArgb);
        layout["PlayedLineColour"_L1]      = colours.colour(Colours::Type::LinePlayed).name(QColor::HexArgb);
        layout["CurrentLineColour"_L1]     = colours.colour(Colours::Type::LineSynced).name(QColor::HexArgb);
        layout["CurrentWordLineColour"_L1] = colours.colour(Colours::Type::WordLineSynced).name(QColor::HexArgb);
        layout["CurrentWordColour"_L1]     = colours.colour(Colours::Type::WordSynced).name(QColor::HexArgb);
    }

    layout["LineFont"_L1]     = config.lineFont;
    layout["WordLineFont"_L1] = config.wordLineFont;
    layout["WordFont"_L1]     = config.wordFont;
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

    auto* searchLyrics = new QAction(tr("Search for lyrics"), menu);
    searchLyrics->setStatusTip(tr("Search for lyrics for the current track"));
    QObject::connect(searchLyrics, &QAction::triggered, this, [this]() {
        QObject::disconnect(m_finderConnection);
        m_finderConnection
            = QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this, &LyricsWidget::loadLyrics);
        m_lyrics.clear();
        m_lyricsFinder->findLyrics(m_currentTrack);
    });
    menu->addAction(searchLyrics);

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
    addConfigureAction(menu);

    menu->popup(event->globalPos());
}

void LyricsWidget::loadLyrics(const Lyrics& lyrics)
{
    const bool first = m_lyrics.empty();
    m_lyrics.push_back(lyrics);

    if(first) {
        changeLyrics(lyrics);
    }
}

void LyricsWidget::changeLyrics(const Lyrics& lyrics)
{
    m_currentLyrics = lyrics;

    if(!lyrics.isLocal) {
        m_lyricsSaver->autoSaveLyrics(lyrics, m_currentTrack);
    }

    if(lyrics.isValid()) {
        m_model->setLyrics(m_currentLyrics);
        m_lyricsView->setDisplayString({});
    }
    else {
        m_lyricsView->setDisplayString(m_parser.evaluate(m_config.noLyricsScript, m_currentTrack));
        m_model->setLyrics({});
    }
}

void LyricsWidget::openEditor(const Lyrics& lyrics)
{
    auto* dlg = new LyricsEditorDialog(lyrics, m_playerController, m_lyricsSaver, m_settings, Utils::getMainWindow());
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dlg, &QDialog::finished, dlg, &LyricsEditorDialog::saveState);
    QObject::connect(dlg->editor(), &LyricsEditor::lyricsEdited, this, &LyricsWidget::changeLyrics);

    dlg->show();
    dlg->restoreState();
}

void LyricsWidget::openConfigDialog()
{
    showConfigDialog(new LyricsConfigDialog(this, this));
}

void LyricsWidget::playStateChanged(Engine::PlaybackState state)
{
    const auto stopScrolling = [this]() {
        if(m_scrollMode == ScrollMode::Automatic && m_scrollAnim) {
            m_scrollAnim->stop();
        }
    };

    switch(state) {
        case(Engine::PlaybackState::Paused):
            stopScrolling();
            break;
        case(Engine::PlaybackState::Error):
        case(Engine::PlaybackState::Stopped):
            stopScrolling();
            scrollToCurrentLine(0);
            break;
        case(Engine::PlaybackState::Playing):
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
    if(wordIndex > 0) {
        const auto words = index.data(LyricsModel::WordsRole).value<std::vector<ParsedWord>>();
        if(std::cmp_less(wordIndex, words.size())) {
            m_playerController->seek(words[wordIndex].timestamp);
            return;
        }
    }
}

void LyricsWidget::highlightCurrentLine()
{
    if(!m_currentLyrics.isSynced()) {
        return;
    }

    const int newLine      = m_model->currentLineIndex();
    const int prevLine     = std::exchange(m_currentLine, newLine);
    const bool lineChanged = m_currentLine != prevLine;

    if(!lineChanged) {
        return;
    }

    if(m_currentLine <= 0) {
        scrollToCurrentLine(0);
        return;
    }

    if(m_currentLine >= 0) {
        const QModelIndex currentIndex = m_model->index(m_currentLine, 0);
        if(currentIndex.isValid()) {
            const QRect rect = m_lyricsView->visualRect(currentIndex);
            if(rect.isValid()) {
                const int absoluteY = rect.top() + m_lyricsView->verticalScrollBar()->value();
                scrollToCurrentLine(absoluteY);
            }
            else {
                // Item not visible
                int y{0};
                for(int i{0}; i < m_currentLine; ++i) {
                    const QModelIndex idx = m_model->index(i, 0);
                    y += m_lyricsView->sizeHintForIndex(idx).height();
                }
                scrollToCurrentLine(y);
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
} // namespace Fooyin::Lyrics

#include "moc_lyricswidget.cpp"
