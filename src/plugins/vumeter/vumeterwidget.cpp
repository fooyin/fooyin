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

#include "vumeterwidget.h"

#include "vumetercolours.h"
#include "vumetersettings.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>
#include <core/player/playercontroller.h>
#include <gui/guisettings.h>
#include <utils/async.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QBasicTimer>
#include <QElapsedTimer>
#include <QContextMenuEvent>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QTimerEvent>

using namespace Qt::StringLiterals;

constexpr auto MaxChannels    = 20;
constexpr auto UpdateInterval = 25;
constexpr auto MinDb          = -60.0F;
constexpr auto MaxDb          = 3.0F;
constexpr auto DbRange        = MaxDb - MinDb;
constexpr auto TickInterval   = 10;
constexpr auto LegendPadding  = 10;

namespace {
float dbScale(float db)
{
    static constexpr auto scaleMin = 0.0F;
    static constexpr auto scaleMax = 1.0F;

    if(db < MinDb) {
        return scaleMin;
    }
    if(db > MaxDb) {
        return scaleMax;
    }

    const float scale = (db - MinDb) / (MaxDb - MinDb);
    return scale;
}

float dbOnRange(float db)
{
    return std::clamp<float>(db, MinDb, MaxDb);
}

QString channelName(int channel)
{
    static const QStringList channelNames
        = {u"FL"_s,  u"FR"_s,  u"FC"_s,  u"LFE"_s, u"SL"_s,  u"SR"_s,   u"BL"_s,   u"BR"_s,  u"BC"_s,
           u"TFL"_s, u"TFR"_s, u"TBC"_s, u"TML"_s, u"TMR"_s, u"TFLR"_s, u"TFRL"_s, u"TBR"_s, u"TBL"_s};

    if(channel >= 0 && channel < channelNames.size()) {
        return channelNames.at(channel);
    }

    return u"Unknown"_s;
}
} // namespace

namespace Fooyin::VuMeter {
class VuMeterWidgetPrivate
{
public:
    explicit VuMeterWidgetPrivate(VuMeterWidget* self, VuMeterWidget::Type type, PlayerController* playerController,
                                  SettingsManager* settings);

    void reset();
    void updateSize();
    void calculatePeak();
    void updateChannelLevels(int channel, qint64 elapsedTime, qint64 peakTime, float falloff, bool& zeroLevel);
    QRect calculateUpdateRect(int channel);
    void createGradient();

    [[nodiscard]] float channelSize() const;
    [[nodiscard]] float barSize() const;
    [[nodiscard]] float dbToSize(float db) const;
    [[nodiscard]] float dbToPos(float db) const;

    [[nodiscard]] bool isHorizontal() const;

    [[nodiscard]] float channelX(int channel) const;
    [[nodiscard]] float channelY(int channel) const;

    void drawLegend(QPainter& painter);
    void drawChannel(QPainter& painter, float start, int channel, float size);
    void drawHorizontalBars(QPainter& painter, float x, float y, float channelLevel, float channelSize,
                            float start) const;
    void drawVerticalBars(QPainter& painter, float x, float channelLevel, float channelSize, float start) const;

    void playStateChanged(Player::PlayState state);

    VuMeterWidget* m_self;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    AudioFormat m_format;
    std::array<float, MaxChannels> m_channelDbLevels;
    std::array<float, MaxChannels> m_channelPeaks;
    std::vector<QElapsedTimer> m_lastPeakTimers;

    VuMeterWidget::Type m_type{VuMeterWidget::Type::Peak};
    Qt::Orientation m_orientation{Qt::Horizontal};
    bool m_showPeaks{false};
    float m_channelSpacing;
    bool m_showLegend{false};
    float m_barSize;
    float m_barSpacing;
    int m_barSections;
    float m_sectionSpacing;

    float m_meterWidth{0};
    float m_meterHeight{0};
    float m_legendSize{0};
    float m_labelsSize{0};
    bool m_drawLegend{false};
    bool m_changingTrack{false};
    bool m_stopping{false};

    Colours m_colours;
    QLinearGradient m_gradient;
    QBasicTimer m_updateTimer;
    QElapsedTimer m_elapsedTimer;

    std::array<float, MaxChannels> m_previousChannelDbLevels{0.0F};
    std::array<float, MaxChannels> m_previousChannelPeaks{0.0F};
};

VuMeterWidgetPrivate::VuMeterWidgetPrivate(VuMeterWidget* self, VuMeterWidget::Type type,
                                           PlayerController* playerController, SettingsManager* settings)
    : m_self{self}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_type{type}
    , m_channelSpacing{static_cast<float>(m_settings->value<Settings::VuMeter::ChannelSpacing>())}
    , m_barSize{static_cast<float>(m_settings->value<Settings::VuMeter::BarSize>())}
    , m_barSpacing{static_cast<float>(m_settings->value<Settings::VuMeter::BarSpacing>())}
    , m_barSections{m_settings->value<Settings::VuMeter::BarSections>()}
    , m_sectionSpacing{static_cast<float>(m_settings->value<Settings::VuMeter::SectionSpacing>())}
    , m_colours{m_settings->value<Settings::VuMeter::MeterColours>().value<Colours>()}
{
    m_format.setSampleFormat(SampleFormat::F32);

    playStateChanged(m_playerController->playState());

    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_self,
                     [this]() { playStateChanged(m_playerController->playState()); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_self,
                     [this]() { playStateChanged(m_playerController->playState()); });

    reset();
    updateSize();
}

void VuMeterWidgetPrivate::reset()
{
    std::ranges::fill(m_channelDbLevels, MinDb);
    std::ranges::fill(m_channelPeaks, MinDb);
    std::ranges::for_each(m_lastPeakTimers, &QElapsedTimer::start);
}

void VuMeterWidgetPrivate::updateSize()
{
    const auto width  = static_cast<float>(m_self->width());
    const auto height = static_cast<float>(m_self->height());

    m_drawLegend  = false;
    m_meterHeight = height;
    m_legendSize  = 0;
    m_labelsSize  = 0;
    m_meterWidth  = width;

    const QFontMetrics fm{m_self->fontMetrics()};
    const auto textRect = fm.boundingRect(u"-60dB"_s);

    if(m_showLegend) {
        if(isHorizontal()) {
            if(width > 300 && height > 60) {
                m_drawLegend  = true;
                m_legendSize  = static_cast<float>(textRect.height() + LegendPadding);
                m_labelsSize  = static_cast<float>(fm.horizontalAdvance(u"TFLR"_s) + 5);
                m_meterHeight = height - m_legendSize;
                m_meterWidth  = width - m_labelsSize;
            }
        }
        else if(width > 100 && height > 150) {
            m_drawLegend  = true;
            m_legendSize  = static_cast<float>(textRect.width() + LegendPadding);
            m_labelsSize  = static_cast<float>(textRect.height() + 5);
            m_meterHeight = height - m_labelsSize;
            m_meterWidth  = width - m_legendSize;
        }
    }

    createGradient();
}

void VuMeterWidgetPrivate::calculatePeak()
{
    const qint64 elapsedTime = m_elapsedTimer.restart();
    const auto peakTime      = static_cast<qint64>(m_settings->value<Settings::VuMeter::PeakHoldTime>() * 1000);
    const auto falloff       = static_cast<float>(m_settings->value<Settings::VuMeter::FalloffTime>() / 1000.0);

    QRect updateRect;

    bool m_zeroLevel{true};

    const int channels = m_format.channelCount();
    for(int channel{0}; channel < channels; ++channel) {
        updateChannelLevels(channel, elapsedTime, peakTime, falloff, m_zeroLevel);
        if(m_barSize > 0) {
            updateRect = updateRect.united(calculateUpdateRect(channel));
        }
    }

    if(m_stopping && m_zeroLevel) {
        m_stopping = false;
        m_updateTimer.stop();
    }

    if(m_barSize == 0 || m_changingTrack) {
        m_self->update();
    }
    else {
        m_self->update(updateRect);
    }
}

void VuMeterWidgetPrivate::updateChannelLevels(int channel, qint64 elapsedTime, qint64 peakTime, float falloff,
                                               bool& zeroLevel)
{
    float& level    = m_channelDbLevels.at(channel);
    float& peak     = m_channelPeaks.at(channel);
    auto& peakTimer = m_lastPeakTimers.at(channel);

    const qint64 elapsedPeak = peakTimer.elapsed();
    const auto decay         = static_cast<float>(elapsedTime) * falloff;

    level = dbOnRange(level - decay);

    if(level > MinDb) {
        zeroLevel = false;
    }

    if(level > peak || elapsedPeak > peakTime) {
        peak = level;
        peakTimer.start();
    }
}

QRect VuMeterWidgetPrivate::calculateUpdateRect(int channel)
{
    const auto labelSize = static_cast<int>(m_labelsSize);
    const auto barSize   = static_cast<int>(m_barSize + m_barSpacing);

    if(isHorizontal()) {
        const int y        = static_cast<int>(channelY(channel));
        const int oldX     = labelSize + static_cast<int>(dbToSize(m_previousChannelDbLevels.at(channel)));
        const int oldPeakX = labelSize + static_cast<int>(dbToSize(m_previousChannelPeaks.at(channel)));
        const int levelX   = labelSize + static_cast<int>(dbToSize(m_channelDbLevels.at(channel)));
        const int peakX    = labelSize + static_cast<int>(dbToSize(m_channelPeaks.at(channel)));

        const int minX = std::min({oldX, oldPeakX, levelX, peakX});
        const int maxX = std::max({oldX, oldPeakX, levelX, peakX});

        int snappedMinX       = (minX / barSize) * barSize;
        const int snappedMaxX = ((maxX + barSize - 1) / barSize) * barSize;
        int width             = snappedMaxX - snappedMinX;

        if(snappedMinX - (2 * barSize) > labelSize) {
            snappedMinX -= (2 * barSize);
            width += (4 * barSize);
        }

        return {snappedMinX, y, width, static_cast<int>(channelSize()) + 1};
    }

    const int x        = static_cast<int>(channelX(channel));
    const int oldY     = static_cast<int>(dbToPos(m_previousChannelDbLevels.at(channel))) - labelSize;
    const int oldPeakY = static_cast<int>(dbToPos(m_previousChannelPeaks.at(channel))) - labelSize;
    const int levelY   = static_cast<int>(dbToPos(m_channelDbLevels.at(channel))) - labelSize;
    const int peakY    = static_cast<int>(dbToPos(m_channelPeaks.at(channel))) - labelSize;

    const int minY = std::min({oldY, oldPeakY, levelY, peakY});
    const int maxY = std::max({oldY, oldPeakY, levelY, peakY});

    int snappedMinY       = (minY / barSize) * barSize;
    const int snappedMaxY = ((maxY + barSize - 1) / barSize) * barSize;
    int height            = snappedMaxY - snappedMinY;

    if(snappedMinY - (2 * barSize) > labelSize) {
        snappedMinY -= (2 * barSize);
        height += (4 * barSize);
    }

    return {x, snappedMinY, static_cast<int>(channelSize()) + 1, height};
}

void VuMeterWidgetPrivate::createGradient()
{
    QLinearGradient pattern;

    if(m_orientation == Qt::Horizontal) {
        pattern = {0, 0, m_meterWidth, 0};
    }
    else {
        pattern = {0, m_meterHeight, 0, 0};
    }

    pattern.setColorAt(dbScale(-60), m_colours.colour(Colours::Type::Gradient1));
    pattern.setColorAt(dbScale(3), m_colours.colour(Colours::Type::Gradient2));

    m_gradient = pattern;
}

float VuMeterWidgetPrivate::channelSize() const
{
    if(isHorizontal()) {
        return m_meterHeight / static_cast<float>(m_format.channelCount());
    }
    return m_meterWidth / static_cast<float>(m_format.channelCount());
}

float VuMeterWidgetPrivate::barSize() const
{
    const int channels  = m_format.channelCount();
    const float spacing = m_channelSpacing * static_cast<float>(channels - 1);

    if(isHorizontal()) {
        return (m_meterHeight - spacing) / static_cast<float>(channels);
    }
    return (m_meterWidth - spacing) / static_cast<float>(channels);
}

float VuMeterWidgetPrivate::dbToSize(float db) const
{
    return dbScale(db) * (isHorizontal() ? m_meterWidth : m_meterHeight);
}

float VuMeterWidgetPrivate::dbToPos(float db) const
{
    if(isHorizontal()) {
        return m_meterWidth - dbToSize(db);
    }

    return m_meterHeight + m_labelsSize - dbToSize(db);
}

bool VuMeterWidgetPrivate::isHorizontal() const
{
    return m_orientation == Qt::Horizontal;
}

float VuMeterWidgetPrivate::channelX(int channel) const
{
    if(isHorizontal()) {
        return m_labelsSize;
    }
    return m_legendSize + (static_cast<float>(channel) * (barSize() + m_channelSpacing));
}

float VuMeterWidgetPrivate::channelY(int channel) const
{
    if(!isHorizontal()) {
        return 0;
    }
    return static_cast<float>(channel) * (barSize() + m_channelSpacing);
}

void VuMeterWidgetPrivate::drawLegend(QPainter& painter)
{
    if(!m_drawLegend) {
        return;
    }

    const auto dbToLegendPos = [this](int db) -> int {
        const auto fltDb = static_cast<float>(db);

        if(isHorizontal()) {
            return static_cast<int>((m_meterWidth * (fltDb - MinDb) / DbRange) + m_labelsSize);
        }

        const auto height = m_meterHeight;
        return static_cast<int>(height - (height * (fltDb - MinDb) / DbRange));
    };

    QPen linePen      = painter.pen();
    QColor lineColour = linePen.color();
    lineColour.setAlpha(65);
    linePen.setColor(lineColour);

    const QFontMetrics fm{painter.fontMetrics()};
    const QString dbText = u"%1dB"_s;

    for(int db{static_cast<int>(MinDb)}; db <= 0; db += TickInterval) {
        const QString text  = dbText.arg(db);
        const int textWidth = fm.horizontalAdvance(text);

        if(isHorizontal()) {
            const int x      = dbToLegendPos(db);
            const auto textX = x - (textWidth / 2);
            const auto textY = static_cast<int>(m_meterHeight + (m_legendSize * 0.5F) + 5);

            painter.drawText(textX, textY, text);

            painter.save();
            painter.setPen(linePen);

            const int tickHeight = static_cast<int>(m_meterHeight - 1);
            painter.drawLine(x, 0, x, tickHeight);
        }
        else {
            const int y      = dbToLegendPos(db);
            const auto textX = static_cast<int>(m_legendSize) - textWidth - 5;
            const auto textY = y + (fm.height() / 4);

            painter.drawText(textX, textY, text);

            painter.save();
            painter.setPen(linePen);

            const int tickWidth = static_cast<int>(m_meterWidth + m_legendSize - 1);
            painter.drawLine(static_cast<int>(m_legendSize), y, tickWidth, y);
        }

        painter.restore();
    }

    const int channels = m_format.channelCount();
    for(int i{0}; i < channels; ++i) {
        const QString name = channelName(i);

        if(isHorizontal()) {
            const int textWidth = fm.horizontalAdvance(name);
            const int textX     = static_cast<int>(m_labelsSize) - textWidth - 5;
            const auto textY = static_cast<int>(channelY(i) + (barSize() / 2) + (static_cast<float>(fm.height()) / 4));

            painter.drawText(textX, textY, name);
        }
        else {
            const int textWidth = fm.horizontalAdvance(name);
            const auto textX    = static_cast<int>(channelX(i)) + static_cast<int>(barSize() / 2) - (textWidth / 2);
            const auto textY    = static_cast<int>(m_meterHeight + m_labelsSize - 5);

            painter.drawText(textX, textY, name);
        }
    }
}

void VuMeterWidgetPrivate::drawChannel(QPainter& painter, float start, int channel, float channelSize)
{
    const float x            = channelX(channel);
    const float y            = channelY(channel);
    const float channelLevel = m_channelDbLevels.at(channel);
    const float channelPeak  = m_channelPeaks.at(channel);

    m_previousChannelDbLevels.at(channel) = channelLevel;
    m_previousChannelPeaks.at(channel)    = channelPeak;

    if(isHorizontal()) {
        drawHorizontalBars(painter, x, y, channelLevel, channelSize, start);
    }
    else {
        drawVerticalBars(painter, x, channelLevel, channelSize, start);
    }

    if(m_showPeaks && channelPeak > MinDb) {
        painter.setPen(m_colours.colour(Colours::Type::Peak));
        if(isHorizontal()) {
            const auto peakX = m_labelsSize + dbToSize(channelPeak);
            painter.drawLine(QLineF{peakX, y, peakX, y + channelSize - m_channelSpacing});
        }
        else {
            const auto peakY = dbToPos(channelPeak) - m_labelsSize;
            painter.drawLine(QLineF{x, peakY, x + channelSize - m_channelSpacing, peakY});
        }
    }

    m_changingTrack = false;
}

void VuMeterWidgetPrivate::drawHorizontalBars(QPainter& painter, float x, float y, float channelLevel,
                                              float channelSize, float start) const
{
    const auto sections = static_cast<float>(m_barSections);
    const float barSize = m_barSize + m_barSpacing;
    const int barCount  = m_barSize == 0 ? 1 : static_cast<int>(m_meterWidth / barSize);
    const auto bars     = static_cast<float>(barCount);
    const float dbStep  = DbRange / bars;

    const float totalHeight = (channelSize - (m_sectionSpacing * (sections - 1)));
    const float barHeight   = totalHeight / sections;

    if(barCount == 1) {
        for(int row{0}; row < m_barSections; ++row) {
            const float barY = y + static_cast<float>(row) * (barHeight + m_barSpacing);
            painter.fillRect(QRectF{x, barY, dbToSize(channelLevel), barHeight}, m_gradient);
        }
    }
    else {
        const auto first = static_cast<int>(std::max(0.0F, (((start - m_labelsSize) / m_meterWidth) * bars)));
        for(int column{first}; column < barCount; ++column) {
            const float barDb = MinDb + static_cast<float>(column) * dbStep;
            if(channelLevel > barDb) {
                const float barX = x + (static_cast<float>(column) * barSize);

                for(int row{0}; row < m_barSections; ++row) {
                    const float barY = y + static_cast<float>(row) * (barHeight + m_sectionSpacing);
                    painter.fillRect(QRectF{barX, barY, m_barSize, barHeight}, m_gradient);
                }
            }
        }
    }
}

void VuMeterWidgetPrivate::drawVerticalBars(QPainter& painter, float x, float channelLevel, float channelSize,
                                            float start) const
{
    const auto sections = static_cast<float>(m_barSections);
    const float barSize = m_barSize + m_barSpacing;
    const int barCount  = m_barSize == 0 ? 1 : static_cast<int>(m_meterHeight / barSize);
    const auto bars     = static_cast<float>(barCount);
    const float dbStep  = DbRange / bars;

    const float totalWidth = (channelSize - (m_sectionSpacing * (sections - 1)));
    const float barWidth   = totalWidth / sections;

    if(barCount == 1) {
        for(int column{0}; column < m_barSections; ++column) {
            const float barX = x + static_cast<float>(column) * (barWidth + m_barSpacing);
            const float barY = dbToPos(channelLevel) - m_labelsSize;
            painter.fillRect(QRectF{barX, barY, barWidth, m_meterHeight - barY}, m_gradient);
        }
    }
    else {
        const auto first = static_cast<int>(std::max(0.0F, bars - ((start / m_meterHeight) * bars) - 1));
        for(int row{first}; row < barCount; ++row) {
            const float barDb = MinDb + static_cast<float>(row) * dbStep;

            if(channelLevel > barDb) {
                const float barY = m_meterHeight - (static_cast<float>(row) * barSize);

                for(int column{0}; column < m_barSections; ++column) {
                    const float barX = x + static_cast<float>(column) * (barWidth + m_sectionSpacing);
                    painter.fillRect(QRectF{barX, barY, barWidth, m_barSize}, m_gradient);
                }
            }
        }
    }
}

void VuMeterWidgetPrivate::playStateChanged(Player::PlayState state)
{
    m_changingTrack = true;
    updateSize();

    switch(state) {
        case(Player::PlayState::Playing):
            m_updateTimer.start(UpdateInterval, m_self);
            m_elapsedTimer.start();
            break;
        case(Player::PlayState::Paused):
            m_updateTimer.stop();
            break;
        case(Player::PlayState::Stopped):
            if(m_updateTimer.isActive()) {
                m_stopping = true;
            }
            break;
    }
}

VuMeterWidget::VuMeterWidget(Type type, PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<VuMeterWidgetPrivate>(this, type, playerController, settings)}
{
    setObjectName(VuMeterWidget::name());

    p->m_settings->subscribe<Settings::VuMeter::ChannelSpacing>(this, &VuMeterWidget::setChannelSpacing);
    p->m_settings->subscribe<Settings::VuMeter::BarSize>(this, &VuMeterWidget::setBarSize);
    p->m_settings->subscribe<Settings::VuMeter::BarSpacing>(this, &VuMeterWidget::setBarSpacing);
    p->m_settings->subscribe<Settings::VuMeter::BarSections>(this, &VuMeterWidget::setBarSections);
    p->m_settings->subscribe<Settings::VuMeter::SectionSpacing>(this, &VuMeterWidget::setSectionSpacing);

    auto updateColours = [this]() {
        p->m_colours = p->m_settings->value<Settings::VuMeter::MeterColours>().value<Colours>();
        p->createGradient();
        update();
    };
    p->m_settings->subscribe<Settings::VuMeter::MeterColours>(this, updateColours);
    p->m_settings->subscribe<Settings::Gui::Theme>(this, updateColours);
    p->m_settings->subscribe<Settings::Gui::Style>(this, updateColours);
}

VuMeterWidget::~VuMeterWidget() = default;

QString VuMeterWidget::name() const
{
    return p->m_type == Type::Peak ? tr("Peak Meter") : tr("VU Meter");
}

QString VuMeterWidget::layoutName() const
{
    return p->m_type == Type::Peak ? u"PeakMeter"_s : u"VUMeter"_s;
}

void VuMeterWidget::saveLayoutData(QJsonObject& layout)
{
    layout["Orientation"_L1] = p->m_orientation;
    layout["ShowLegend"_L1]  = p->m_showLegend;
    layout["ShowPeaks"_L1]   = p->m_showPeaks;
}

void VuMeterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Orientation"_L1)) {
        setOrientation(static_cast<Qt::Orientation>(layout.value("Orientation"_L1).toInt()));
    }
    if(layout.contains("ShowLegend"_L1)) {
        setShowLegend(layout.value("ShowLegend"_L1).toBool());
    }
    if(layout.contains("ShowPeaks"_L1)) {
        p->m_showPeaks = layout.value("ShowPeaks"_L1).toBool();
    }
}

void VuMeterWidget::renderBuffer(const AudioBuffer& buffer)
{
    p->m_format.setSampleRate(buffer.format().sampleRate());
    const int channels = buffer.format().channelCount();
    p->m_format.setChannelCount(channels);

    const AudioBuffer normalisedBuffer = Audio::convert(buffer, p->m_format);

    p->m_lastPeakTimers.resize(channels);

    auto calculatePeaks = Utils::asyncExec([this, normalisedBuffer, channels]() {
        const int totalSamples = normalisedBuffer.sampleCount();
        const int bps          = normalisedBuffer.format().bytesPerSample();

        std::array<float, MaxChannels> peaks{0.0F};
        std::array<int, MaxChannels> sampleCounts{0};

        for(int i{0}; i < totalSamples; ++i) {
            const int sampleIndex  = i / channels;
            const int channelIndex = i % channels;

            float sample;
            const auto offset = (sampleIndex * channels + channelIndex) * bps;
            std::memcpy(&sample, normalisedBuffer.data() + offset, bps);

            if(p->m_type == Type::Peak) {
                peaks.at(channelIndex) = std::max(peaks.at(channelIndex), std::abs(sample));
            }
            else {
                peaks.at(channelIndex) += sample * sample;
                sampleCounts.at(channelIndex)++;
            }
        }

        if(p->m_type == Type::Rms) {
            for(int i{0}; i < channels; ++i) {
                peaks.at(i) = std::sqrt(peaks.at(i) / static_cast<float>(sampleCounts.at(i)));
            }
        }

        return peaks;
    });

    calculatePeaks.then(this, [this, channels](const std::array<float, MaxChannels>& peaks) {
        for(int i{0}; i < channels; ++i) {
            const float bufferRMS = peaks.at(i);
            const float bufferDb  = dbOnRange(20 * std::log10(bufferRMS));

            float& channelLevel = p->m_channelDbLevels.at(i);
            float& channelPeak  = p->m_channelPeaks.at(i);

            if(bufferDb > channelLevel) {
                channelLevel = bufferDb;
            }
            if(bufferDb > channelPeak) {
                channelPeak = bufferDb;
                p->m_lastPeakTimers.at(i).start();
            }
        }
    });
}

void VuMeterWidget::setOrientation(Qt::Orientation orientation)
{
    p->m_orientation = orientation;
    p->updateSize();
    update();
}

void VuMeterWidget::setShowLegend(bool show)
{
    p->m_showLegend = show;
    p->updateSize();
    update();
}

void VuMeterWidget::setChannelSpacing(int size)
{
    p->m_channelSpacing = static_cast<float>(size);
    update();
}

void VuMeterWidget::setBarSize(int size)
{
    p->m_barSize = static_cast<float>(size);
    update();
}

void VuMeterWidget::setBarSpacing(int size)
{
    p->m_barSpacing = static_cast<float>(size);
    update();
}

void VuMeterWidget::setBarSections(int count)
{
    p->m_barSections = count;
    update();
}

void VuMeterWidget::setSectionSpacing(int size)
{
    p->m_sectionSpacing = static_cast<float>(size);
    update();
}

QSize VuMeterWidget::minimumSizeHint() const
{
    return {5, 5};
}

void VuMeterWidget::resizeEvent(QResizeEvent* event)
{
    p->updateSize();
    FyWidget::resizeEvent(event);
}

void VuMeterWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_updateTimer.timerId()) {
        p->calculatePeak();
    }
    FyWidget::timerEvent(event);
}

void VuMeterWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};
    painter.fillRect(0, 0, width(), height(), p->m_colours.colour(Colours::Type::Background));

    p->drawLegend(painter);

    const float channelSize = p->barSize();

    const QRect rect = event->rect();
    const auto first = static_cast<float>(p->isHorizontal() ? rect.left() : rect.bottom());

    const int channels = p->m_format.channelCount();
    for(int channel{0}; channel < channels; ++channel) {
        painter.save();
        p->drawChannel(painter, first, channel, channelSize);
        painter.restore();
    }
}

void VuMeterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);

    auto* showPeaks = new QAction(tr("Show peaks"), menu);
    showPeaks->setCheckable(true);
    showPeaks->setChecked(p->m_showPeaks);
    QObject::connect(showPeaks, &QAction::triggered, this, [this](const bool checked) { p->m_showPeaks = checked; });

    auto* showLegend = new QAction(tr("Show legend"), menu);
    showLegend->setCheckable(true);
    showLegend->setChecked(p->m_showLegend);
    QObject::connect(showLegend, &QAction::triggered, this, [this](const bool checked) { setShowLegend(checked); });

    auto* orientationMenu  = new QMenu(tr("Orientation"), menu);
    auto* orientationGroup = new QActionGroup(orientationMenu);

    auto* horizontal = new QAction(tr("Horizontal"), orientationGroup);
    auto* vertical   = new QAction(tr("Vertical"), orientationGroup);

    horizontal->setCheckable(true);
    vertical->setCheckable(true);

    horizontal->setChecked(p->isHorizontal());
    vertical->setChecked(!p->isHorizontal());

    QObject::connect(horizontal, &QAction::triggered, this, [this]() { setOrientation(Qt::Horizontal); });
    QObject::connect(vertical, &QAction::triggered, this, [this]() { setOrientation(Qt::Vertical); });

    orientationMenu->addAction(horizontal);
    orientationMenu->addAction(vertical);

    auto* gotoSettings = new QAction(tr("Settings…"), menu);
    QObject::connect(gotoSettings, &QAction::triggered, this,
                     [this]() { p->m_settings->settingsDialog()->openAtPage(VuMeterPage); });

    menu->addAction(showPeaks);
    menu->addAction(showLegend);
    menu->addSeparator();
    menu->addMenu(orientationMenu);
    menu->addSeparator();
    menu->addAction(gotoSettings);

    menu->popup(event->globalPos());
}
} // namespace Fooyin::VuMeter

#include "moc_vumeterwidget.cpp"
