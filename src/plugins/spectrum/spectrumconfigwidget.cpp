/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "spectrumconfigwidget.h"

#include "spectrumaxisrenderer.h"
#include "spectrumcolours.h"
#include "spectrumsettings.h"

#include <gui/framerate.h>
#include <gui/guiutils.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/gradienteditor.h>

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin::Spectrum {
namespace {
QFont fontFromString(const QString& fontString)
{
    QFont font;
    if(!fontString.isEmpty()) {
        font.fromString(fontString);
    }
    return font;
}

QString formatMidiNote(int midiNote)
{
    static constexpr std::array<const char*, 12> NoteNames
        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int semitone = ((midiNote % 12) + 12) % 12;
    const int octave   = midiNote / 12;
    return QString::fromLatin1(NoteNames[semitone]) + QString::number(octave);
}

std::optional<int> parseMidiNote(const QString& text)
{
    const QString trimmed = text.trimmed().toUpper();

    static constexpr std::array<const char*, 12> NoteNames
        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    for(int semitone = 0; std::cmp_less(semitone, NoteNames.size()); ++semitone) {
        const QString note = QString::fromLatin1(NoteNames[semitone]);
        if(!trimmed.startsWith(note)) {
            continue;
        }

        bool ok          = false;
        const int octave = trimmed.mid(note.size()).toInt(&ok);
        if(ok) {
            return (octave * 12) + semitone;
        }
    }

    return {};
}

class NoteSpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit NoteSpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent)
    { }

protected:
    [[nodiscard]] QString textFromValue(int value) const override
    {
        return formatMidiNote(value);
    }

    [[nodiscard]] int valueFromText(const QString& text) const override
    {
        return parseMidiNote(text).value_or(value());
    }

    QValidator::State validate(QString& input, int& /*pos*/) const override
    {
        if(input.trimmed().isEmpty()) {
            return QValidator::Intermediate;
        }
        return parseMidiNote(input).has_value() ? QValidator::Acceptable : QValidator::Intermediate;
    }
};

void addGridRow(QGridLayout* layout, int& row, const QString& label, QWidget* widget, QWidget* parent,
                Qt::Alignment align = {})
{
    auto* labelWidget = new QLabel(label, parent);
    labelWidget->setToolTip(widget->toolTip());
    layout->addWidget(labelWidget, row, 0, align);
    layout->addWidget(widget, row++, 1);
    layout->setColumnStretch(1, 1);
}

void addGridRow(QGridLayout* layout, int row, int column, const QString& label, QWidget* widget, QWidget* parent,
                Qt::Alignment align = {})
{
    auto* labelWidget = new QLabel(label, parent);
    labelWidget->setToolTip(widget->toolTip());
    layout->addWidget(labelWidget, row, column, align);
    layout->addWidget(widget, row, column + 1);
    layout->setColumnStretch(column + 1, 1);
}

void addGridSection(QGridLayout* layout, int row, int column, const QString& text, QWidget* parent)
{
    layout->addWidget(Gui::createSectionHeader(text, parent), row, column, 1, 2);
}
} // namespace

SpectrumConfigDialog::SpectrumConfigDialog(SpectrumWidget* spectrum, QWidget* parent)
    : WidgetConfigDialog{spectrum, tr("Spectrum Settings"), parent}
    , m_bandCount{new QSpinBox(this)}
    , m_barSpacing{new QSpinBox(this)}
    , m_barSections{new QSpinBox(this)}
    , m_sectionSpacing{new QSpinBox(this)}
    , m_labelMode{new QComboBox(this)}
    , m_minFrequency{new QSpinBox(this)}
    , m_maxFrequency{new QSpinBox(this)}
    , m_minNote{new NoteSpinBox(this)}
    , m_maxNote{new NoteSpinBox(this)}
    , m_pitchHz{new QSpinBox(this)}
    , m_transpose{new QSpinBox(this)}
    , m_amplitudesGroup{new QGroupBox(tr("Amplitudes"), this)}
    , m_amplitudeMinDb{new QSpinBox(this)}
    , m_amplitudeMaxDb{new QSpinBox(this)}
    , m_amplitudeHoldTime{new QSpinBox(this)}
    , m_decayTime{new QSpinBox(this)}
    , m_peaksGroup{new QGroupBox(tr("Peaks"), this)}
    , m_peakHoldTime{new QSpinBox(this)}
    , m_peakGravity{new QSpinBox(this)}
    , m_updateFps{new QComboBox(this)}
    , m_fftSize{new QComboBox(this)}
    , m_windowFunction{new QComboBox(this)}
    , m_drawStyle{new QComboBox(this)}
    , m_showTopLabels{new QCheckBox(tr("Top labels"), this)}
    , m_showBottomLabels{new QCheckBox(tr("Bottom labels"), this)}
    , m_showLeftLabels{new QCheckBox(tr("Left labels"), this)}
    , m_showRightLabels{new QCheckBox(tr("Right labels"), this)}
    , m_showHorizontalGrid{new QCheckBox(tr("Horizontal gridlines"), this)}
    , m_showVerticalGrid{new QCheckBox(tr("Vertical gridlines"), this)}
    , m_showWhiteKeys{new QCheckBox(tr("White keys"), this)}
    , m_showBlackKeys{new QCheckBox(tr("Black keys"), this)}
    , m_showTooltip{new QCheckBox(tr("Tooltip"), this)}
    , m_fillSpectrum{new QCheckBox(tr("Fill spectrum"), this)}
    , m_interpolate{new QCheckBox(tr("Interpolate"), this)}
    , m_axisFont{new FontButton(this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_textColour{new ColourButton(this)}
    , m_backgroundColour{new ColourButton(this)}
    , m_barGradient{new GradientEditor(this)}
    , m_peaksColour{new ColourButton(this)}
    , m_horizontalGridColour{new ColourButton(this)}
    , m_verticalGridColour{new ColourButton(this)}
    , m_octaveGridColour{new ColourButton(this)}
    , m_whiteKeyColour{new ColourButton(this)}
    , m_blackKeyColour{new ColourButton(this)}
{
    auto* spectrumGroup    = new QGroupBox(tr("Scale"), this);
    auto* spectrumLayout   = new QGridLayout(spectrumGroup);
    auto* analysisGroup    = new QGroupBox(tr("Analysis"), this);
    auto* analysisLayout   = new QGridLayout(analysisGroup);
    auto* amplitudesLayout = new QGridLayout(m_amplitudesGroup);
    auto* peaksLayout      = new QGridLayout(m_peaksGroup);
    auto* axesGroup        = new QGroupBox(tr("Axes"), this);
    auto* axesLayout       = new QVBoxLayout(axesGroup);
    auto* displayGroup     = new QGroupBox(tr("Display"), this);
    auto* displayLayout    = new QVBoxLayout(displayGroup);

    m_bandCount->setRange(MinBandCount, MaxBandCount);
    m_bandCount->setSingleStep(10);
    m_barSpacing->setRange(MinBarSpacing, MaxBarSpacing);
    m_barSpacing->setSuffix(u" px"_s);
    m_barSections->setRange(MinBarSections, MaxBarSections);
    m_sectionSpacing->setRange(MinSectionSpacing, MaxSectionSpacing);
    m_sectionSpacing->setSuffix(u" px"_s);
    m_minFrequency->setRange(MinFrequencyHz, MaxFrequencyHz);
    m_minFrequency->setSingleStep(10);
    m_minFrequency->setSuffix(u" Hz"_s);
    m_maxFrequency->setRange(MinFrequencyHz, MaxFrequencyHz);
    m_maxFrequency->setSingleStep(1000);
    m_maxFrequency->setSuffix(u" Hz"_s);
    m_minNote->setRange(MinNote, MaxNote - 12);
    m_maxNote->setRange(MinNote + 12, MaxNote);
    m_pitchHz->setRange(MinPitchHz, MaxPitchHz);
    m_pitchHz->setSuffix(u" Hz"_s);
    m_transpose->setRange(MinTranspose, MaxTranspose);
    m_transpose->setSuffix(u" st"_s);
    m_amplitudeMinDb->setRange(MinAmplitudeDb, MaxAmplitudeDb);
    m_amplitudeMinDb->setSuffix(u" dB"_s);
    m_amplitudeMaxDb->setRange(MinAmplitudeDb, MaxAmplitudeDb);
    m_amplitudeMaxDb->setSuffix(u" dB"_s);
    m_amplitudeHoldTime->setRange(MinAmplitudeHoldTimeMs, MaxAmplitudeHoldTimeMs);
    m_amplitudeHoldTime->setSingleStep(100);
    m_amplitudeHoldTime->setSuffix(u" ms"_s);
    m_decayTime->setRange(MinAmplitudeGravity, MaxAmplitudeGravity);
    m_decayTime->setSingleStep(10);
    m_decayTime->setSuffix(u" dB/s"_s);
    m_peakHoldTime->setRange(MinPeakHoldTimeMs, MaxPeakHoldTimeMs);
    m_peakHoldTime->setSingleStep(100);
    m_peakHoldTime->setSuffix(u" ms"_s);
    m_peakGravity->setRange(MinPeakGravity, MaxPeakGravity);
    m_peakGravity->setSingleStep(10);
    m_peakGravity->setSuffix(u" dB/s"_s);

    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_updateFps->addItem(tr("%1 fps").arg(fps), fps);
    }

    for(const int fftSize : {256, 512, 1024, 2048, 4096, 8192, 16384}) {
        m_fftSize->addItem(QString::number(fftSize), fftSize);
    }

    m_windowFunction->addItem(tr("Blackman-Harris"), static_cast<int>(WindowFunction::BlackmanHarris));
    m_windowFunction->addItem(tr("Hann"), static_cast<int>(WindowFunction::Hann));
    m_windowFunction->addItem(tr("None"), static_cast<int>(WindowFunction::None));

    m_labelMode->addItem(tr("Frequencies"), static_cast<int>(LabelMode::Frequency));
    m_labelMode->addItem(tr("Notes"), static_cast<int>(LabelMode::Notes));
    m_drawStyle->addItem(tr("Bars"), static_cast<int>(DrawStyle::Bars));
    m_drawStyle->addItem(tr("Curve"), static_cast<int>(DrawStyle::Curve));

    m_labelMode->setToolTip(tr("Choose whether bands are spaced by frequency or by musical note"));
    m_minFrequency->setToolTip(tr("Lowest frequency shown"));
    m_maxFrequency->setToolTip(tr("Highest frequency"));
    m_minNote->setToolTip(tr("Lowest note shown"));
    m_maxNote->setToolTip(tr("Highest note shown"));
    m_amplitudeMinDb->setToolTip(tr("Signal level mapped to the bottom of the spectrum"));
    m_amplitudeMaxDb->setToolTip(tr("Signal level mapped to the top of the spectrum"));
    m_bandCount->setToolTip(tr("Number of frequency bands to draw"));
    m_fftSize->setToolTip(tr("Number of samples analysed per spectrum frame; higher values improve frequency "
                             "detail but respond more slowly"));
    m_windowFunction->setToolTip(tr("Window applied before FFT analysis"));
    m_pitchHz->setToolTip(tr("Reference frequency for A4"));
    m_transpose->setToolTip(tr("Shift note labels and note-based bands by semitones"));
    m_amplitudesGroup->setToolTip(tr("Enable smoothing for falling bar levels"));
    m_amplitudeHoldTime->setToolTip(tr("How long a raised bar level is held before it starts falling"));
    m_decayTime->setToolTip(tr("How quickly bar levels fall after the hold time"));
    m_peaksGroup->setToolTip(tr("Show peak markers above the current bar levels"));
    m_peakHoldTime->setToolTip(tr("How long each peak marker is held before it starts falling"));
    m_peakGravity->setToolTip(tr("How quickly peak markers fall after the hold time"));
    m_updateFps->setToolTip(tr("Maximum spectrum refresh rate"));
    m_showWhiteKeys->setToolTip(tr("Highlight white piano-key note ranges behind the spectrum"));
    m_showBlackKeys->setToolTip(tr("Highlight black piano-key note ranges behind the spectrum"));
    m_showTooltip->setToolTip(tr("Show frequency or note and level when hovering over the spectrum"));
    m_fillSpectrum->setToolTip(tr("Fill the spectrum area instead of drawing only the outline"));
    m_interpolate->setToolTip(tr("Smooth low-frequency bands when several bands map to the same FFT bin"));
    m_barSpacing->setToolTip(tr("Horizontal gap between adjacent bars"));
    m_barSections->setToolTip(tr("Split each bar into this many vertical segments"));
    m_sectionSpacing->setToolTip(tr("Vertical gap between bar segments"));
    m_drawStyle->setToolTip(tr("Choose whether to draw separate bars or a continuous curve"));
    m_axisFont->setToolTip(tr("Font used for spectrum axis labels"));
    m_barGradient->setToolTip(tr("Colour gradient used for bars and filled curves"));
    m_peaksColour->setToolTip(tr("Colour used for peak markers"));
    m_octaveGridColour->setToolTip(tr("Colour used for octave gridlines in note mode"));

    for(QAbstractSpinBox* spinBox :
        {m_minFrequency, m_maxFrequency, m_minNote, m_maxNote, m_amplitudeMinDb, m_amplitudeMaxDb}) {
        spinBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    for(ColourButton* button : {m_textColour, m_backgroundColour, m_peaksColour, m_horizontalGridColour,
                                m_verticalGridColour, m_octaveGridColour, m_whiteKeyColour, m_blackKeyColour}) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_barGradient->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* frequencyRange  = new QWidget(this);
    auto* frequencyLayout = new QHBoxLayout(frequencyRange);
    frequencyRange->setToolTip(m_minFrequency->toolTip());
    frequencyLayout->addWidget(m_minFrequency);
    frequencyLayout->addWidget(new QLabel(u"-"_s, this));
    frequencyLayout->addWidget(m_maxFrequency);
    frequencyLayout->addStretch(1);

    auto* noteRange  = new QWidget(this);
    auto* noteLayout = new QHBoxLayout(noteRange);
    noteRange->setToolTip(m_minNote->toolTip());
    noteLayout->addWidget(m_minNote);
    noteLayout->addWidget(new QLabel(u"-"_s, this));
    noteLayout->addWidget(m_maxNote);
    noteLayout->addStretch(1);

    auto* scaleStack = new QStackedWidget(this);
    scaleStack->addWidget(frequencyRange);
    scaleStack->addWidget(noteRange);
    scaleStack->setToolTip(tr("Frequency or note range covered by the spectrum"));
    scaleStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* amplitudeRange  = new QWidget(this);
    auto* amplitudeLayout = new QHBoxLayout(amplitudeRange);
    amplitudeRange->setToolTip(tr("Decibel range mapped to the spectrum height"));

    amplitudeLayout->addWidget(m_amplitudeMinDb);
    amplitudeLayout->addWidget(new QLabel(u"-"_s, this));
    amplitudeLayout->addWidget(m_amplitudeMaxDb);
    amplitudeLayout->addStretch(1);

    int row{0};
    addGridRow(spectrumLayout, row, tr("Axis") + ":"_L1, m_labelMode, this);
    addGridRow(spectrumLayout, row, tr("Range") + ":"_L1, scaleStack, this);
    addGridRow(spectrumLayout, row, tr("Amplitude") + ":"_L1, amplitudeRange, this);
    addGridRow(spectrumLayout, row, tr("Bands") + ":"_L1, m_bandCount, this);

    row = 0;
    addGridRow(analysisLayout, row, tr("FFT size") + ":"_L1, m_fftSize, this);
    addGridRow(analysisLayout, row, tr("Window") + ":"_L1, m_windowFunction, this);
    addGridRow(analysisLayout, row, tr("Pitch (A4)") + ":"_L1, m_pitchHz, this);
    addGridRow(analysisLayout, row, tr("Transpose") + ":"_L1, m_transpose, this);

    const auto updateScaleControls = [this, scaleStack]() {
        const auto mode  = static_cast<LabelMode>(m_labelMode->currentData().toInt());
        const bool notes = mode == LabelMode::Notes;
        scaleStack->setCurrentIndex(notes ? 1 : 0);
        m_bandCount->setEnabled(!notes);
        m_pitchHz->setEnabled(notes);
        m_transpose->setEnabled(notes);
        m_showWhiteKeys->setEnabled(notes);
        m_showBlackKeys->setEnabled(notes);
    };

    QObject::connect(m_labelMode, &QComboBox::currentIndexChanged, this, updateScaleControls);
    QObject::connect(m_minNote, qOverload<int>(&QSpinBox::valueChanged), this,
                     [this](int value) { m_maxNote->setMinimum(std::min(MaxNote, value + 12)); });
    QObject::connect(m_maxNote, qOverload<int>(&QSpinBox::valueChanged), this,
                     [this](int value) { m_minNote->setMaximum(std::max(MinNote, value - 12)); });
    m_amplitudesGroup->setCheckable(true);

    row = 0;
    addGridRow(amplitudesLayout, row, tr("Hold time") + ":"_L1, m_amplitudeHoldTime, this);
    addGridRow(amplitudesLayout, row, tr("Falloff") + ":"_L1, m_decayTime, this);

    m_peaksGroup->setCheckable(true);
    row = 0;
    addGridRow(peaksLayout, row, tr("Hold time") + ":"_L1, m_peakHoldTime, this);
    addGridRow(peaksLayout, row, tr("Falloff") + ":"_L1, m_peakGravity, this);

    axesLayout->addWidget(m_showTopLabels);
    axesLayout->addWidget(m_showBottomLabels);
    axesLayout->addWidget(m_showLeftLabels);
    axesLayout->addWidget(m_showRightLabels);
    axesLayout->addWidget(m_showHorizontalGrid);
    axesLayout->addWidget(m_showVerticalGrid);
    axesLayout->addWidget(m_showWhiteKeys);
    axesLayout->addWidget(m_showBlackKeys);

    displayLayout->addWidget(m_showTooltip);
    displayLayout->addWidget(m_fillSpectrum);
    displayLayout->addWidget(m_interpolate);

    auto* barLayout = new QGridLayout();
    row             = 0;
    addGridRow(barLayout, row, tr("Update FPS") + ":"_L1, m_updateFps, this);
    addGridRow(barLayout, row, tr("Bar spacing") + ":"_L1, m_barSpacing, this);
    addGridRow(barLayout, row, tr("Sections") + ":"_L1, m_barSections, this);
    addGridRow(barLayout, row, tr("Section spacing") + ":"_L1, m_sectionSpacing, this);
    addGridRow(barLayout, row, tr("Style") + ":"_L1, m_drawStyle, this);
    addGridRow(barLayout, row, tr("Axis font") + ":"_L1, m_axisFont, this);
    displayLayout->addLayout(barLayout);

    m_colourGroup->setTitle(tr("Colours"));
    m_colourGroup->setCheckable(true);

    auto* coloursLayout      = new QGridLayout(m_colourGroup);
    auto* leftColoursColumn  = new QWidget(m_colourGroup);
    auto* rightColoursColumn = new QWidget(m_colourGroup);
    auto* leftColoursLayout  = new QGridLayout(leftColoursColumn);
    auto* rightColoursLayout = new QGridLayout(rightColoursColumn);
    leftColoursLayout->setContentsMargins({});
    rightColoursLayout->setContentsMargins({});

    addGridSection(leftColoursLayout, 0, 0, tr("General"), this);
    addGridRow(leftColoursLayout, 1, 0, tr("Text colour") + ":"_L1, m_textColour, this);
    addGridRow(leftColoursLayout, 2, 0, tr("Background colour") + ":"_L1, m_backgroundColour, this);
    addGridSection(leftColoursLayout, 3, 0, tr("Grid"), this);
    addGridRow(leftColoursLayout, 4, 0, tr("Horizontal gridlines") + ":"_L1, m_horizontalGridColour, this);
    addGridRow(leftColoursLayout, 5, 0, tr("Vertical gridlines") + ":"_L1, m_verticalGridColour, this);
    addGridRow(leftColoursLayout, 6, 0, tr("Octave gridlines") + ":"_L1, m_octaveGridColour, this);
    addGridSection(leftColoursLayout, 7, 0, tr("Keys"), this);
    addGridRow(leftColoursLayout, 8, 0, tr("White keys") + ":"_L1, m_whiteKeyColour, this);
    addGridRow(leftColoursLayout, 9, 0, tr("Black keys") + ":"_L1, m_blackKeyColour, this);

    addGridSection(rightColoursLayout, 0, 0, tr("Bars"), this);
    addGridRow(rightColoursLayout, 1, 0, tr("Bar gradient") + ":"_L1, m_barGradient, this, Qt::AlignTop);
    addGridRow(rightColoursLayout, 2, 0, tr("Peaks colour") + ":"_L1, m_peaksColour, this);
    rightColoursLayout->setRowStretch(1, 1);

    const int coloursColumnWidth
        = std::max(leftColoursColumn->sizeHint().width(), rightColoursColumn->sizeHint().width());
    leftColoursColumn->setMinimumWidth(coloursColumnWidth);
    rightColoursColumn->setMinimumWidth(coloursColumnWidth);
    coloursLayout->addWidget(leftColoursColumn, 0, 0);
    coloursLayout->addWidget(rightColoursColumn, 0, 1);
    coloursLayout->setColumnStretch(0, 1);
    coloursLayout->setColumnStretch(1, 1);

    auto* spectrumPage       = new QWidget(this);
    auto* spectrumPageLayout = new QGridLayout(spectrumPage);

    row = 0;
    spectrumPageLayout->addWidget(spectrumGroup, row, 0);
    spectrumPageLayout->addWidget(analysisGroup, row++, 1);
    spectrumPageLayout->addWidget(m_amplitudesGroup, row, 0);
    spectrumPageLayout->addWidget(m_peaksGroup, row++, 1);
    spectrumPageLayout->setRowStretch(spectrumPageLayout->rowCount(), 1);

    auto* appearancePage       = new QWidget(this);
    auto* appearancePageLayout = new QGridLayout(appearancePage);
    appearancePageLayout->addWidget(axesGroup, 0, 0);
    appearancePageLayout->addWidget(displayGroup, 0, 1);
    appearancePageLayout->setRowStretch(1, 1);

    auto* coloursPage       = new QWidget(this);
    auto* coloursPageLayout = new QGridLayout(coloursPage);
    coloursPageLayout->addWidget(m_colourGroup, 0, 0);
    coloursPageLayout->setRowStretch(1, 1);

    const int tabColumnWidth = std::max({spectrumGroup->sizeHint().width(), analysisGroup->sizeHint().width(),
                                         axesGroup->sizeHint().width(), displayGroup->sizeHint().width()});
    for(QWidget* group : {spectrumGroup, analysisGroup, m_peaksGroup, m_amplitudesGroup, axesGroup, displayGroup}) {
        group->setMinimumWidth(tabColumnWidth);
    }
    spectrumGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    analysisGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    axesGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    displayGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_colourGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(spectrumPage, tr("General"));
    tabs->addTab(appearancePage, tr("Display"));
    tabs->addTab(coloursPage, tr("Colours"));

    auto* layout{contentLayout()};
    layout->addWidget(tabs, 0, 0);
    layout->setColumnStretch(0, 1);

    loadCurrentConfig();
}

SpectrumWidget::ConfigData SpectrumConfigDialog::config() const
{
    const QFont axisFont        = m_axisFont->buttonFont();
    const QFont defaultAxisFont = SpectrumAxisRenderer::defaultAxisFont(widget()->font());

    SpectrumWidget::ConfigData config{
        .bandCount           = m_bandCount->value(),
        .barSpacing          = m_barSpacing->value(),
        .barSections         = m_barSections->value(),
        .sectionSpacing      = m_sectionSpacing->value(),
        .minFrequencyHz      = m_minFrequency->value(),
        .maxFrequencyHz      = m_maxFrequency->value(),
        .minNote             = m_minNote->value(),
        .maxNote             = m_maxNote->value(),
        .pitchHz             = m_pitchHz->value(),
        .transpose           = m_transpose->value(),
        .amplitudeMinDb      = m_amplitudeMinDb->value(),
        .amplitudeMaxDb      = m_amplitudeMaxDb->value(),
        .amplitudesEnabled   = m_amplitudesGroup->isChecked(),
        .amplitudeHoldTimeMs = m_amplitudeHoldTime->value(),
        .amplitudeGravity    = m_decayTime->value(),
        .peaksEnabled        = m_peaksGroup->isChecked(),
        .peakHoldTimeMs      = m_peakHoldTime->value(),
        .peakGravity         = m_peakGravity->value(),
        .updateFps           = m_updateFps->currentData().toInt(),
        .fftSize             = m_fftSize->currentData().toInt(),
        .windowFunction      = static_cast<WindowFunction>(m_windowFunction->currentData().toInt()),
        .gradientOrientation = m_barGradient->orientation() == Qt::Vertical ? GradientOrientation::Vertical
                                                                            : GradientOrientation::Horizontal,
        .labelMode           = static_cast<LabelMode>(m_labelMode->currentData().toInt()),
        .drawStyle           = static_cast<DrawStyle>(m_drawStyle->currentData().toInt()),
        .showTopLabels       = m_showTopLabels->isChecked(),
        .showBottomLabels    = m_showBottomLabels->isChecked(),
        .showLeftLabels      = m_showLeftLabels->isChecked(),
        .showRightLabels     = m_showRightLabels->isChecked(),
        .showHorizontalGrid  = m_showHorizontalGrid->isChecked(),
        .showVerticalGrid    = m_showVerticalGrid->isChecked(),
        .showWhiteKeys       = m_showWhiteKeys->isChecked(),
        .showBlackKeys       = m_showBlackKeys->isChecked(),
        .showTooltip         = m_showTooltip->isChecked(),
        .fillSpectrum        = m_fillSpectrum->isChecked(),
        .interpolate         = m_interpolate->isChecked(),
        .axisFont            = axisFont == defaultAxisFont ? QString{} : axisFont.toString(),
        .colours             = QVariant{},
    };

    if(m_colourGroup->isChecked()) {
        Colours colours;
        colours.setColour(Colours::Type::Text, m_textColour->colour());
        colours.setColour(Colours::Type::Background, m_backgroundColour->colour());
        colours.setGradient(m_barGradient->colours());
        colours.setColour(Colours::Type::Peaks, m_peaksColour->colour());
        colours.setColour(Colours::Type::HorizontalGrid, m_horizontalGridColour->colour());
        colours.setColour(Colours::Type::VerticalGrid, m_verticalGridColour->colour());
        colours.setColour(Colours::Type::OctaveGrid, m_octaveGridColour->colour());
        colours.setColour(Colours::Type::WhiteKey, m_whiteKeyColour->colour());
        colours.setColour(Colours::Type::BlackKey, m_blackKeyColour->colour());
        config.colours = QVariant::fromValue(colours);
    }

    return config;
}

void SpectrumConfigDialog::setConfig(const SpectrumWidget::ConfigData& config)
{
    m_bandCount->setValue(config.bandCount);
    m_barSpacing->setValue(config.barSpacing);
    m_barSections->setValue(config.barSections);
    m_sectionSpacing->setValue(config.sectionSpacing);
    m_minFrequency->setValue(config.minFrequencyHz);
    m_maxFrequency->setValue(config.maxFrequencyHz);
    m_minNote->setValue(config.minNote);
    m_maxNote->setValue(config.maxNote);
    m_maxNote->setMinimum(std::min(MaxNote, config.minNote + 12));
    m_minNote->setMaximum(std::max(MinNote, config.maxNote - 12));
    m_pitchHz->setValue(config.pitchHz);
    m_transpose->setValue(config.transpose);
    m_amplitudeMinDb->setValue(config.amplitudeMinDb);
    m_amplitudeMaxDb->setValue(config.amplitudeMaxDb);
    m_amplitudesGroup->setChecked(config.amplitudesEnabled);
    m_amplitudeHoldTime->setValue(config.amplitudeHoldTimeMs);
    m_decayTime->setValue(config.amplitudeGravity);
    m_peaksGroup->setChecked(config.peaksEnabled);
    m_peakHoldTime->setValue(config.peakHoldTimeMs);
    m_peakGravity->setValue(config.peakGravity);

    const int nearestFps = Gui::FrameRate::nearestPresetFps(config.updateFps);
    int fpsIndex         = m_updateFps->findData(nearestFps);
    if(fpsIndex < 0) {
        fpsIndex = m_updateFps->findData(DefaultUpdateFps);
    }
    m_updateFps->setCurrentIndex(fpsIndex);

    int fftIndex = m_fftSize->findData(config.fftSize);
    if(fftIndex < 0) {
        fftIndex = m_fftSize->findData(DefaultFftSize);
    }
    m_fftSize->setCurrentIndex(fftIndex);

    int windowIndex = m_windowFunction->findData(static_cast<int>(config.windowFunction));
    if(windowIndex < 0) {
        windowIndex = m_windowFunction->findData(static_cast<int>(WindowFunction::Hann));
    }
    m_windowFunction->setCurrentIndex(windowIndex);

    int drawStyleIndex = m_drawStyle->findData(static_cast<int>(config.drawStyle));
    if(drawStyleIndex < 0) {
        drawStyleIndex = m_drawStyle->findData(static_cast<int>(DrawStyle::Bars));
    }
    m_drawStyle->setCurrentIndex(drawStyleIndex);

    int labelModeIndex = m_labelMode->findData(static_cast<int>(config.labelMode));
    if(labelModeIndex < 0) {
        labelModeIndex = m_labelMode->findData(static_cast<int>(LabelMode::Frequency));
    }
    m_labelMode->setCurrentIndex(labelModeIndex);

    const auto mode = static_cast<LabelMode>(m_labelMode->currentData().toInt());
    m_bandCount->setEnabled(mode != LabelMode::Notes);
    m_pitchHz->setEnabled(mode == LabelMode::Notes);
    m_transpose->setEnabled(mode == LabelMode::Notes);
    m_showWhiteKeys->setEnabled(mode == LabelMode::Notes);
    m_showBlackKeys->setEnabled(mode == LabelMode::Notes);
    m_showTopLabels->setChecked(config.showTopLabels);
    m_showBottomLabels->setChecked(config.showBottomLabels);
    m_showLeftLabels->setChecked(config.showLeftLabels);
    m_showRightLabels->setChecked(config.showRightLabels);
    m_showHorizontalGrid->setChecked(config.showHorizontalGrid);
    m_showVerticalGrid->setChecked(config.showVerticalGrid);
    m_showWhiteKeys->setChecked(config.showWhiteKeys);
    m_showBlackKeys->setChecked(config.showBlackKeys);
    m_showTooltip->setChecked(config.showTooltip);
    m_fillSpectrum->setChecked(config.fillSpectrum);
    m_interpolate->setChecked(config.interpolate);
    m_axisFont->setButtonFont(config.axisFont.isEmpty() ? SpectrumAxisRenderer::defaultAxisFont(widget()->font())
                                                        : fontFromString(config.axisFont));

    const bool customColours = config.colours.isValid() && config.colours.canConvert<Colours>()
                            && !config.colours.value<Colours>().isEmpty();
    const Colours colours    = customColours ? config.colours.value<Colours>() : Colours{};

    m_colourGroup->setChecked(customColours);
    m_textColour->setColour(colours.colour(Colours::Type::Text, widget()->palette()));
    m_backgroundColour->setColour(colours.colour(Colours::Type::Background, widget()->palette()));
    m_barGradient->setColours(colours.gradient(widget()->palette()));
    m_barGradient->setOrientation(config.gradientOrientation == GradientOrientation::Vertical ? Qt::Vertical
                                                                                              : Qt::Horizontal);
    m_peaksColour->setColour(colours.colour(Colours::Type::Peaks, widget()->palette()));
    m_horizontalGridColour->setColour(colours.colour(Colours::Type::HorizontalGrid, widget()->palette()));
    m_verticalGridColour->setColour(colours.colour(Colours::Type::VerticalGrid, widget()->palette()));
    m_octaveGridColour->setColour(colours.colour(Colours::Type::OctaveGrid, widget()->palette()));
    m_whiteKeyColour->setColour(colours.colour(Colours::Type::WhiteKey, widget()->palette()));
    m_blackKeyColour->setColour(colours.colour(Colours::Type::BlackKey, widget()->palette()));
}
} // namespace Fooyin::Spectrum

#include "moc_spectrumconfigwidget.cpp"
#include "spectrumconfigwidget.moc"
