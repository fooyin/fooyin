/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "scriptdisplay.h"

#include "scriptdisplayconfigdialog.h"
#include "scripting/scriptcommandhandler.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <gui/configdialog.h>
#include <gui/scripting/richtext.h>
#include <gui/scripting/scriptformatter.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QAbstractTextDocumentLayout>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>

#include <array>

using namespace Qt::StringLiterals;

constexpr auto ScriptKey              = "TextWidget/Script";
constexpr auto FontKey                = "TextWidget/Font";
constexpr auto BackgroundColourKey    = "TextWidget/BackgroundColour";
constexpr auto ForegroundColourKey    = "TextWidget/ForegroundColour";
constexpr auto LinkColourKey          = "TextWidget/LinkColour";
constexpr auto HorizontalAlignmentKey = "TextWidget/HorizontalAlignment";
constexpr auto VerticalAlignmentKey   = "TextWidget/VerticalAlignment";

namespace {
QFont fontFromString(const QString& fontString)
{
    QFont font;
    if(!fontString.isEmpty()) {
        font.fromString(fontString);
    }
    return font;
}

QString normaliseColour(const QString& colour)
{
    const QColor parsedColour{colour};
    return parsedColour.isValid() ? parsedColour.name(QColor::HexArgb) : QString{};
}

QString richTextToHtml(const Fooyin::RichText& richText, const QColor& linkColour = {})
{
    QString html;
    html.reserve(richText.joinedText().size() * 2);

    for(const auto& block : richText.blocks) {
        QStringList styles;

        const bool isLink = !block.format.link.isEmpty();

        if(!block.format.font.family().isEmpty()) {
            styles.emplace_back(u"font-family:'%1'"_s.arg(block.format.font.family().toHtmlEscaped()));
        }
        if(block.format.font.pointSizeF() > 0) {
            styles.emplace_back(u"font-size:%1pt"_s.arg(block.format.font.pointSizeF()));
        }
        if(block.format.font.bold()) {
            styles.emplace_back(u"font-weight:bold"_s);
        }
        if(block.format.font.italic()) {
            styles.emplace_back(u"font-style:italic"_s);
        }
        if(block.format.font.underline()) {
            styles.emplace_back(u"text-decoration:underline"_s);
        }
        if(block.format.font.strikeOut()) {
            styles.emplace_back(u"text-decoration:line-through"_s);
        }
        if(isLink && linkColour.isValid()) {
            styles.emplace_back(u"color:%1"_s.arg(linkColour.name(QColor::HexArgb)));
        }
        else if(block.format.colour.isValid()) {
            styles.emplace_back(u"color:%1"_s.arg(block.format.colour.name(QColor::HexArgb)));
        }

        QString text = block.text.toHtmlEscaped();
        text.replace(u'\n', u"<br/>"_s);

        const QString style = u"white-space:pre-wrap;%1"_s.arg(styles.join(u";"_s));

        if(!isLink) {
            html += u"<span style=\"%1\">%2</span>"_s.arg(style, text);
        }
        else {
            html += u"<a href=\"%1\"><span style=\"%2\">%3</span></a>"_s.arg(block.format.link.toHtmlEscaped(), style,
                                                                             text);
        }
    }

    return html;
}

QString alignmentToCss(const int alignment)
{
    if((alignment & Qt::AlignRight) != 0) {
        return u"right"_s;
    }
    if((alignment & Qt::AlignHCenter) != 0) {
        return u"center"_s;
    }
    return u"left"_s;
}
} // namespace

namespace Fooyin {
// Used to make QTextBrowser::setViewportMargins public
class TextBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    using QTextBrowser::QTextBrowser;
    using QTextBrowser::setViewportMargins;
};

ScriptDisplay::ScriptDisplay(PlayerController* playerController, PlaylistHandler* playlistHandler,
                             ScriptCommandHandler* commandHandler, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_commandHandler{commandHandler}
    , m_settings{settings}
    , m_scriptParser{std::make_unique<ScriptParser>(new ScriptRegistry(m_playerController))}
    , m_layout{new QHBoxLayout(this)}
    , m_text{new TextBrowser(this)}
{
    setObjectName(ScriptDisplay::name());

    m_layout->addWidget(m_text, 1);

    m_text->setReadOnly(true);
    m_text->setOpenLinks(false);
    m_text->setOpenExternalLinks(false);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_text->setUndoRedoEnabled(false);
    m_text->setContextMenuPolicy(Qt::NoContextMenu);
    m_text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_text->document()->setDocumentMargin(0);

    applyConfig(defaultConfig());

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &ScriptDisplay::updateText);
    QObject::connect(m_playerController, &PlayerController::positionChangedSeconds, this, &ScriptDisplay::updateText);
    QObject::connect(m_playerController, &PlayerController::bitrateChanged, this, &ScriptDisplay::updateText);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &ScriptDisplay::updateText);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &ScriptDisplay::updateText);
    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this, &ScriptDisplay::updateText);

    QObject::connect(m_text, &QTextBrowser::anchorClicked, this,
                     [this](const QUrl& url) { activateLink(url.toString()); });
    QObject::connect(m_text->verticalScrollBar(), &QScrollBar::rangeChanged, this,
                     &ScriptDisplay::updateViewportAlignment);
    QObject::connect(m_text->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
                     &ScriptDisplay::updateViewportAlignment);
}

ScriptDisplay::~ScriptDisplay() = default;

QString ScriptDisplay::name() const
{
    return tr("Script Display");
}

QString ScriptDisplay::layoutName() const
{
    return u"ScriptDisplay"_s;
}

ScriptDisplay::ConfigData ScriptDisplay::defaultConfig() const
{
    auto config{factoryConfig()};

    config.script              = m_settings->fileValue(ScriptKey, config.script).toString();
    config.font                = m_settings->fileValue(FontKey, config.font).toString();
    config.bgColour            = normaliseColour(m_settings->fileValue(BackgroundColourKey, {}).toString());
    config.fgColour            = normaliseColour(m_settings->fileValue(ForegroundColourKey, {}).toString());
    config.linkColour          = normaliseColour(m_settings->fileValue(LinkColourKey, {}).toString());
    config.horizontalAlignment = m_settings->fileValue(HorizontalAlignmentKey, config.horizontalAlignment).toInt();
    config.verticalAlignment   = m_settings->fileValue(VerticalAlignmentKey, config.verticalAlignment).toInt();

    return config;
}

ScriptDisplay::ConfigData ScriptDisplay::factoryConfig()
{
    return {
        .script   = uR"(<sized=3><b>%1</b></size>
$crlf()
<alpha=190>%2</alpha>)"_s.arg(tr("Script Display"), tr("Right-click to configure this panel.")),
        .font     = {},
        .bgColour = {},
        .fgColour = {},
        .linkColour          = {},
        .horizontalAlignment = Qt::AlignLeft,
        .verticalAlignment   = Qt::AlignVCenter,
    };
}

const ScriptDisplay::ConfigData& ScriptDisplay::currentConfig() const
{
    return m_config;
}

void ScriptDisplay::applyConfig(const ConfigData& config)
{
    m_config.script              = config.script;
    m_config.font                = config.font;
    m_config.bgColour            = normaliseColour(config.bgColour);
    m_config.fgColour            = normaliseColour(config.fgColour);
    m_config.linkColour          = normaliseColour(config.linkColour);
    m_config.horizontalAlignment = config.horizontalAlignment;
    m_config.verticalAlignment   = config.verticalAlignment;

    applyAppearance();
    updateText();
}

void ScriptDisplay::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(ScriptKey, config.script);
    m_settings->fileSet(FontKey, config.font);
    m_settings->fileSet(BackgroundColourKey, normaliseColour(config.bgColour));
    m_settings->fileSet(ForegroundColourKey, normaliseColour(config.fgColour));
    m_settings->fileSet(LinkColourKey, normaliseColour(config.linkColour));
    m_settings->fileSet(HorizontalAlignmentKey, config.horizontalAlignment);
    m_settings->fileSet(VerticalAlignmentKey, config.verticalAlignment);
}

void ScriptDisplay::clearSavedDefaults() const
{
    m_settings->fileRemove(ScriptKey);
    m_settings->fileRemove(FontKey);
    m_settings->fileRemove(BackgroundColourKey);
    m_settings->fileRemove(ForegroundColourKey);
    m_settings->fileRemove(LinkColourKey);
    m_settings->fileRemove(HorizontalAlignmentKey);
    m_settings->fileRemove(VerticalAlignmentKey);
    m_settings->fileRemove(u"TextWidget/UseFormatter"_s);
    m_settings->fileRemove(u"TextWidget/Colour"_s);
}

void ScriptDisplay::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void ScriptDisplay::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

QSize ScriptDisplay::sizeHint() const
{
    return m_layout->sizeHint();
}

QSize ScriptDisplay::minimumSizeHint() const
{
    return m_layout->minimumSize();
}

void ScriptDisplay::changeEvent(QEvent* event)
{
    FyWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            updateText();
            break;
        default:
            break;
    }
}

void ScriptDisplay::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto applyAlignment = [this](const auto& update) {
        auto config{currentConfig()};
        update(config);
        applyConfig(config);
    };

    const auto addAlignmentMenu = [this, applyAlignment](QMenu* parent, const QString& title, const int currentValue,
                                                         const auto& entries, const auto& setter) {
        auto* subMenu = parent->addMenu(title);
        auto* group   = new QActionGroup(subMenu);
        group->setExclusive(true);

        for(const auto& [text, value] : entries) {
            auto* action = new QAction(text, subMenu);
            subMenu->addAction(action);
            action->setCheckable(true);
            action->setChecked(currentValue == value);
            group->addAction(action);

            QObject::connect(action, &QAction::triggered, this, [value, applyAlignment, setter]() {
                applyAlignment([value, setter](ConfigData& config) { setter(config, value); });
            });
        }
    };

    auto* alignMenu = menu->addMenu(tr("Align"));

    addAlignmentMenu(alignMenu, tr("Horizontal"), m_config.horizontalAlignment,
                     std::to_array<std::pair<QString, int>>({
                         {tr("Left"), Qt::AlignLeft},
                         {tr("Centre"), Qt::AlignHCenter},
                         {tr("Right"), Qt::AlignRight},
                     }),
                     [](ConfigData& config, const int value) { config.horizontalAlignment = value; });

    addAlignmentMenu(alignMenu, tr("Vertical"), m_config.verticalAlignment,
                     std::to_array<std::pair<QString, int>>({
                         {tr("Top"), Qt::AlignTop},
                         {tr("Centre"), Qt::AlignVCenter},
                         {tr("Bottom"), Qt::AlignBottom},
                     }),
                     [](ConfigData& config, const int value) { config.verticalAlignment = value; });

    menu->addSeparator();
    addConfigureAction(menu, false);
    menu->popup(event->globalPos());
}

void ScriptDisplay::openConfigDialog()
{
    showConfigDialog(new ScriptDisplayConfigDialog(this, this));
}

void ScriptDisplay::resizeEvent(QResizeEvent* event)
{
    FyWidget::resizeEvent(event);
    updateViewportAlignment();
}

ScriptDisplay::ConfigData ScriptDisplay::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("Script"_L1)) {
        config.script = layout.value("Script"_L1).toString();
    }
    if(layout.contains("Font"_L1)) {
        config.font = layout.value("Font"_L1).toString();
    }
    if(layout.contains("BackgroundColour"_L1)) {
        config.bgColour = normaliseColour(layout.value("BackgroundColour"_L1).toString());
    }
    if(layout.contains("ForegroundColour"_L1)) {
        config.fgColour = normaliseColour(layout.value("ForegroundColour"_L1).toString());
    }
    if(layout.contains("LinkColour"_L1)) {
        config.linkColour = normaliseColour(layout.value("LinkColour"_L1).toString());
    }
    if(layout.contains("HorizontalAlignment"_L1)) {
        config.horizontalAlignment = layout.value("HorizontalAlignment"_L1).toInt();
    }
    if(layout.contains("VerticalAlignment"_L1)) {
        config.verticalAlignment = layout.value("VerticalAlignment"_L1).toInt();
    }

    return config;
}

void ScriptDisplay::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["Script"_L1]              = config.script;
    layout["Font"_L1]                = config.font;
    layout["BackgroundColour"_L1]    = normaliseColour(config.bgColour);
    layout["ForegroundColour"_L1]    = normaliseColour(config.fgColour);
    layout["LinkColour"_L1]          = normaliseColour(config.linkColour);
    layout["HorizontalAlignment"_L1] = config.horizontalAlignment;
    layout["VerticalAlignment"_L1]   = config.verticalAlignment;
}

void ScriptDisplay::applyAppearance()
{
    if(m_config.font.isEmpty()) {
        m_text->setFont(QFont{});
    }
    else {
        m_text->setFont(fontFromString(m_config.font));
    }

    if(m_config.bgColour.isEmpty()) {
        setAutoFillBackground(false);
        setPalette(QPalette{});
        QPalette browserPalette = m_text->palette();
        browserPalette.setBrush(QPalette::Base, Qt::NoBrush);
        m_text->setPalette(browserPalette);
        m_text->viewport()->setAutoFillBackground(false);
    }
    else {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor{m_config.bgColour});
        setPalette(pal);
        setAutoFillBackground(true);

        QPalette browserPalette = m_text->palette();
        browserPalette.setColor(QPalette::Base, QColor{m_config.bgColour});
        m_text->setPalette(browserPalette);
        m_text->viewport()->setAutoFillBackground(true);
    }

    m_text->show();
    updateViewportAlignment();
}

void ScriptDisplay::updateText()
{
    const Track track      = currentTrack();
    const bool resetScroll = track != m_lastTrack;
    const QString text     = evaluateScript();

    ScriptFormatter formatter;
    formatter.setBaseFont(m_text->font());
    formatter.setBaseColour(m_config.fgColour.isEmpty() ? palette().color(QPalette::WindowText)
                                                        : QColor{m_config.fgColour});

    const QColor linkColour = m_config.linkColour.isEmpty() ? QColor{} : QColor{m_config.linkColour};
    const QString body      = richTextToHtml(formatter.evaluate(text), linkColour);
    const QString html      = u"<html><body style=\"margin:0;text-align:%1;\">%2</body></html>"_s.arg(
        alignmentToCss(m_config.horizontalAlignment), body);

    if(html == m_lastHtml) {
        if(resetScroll) {
            m_text->verticalScrollBar()->setValue(0);
            m_lastTrack = track;
        }
        return;
    }

    auto* scrollBar         = m_text->verticalScrollBar();
    const int previousValue = scrollBar->value();
    const int previousMax   = scrollBar->maximum();

    const QSignalBlocker scrollBlocker{scrollBar};
    m_text->setUpdatesEnabled(false);
    m_text->setHtml(html);
    m_lastHtml  = html;
    m_lastTrack = track;

    if(resetScroll) {
        scrollBar->setValue(0);
    }
    else if(previousMax > 0) {
        const int newMax        = scrollBar->maximum();
        const int restoredValue = static_cast<int>((static_cast<qint64>(previousValue) * newMax) / previousMax);
        scrollBar->setValue(restoredValue);
    }

    updateViewportAlignment();
    m_text->setUpdatesEnabled(true);
    m_text->viewport()->update();
}

void ScriptDisplay::updateViewportAlignment()
{
    int topMargin{0};
    int bottomMargin{0};

    const bool hasVerticalScrollbar = m_text->verticalScrollBar()->maximum() > 0;
    if(!hasVerticalScrollbar) {
        const int availableHeight = m_text->contentsRect().height();
        const int documentHeight  = std::ceil(m_text->document()->documentLayout()->documentSize().height());
        const int extraHeight     = std::max(0, availableHeight - documentHeight - topMargin - bottomMargin);

        if((m_config.verticalAlignment & Qt::AlignBottom) != 0) {
            topMargin += extraHeight;
        }
        else if((m_config.verticalAlignment & Qt::AlignVCenter) != 0) {
            const int topExtra = extraHeight / 2;
            topMargin += topExtra;
            bottomMargin += extraHeight - topExtra;
        }
        else {
            bottomMargin += extraHeight;
        }
    }

    if(auto* label = qobject_cast<TextBrowser*>(m_text)) {
        label->setViewportMargins(0, topMargin, 0, bottomMargin);
    }
}

Track ScriptDisplay::currentTrack() const
{
    if(auto* playlist = m_playlistHandler->activePlaylist(); playlist && playlist->currentTrack().isValid()) {
        return playlist->currentTrack();
    }

    return m_playerController->currentTrack();
}

QString ScriptDisplay::evaluateScript() const
{
    if(const Track track = currentTrack(); track.isValid()) {
        if(auto* playlist = m_playlistHandler->activePlaylist(); playlist && playlist->currentTrack().isValid()) {
            return m_scriptParser->evaluate(m_config.script, *playlist);
        }

        return m_scriptParser->evaluate(m_config.script, track);
    }

    return m_scriptParser->evaluate(m_config.script);
}

void ScriptDisplay::activateLink(const QString& link) const
{
    const QUrl url = QUrl::fromUserInput(link);
    if(!url.isValid()) {
        return;
    }

    if(url.scheme() == "fooyin"_L1 && url.host() == "command"_L1) {
        if(!m_commandHandler) {
            return;
        }

        const QString commandId = QUrl::fromPercentEncoding(url.path().mid(1).toUtf8());
        if(commandId.isEmpty()) {
            return;
        }

        m_commandHandler->execute(commandId);
        return;
    }

    QDesktopServices::openUrl(url);
}
} // namespace Fooyin

#include "moc_scriptdisplay.cpp"
#include "scriptdisplay.moc"
