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

#include "equalisersettingswidget.h"

#include "equaliserpresetstore.h"
#include "equalisersliderstate.h"

#include <core/coresettings.h>
#include <gui/widgets/tooltip.h>

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpacerItem>
#include <QStyle>
#include <QTextStream>
#include <QTimerEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace Qt::StringLiterals;

constexpr auto LastPresetPathKey = "DSP/EqualiserLastPresetPath";

namespace Fooyin::Equaliser {
namespace {
constexpr std::array<const char*, 18> BandLabels = {
    "55",   "77",   "110",  "156",  "220", "311", "440", "622", "880",
    "1.2K", "1.8K", "2.5K", "3.5K", "5K",  "7K",  "10K", "14K", "20K",
};

QSlider* makeGainSlider(QWidget* parent)
{
    auto* slider = new QSlider(Qt::Vertical, parent);

    slider->setRange(-20 * Fooyin::Equaliser::EqualiserSliderScale, 20 * Fooyin::Equaliser::EqualiserSliderScale);
    slider->setSingleStep(1);
    slider->setPageStep(5);
    slider->setTickInterval(50);
    slider->setTickPosition(QSlider::TicksLeft);
    slider->setMinimumHeight(220);
    slider->setMaximumHeight(220);

    return slider;
}

QSlider* makeCompactGainSlider(QWidget* parent)
{
    auto* slider = makeGainSlider(parent);
    slider->setMinimumHeight(80);
    slider->setMaximumHeight(QWIDGETSIZE_MAX);
    slider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return slider;
}

QLabel* makeBandLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);

    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    return label;
}

QLabel* makeValueLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);

    label->setText(u"-20.0"_s);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    return label;
}

QString gainTooltip(const double gainDb)
{
    const QString prefix = gainDb >= 0.0 ? u"+"_s : QString{};
    return prefix + QString::number(gainDb, 'f', 1) + u" dB"_s;
}

void showSliderToolTip(QWidget* owner, QPointer<ToolTip>& toolTip, QSlider* slider)
{
    if(!toolTip) {
        toolTip = new ToolTip(owner);
    }

    toolTip->setText(gainTooltip(sliderValueToGain(slider->value())));
    toolTip->show();
    toolTip->raise();

    const QPoint cursorPos = slider->mapFromGlobal(QCursor::pos());
    const int handleY      = std::clamp(cursorPos.y(), slider->rect().top(), slider->rect().bottom());
    const QPoint handlePos = slider->mapTo(owner, QPoint(slider->rect().center().x(), handleY));

    const int tipHeight = toolTip->height();
    const int tipWidth  = toolTip->width();
    const int anchorY   = std::clamp(handlePos.y() + (tipHeight / 2), tipHeight, owner->height());

    const int rightX = slider->mapTo(owner, QPoint{slider->rect().right() + 6, 0}).x();
    const int leftX  = slider->mapTo(owner, QPoint{slider->rect().left() - 6, 0}).x();

    if(rightX + tipWidth <= owner->width()) {
        toolTip->setPosition(QPoint{rightX, anchorY}, Qt::AlignLeft);
    }
    else {
        toolTip->setPosition(QPoint{leftX, anchorY}, Qt::AlignRight);
    }
}

void hideSliderToolTip(const QPointer<ToolTip>& toolTip)
{
    if(toolTip) {
        toolTip->hide();
    }
}

QStyleOptionSlider sliderStyleOption(QSlider* slider)
{
    QStyleOptionSlider sliderOpt;
    sliderOpt.initFrom(slider);
    sliderOpt.orientation    = Qt::Vertical;
    sliderOpt.minimum        = slider->minimum();
    sliderOpt.maximum        = slider->maximum();
    sliderOpt.tickPosition   = slider->tickPosition();
    sliderOpt.tickInterval   = slider->tickInterval();
    sliderOpt.upsideDown     = !slider->invertedAppearance();
    sliderOpt.direction      = slider->layoutDirection();
    sliderOpt.pageStep       = slider->pageStep();
    sliderOpt.singleStep     = slider->singleStep();
    sliderOpt.sliderPosition = slider->value();
    sliderOpt.sliderValue    = slider->value();

    return sliderOpt;
}

int sliderHandleHeight(QSlider* slider)
{
    if(!slider) {
        return 0;
    }

    const QStyleOptionSlider sliderOpt = sliderStyleOption(slider);
    const QRect handleRect
        = slider->style()->subControlRect(QStyle::CC_Slider, &sliderOpt, QStyle::SC_SliderHandle, slider);
    return std::max(0, handleRect.height());
}

int scaleHandleHalfOffset(QSlider* slider)
{
    return static_cast<int>(std::lround(static_cast<double>(sliderHandleHeight(slider)) / 2.0));
}

int scaleHandleQuarterOffset(QSlider* slider)
{
    return static_cast<int>(std::lround(static_cast<double>(sliderHandleHeight(slider)) / 4.0));
}
} // namespace

class ScaleLabelsWidget : public QWidget
{
public:
    explicit ScaleLabelsWidget(QSlider* slider, QWidget* parent = nullptr)
        : QWidget{parent}
        , m_slider{slider}
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void syncToSlider()
    {
        setFixedHeight(m_slider->height() + scaleHandleHalfOffset(m_slider));

        const QFontMetrics metrics{font()};
        const int width = std::max({metrics.horizontalAdvance(u"+20 dB"_s), metrics.horizontalAdvance(u"+0 dB"_s),
                                    metrics.horizontalAdvance(u"-20 dB"_s)});
        setFixedWidth(width);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        if(!m_slider) {
            return;
        }

        const QFontMetrics metrics{font()};

        const QString topText    = u"+20 dB"_s;
        const QString middleText = u"+0 dB"_s;
        const QString bottomText = u"-20 dB"_s;

        const QRect topRect    = metrics.tightBoundingRect(topText);
        const QRect middleRect = metrics.tightBoundingRect(middleText);
        const QRect bottomRect = metrics.tightBoundingRect(bottomText);

        const auto baselineForCentre = [&middleRect](int centreY) {
            return static_cast<int>(std::lround(
                static_cast<double>(centreY)
                - ((static_cast<double>(middleRect.top()) + static_cast<double>(middleRect.bottom())) / 2.0)));
        };

        const int trackOffset    = scaleHandleHalfOffset(m_slider);
        const int topBaseline    = scaleHandleQuarterOffset(m_slider) - topRect.top();
        const int middleBaseline = baselineForCentre(m_slider->rect().center().y() + trackOffset);
        const int bottomBaseline = (m_slider->height() - 1 + trackOffset) - bottomRect.bottom();

        QPainter painter{this};
        painter.setPen(palette().color(QPalette::WindowText));

        const auto drawRightAlignedText = [&painter, &metrics, this](const int baseline, const QString& text) {
            const int x = width() - metrics.horizontalAdvance(text);
            painter.drawText(x, baseline, text);
        };

        drawRightAlignedText(topBaseline, topText);
        drawRightAlignedText(middleBaseline, middleText);
        drawRightAlignedText(bottomBaseline, bottomText);
    }

private:
    QSlider* m_slider;
};

EqualiserLayoutEditor::EqualiserLayoutEditor(EqualiserPresetStore& presetStore, QWidget* parent)
    : DspLayoutEditor{parent}
    , m_controls{new QWidget(this)}
    , m_presetStore{presetStore}
    , m_controlRow{new QWidget(this)}
    , m_enabledToggle{new QPushButton(tr("Enabled"), m_controlRow)}
    , m_zeroLevelButton{new QPushButton(tr("Zero level"), m_controlRow)}
    , m_autoLevelButton{new QPushButton(tr("Auto level"), m_controlRow)}
    , m_presetBox{new QComboBox(m_controlRow)}
    , m_savePresetButton{new QPushButton(tr("Save preset"), m_controlRow)}
    , m_sliders{}
    , m_valueLabels{}
    , m_showControls{true}
{
    auto* row = new QHBoxLayout(m_controls);
    row->setContentsMargins(4, 4, 4, 0);
    row->setSpacing(4);

    const auto addSlider = [this, row](size_t index, const QString& labelText) {
        auto* column = new QWidget(m_controls);
        auto* layout = new QVBoxLayout(column);
        layout->setContentsMargins({});
        layout->setSpacing(3);

        auto* label      = makeBandLabel(labelText, column);
        auto* slider     = makeCompactGainSlider(column);
        auto* valueLabel = makeValueLabel(column);

        layout->addWidget(label);
        layout->addWidget(slider, 1, Qt::AlignHCenter);
        layout->addWidget(valueLabel);

        const int columnWidth
            = std::max({slider->sizeHint().width(), label->sizeHint().width(), valueLabel->sizeHint().width()});
        column->setMinimumWidth(columnWidth);
        column->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        if(index > 0) {
            row->addStretch();
        }
        row->addWidget(column);

        m_sliders[index]     = slider;
        m_valueLabels[index] = valueLabel;

        QObject::connect(slider, &QSlider::valueChanged, this, [this, index](int value) {
            m_valueLabels[index]->setText(gainText(value));
            m_sliders[index]->setToolTip(gainText(value) + tr(" dB"));
            updatePresetSelection();
            m_previewTimer.start(PreviewDebounceMs, this);
            if(m_sliders[index]->isSliderDown()) {
                showSliderToolTip(this, m_sliderToolTip, m_sliders[index]);
            }
        });
        QObject::connect(slider, &QSlider::sliderPressed, this,
                         [this, slider]() { showSliderToolTip(this, m_sliderToolTip, slider); });
        QObject::connect(slider, &QSlider::sliderMoved, this,
                         [this, slider](int) { showSliderToolTip(this, m_sliderToolTip, slider); });
        QObject::connect(slider, &QSlider::sliderReleased, this, [this]() { hideSliderToolTip(m_sliderToolTip); });
    };

    addSlider(0, tr("Preamp"));
    for(size_t i{0}; i < BandLabels.size(); ++i) {
        addSlider(i + 1, QString::fromLatin1(BandLabels[i]));
    }
    row->addStretch();

    m_controls->setMinimumWidth(row->sizeHint().width());

    auto* controlLayout = new QHBoxLayout(m_controlRow);
    controlLayout->addWidget(m_enabledToggle);
    controlLayout->addWidget(m_zeroLevelButton);
    controlLayout->addWidget(m_autoLevelButton);
    controlLayout->addWidget(m_presetBox);
    controlLayout->addWidget(m_savePresetButton);
    controlLayout->addStretch();

    m_enabledToggle->setCheckable(true);
    m_presetBox->setMinimumContentsLength(10);
    m_presetBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_presetBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(&m_presetStore, &EqualiserPresetStore::presetsChanged, this,
                     &EqualiserLayoutEditor::refreshPresets);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_controls, 1);
    layout->addWidget(m_controlRow);

    QObject::connect(m_enabledToggle, &QPushButton::toggled, this, &DspLayoutEditor::enabledStateChangeRequested);
    QObject::connect(m_zeroLevelButton, &QPushButton::clicked, this, &EqualiserLayoutEditor::zeroLevel);
    QObject::connect(m_autoLevelButton, &QPushButton::clicked, this, &EqualiserLayoutEditor::autoLevel);
    QObject::connect(m_presetBox, qOverload<int>(&QComboBox::activated), this, &EqualiserLayoutEditor::loadPreset);
    QObject::connect(m_savePresetButton, &QPushButton::clicked, this, &EqualiserLayoutEditor::savePreset);

    const int minimumHeight
        = m_controls->minimumSizeHint().height() + layout->spacing() + m_controlRow->minimumSizeHint().height();
    setMinimumSize(100, minimumHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setControlsVisible(m_showControls);
    refreshPresets();
    EqualiserLayoutEditor::restoreDefaults();
}

void EqualiserLayoutEditor::loadSettings(const QByteArray& settings)
{
    applySliderState(sliderStateFromSettings(settings).value_or(zeroSliderState()));
}

QByteArray EqualiserLayoutEditor::saveSettings() const
{
    return settingsFromSliderState(sliderState());
}

EqualiserSliderState EqualiserLayoutEditor::sliderState() const
{
    EqualiserSliderState state;
    state.preamp = m_sliders[0]->value();

    for(size_t i{0}; i < state.bands.size(); ++i) {
        state.bands[i] = m_sliders[i + 1]->value();
    }

    return state;
}

void EqualiserLayoutEditor::applySliderState(const EqualiserSliderState& state)
{
    std::vector<QSignalBlocker> blockers;
    blockers.reserve(m_sliders.size());
    for(auto* slider : m_sliders) {
        blockers.emplace_back(slider);
    }

    m_sliders[0]->setValue(state.preamp);
    for(size_t i{0}; i < state.bands.size(); ++i) {
        m_sliders[i + 1]->setValue(state.bands[i]);
    }

    updateValueLabels();
    updatePresetSelection();
}

void EqualiserLayoutEditor::setControlsEnabled(const bool enabled)
{
    const QSignalBlocker blocker{m_enabledToggle};

    m_enabledToggle->setChecked(enabled);
    m_enabledToggle->setEnabled(true);
    m_zeroLevelButton->setEnabled(enabled);
    m_autoLevelButton->setEnabled(enabled);
    m_presetBox->setEnabled(enabled && m_presetBox->count() > 0);
    m_savePresetButton->setEnabled(enabled);

    const auto labels = m_controls->findChildren<QLabel*>();
    for(auto* label : labels) {
        label->setEnabled(enabled);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    }
    for(auto* slider : m_sliders) {
        slider->setEnabled(enabled);
        slider->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    }
}

void EqualiserLayoutEditor::restoreDefaults()
{
    applySliderState(zeroSliderState());
}

QString EqualiserLayoutEditor::restoreDefaultsActionText() const
{
    return tr("Zero level");
}

void EqualiserLayoutEditor::populateContextMenu(QMenu* menu)
{
    auto* showControls = new QAction(tr("Show controls"), menu);
    showControls->setCheckable(true);
    showControls->setChecked(m_showControls);
    menu->addAction(showControls);
    QObject::connect(showControls, &QAction::triggered, this, &EqualiserLayoutEditor::setControlsVisible);

    menu->addSeparator();

    auto* autoLevelAction = menu->addAction(tr("Auto level"));
    autoLevelAction->setEnabled(m_enabledToggle->isChecked());
    QObject::connect(autoLevelAction, &QAction::triggered, this, &EqualiserLayoutEditor::autoLevel);

    auto* savePresetAction = menu->addAction(tr("Save preset…"));
    savePresetAction->setEnabled(m_enabledToggle->isChecked());
    QObject::connect(savePresetAction, &QAction::triggered, this, &EqualiserLayoutEditor::savePreset);

    auto* presetMenu = menu->addMenu(tr("Presets"));
    presetMenu->setEnabled(m_enabledToggle->isChecked());
    const auto& presets = m_presetStore.presets();
    if(presets.empty()) {
        auto* noPresets = presetMenu->addAction(tr("No presets"));
        noPresets->setEnabled(false);
    }
    else {
        for(const auto& preset : presets) {
            auto* action = presetMenu->addAction(preset.name);
            QObject::connect(action, &QAction::triggered, this, [this, settings = preset.settings]() {
                loadSettings(settings);
                m_previewTimer.start(PreviewDebounceMs, this);
            });
        }
    }
}

void EqualiserLayoutEditor::saveLayoutData(QJsonObject& layout)
{
    layout["ShowControls"_L1] = m_showControls;
}

void EqualiserLayoutEditor::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("ShowControls"_L1)) {
        setControlsVisible(layout.value("ShowControls"_L1).toBool());
    }
}

void EqualiserLayoutEditor::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        Q_EMIT previewSettingsChanged(saveSettings());
        return;
    }

    DspLayoutEditor::timerEvent(event);
}

void EqualiserLayoutEditor::setControlsVisible(const bool visible)
{
    m_showControls = visible;
    m_controlRow->setVisible(visible);

    if(auto* sliderLayout = m_controls->layout()) {
        auto margins = sliderLayout->contentsMargins();
        margins.setBottom(visible ? 0 : margins.top());
        sliderLayout->setContentsMargins(margins);
    }
}

void EqualiserLayoutEditor::updateValueLabels()
{
    for(size_t i{0}; i < m_sliders.size(); ++i) {
        const QString value = gainText(m_sliders[i]->value());
        m_valueLabels[i]->setText(value);
        m_sliders[i]->setToolTip(value + tr(" dB"));
    }
}

void EqualiserLayoutEditor::updatePresetSelection()
{
    int matchingIndex{-1};
    if(m_presetBox->count() > 0) {
        const QByteArray currentSettings = saveSettings();
        for(int i{0}; i < m_presetBox->count(); ++i) {
            if(m_presetBox->itemData(i).toByteArray() == currentSettings) {
                matchingIndex = i;
                break;
            }
        }
    }

    const QSignalBlocker blocker{m_presetBox};
    m_presetBox->setCurrentIndex(matchingIndex);
}

void EqualiserLayoutEditor::refreshPresets()
{
    const QSignalBlocker blocker{m_presetBox};
    m_presetBox->clear();

    const auto& presets = m_presetStore.presets();
    if(presets.empty()) {
        m_presetBox->setPlaceholderText(tr("No presets"));
    }
    else {
        m_presetBox->setPlaceholderText(tr("Load preset…"));
        for(const auto& preset : presets) {
            m_presetBox->addItem(preset.name, preset.settings);
        }
    }

    m_presetBox->setEnabled(m_enabledToggle->isChecked() && !presets.empty());
    updatePresetSelection();
}

void EqualiserLayoutEditor::loadPreset(const int index)
{
    if(index < 0) {
        return;
    }

    const auto state = sliderStateFromSettings(m_presetBox->itemData(index).toByteArray());
    if(!state) {
        return;
    }

    applySliderState(*state);
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserLayoutEditor::savePreset()
{
    bool accepted{false};
    const QString name
        = QInputDialog::getText(this, tr("Save preset"), tr("Preset name:"), QLineEdit::Normal, {}, &accepted)
              .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    const int existingIndex = m_presetStore.indexByName(name);
    if(existingIndex >= 0) {
        const auto answer = QMessageBox::question(this, tr("Preset already exists"),
                                                  tr("Preset \"%1\" already exists. Overwrite?").arg(name));
        if(answer != QMessageBox::Yes) {
            return;
        }
    }

    m_presetStore.setPreset(name, saveSettings());
}

void EqualiserLayoutEditor::zeroLevel()
{
    restoreDefaults();
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserLayoutEditor::autoLevel()
{
    auto state = sliderState();
    if(!autoLevelSliderState(state)) {
        return;
    }
    applySliderState(state);
    m_previewTimer.start(PreviewDebounceMs, this);
}

EqualiserSettingsWidget::EqualiserSettingsWidget(EqualiserPresetStore& presetStore, QWidget* parent)
    : DspSettingsDialog{parent}
    , m_presetBox{new QComboBox(this)}
    , m_presetStore{presetStore}
    , m_loadPresetButton{new QPushButton(tr("Load"), this)}
    , m_savePresetButton{new QPushButton(tr("Save"), this)}
    , m_deletePresetButton{new QPushButton(tr("Delete"), this)}
    , m_importPresetButton{new QPushButton(tr("Import"), this)}
    , m_exportPresetButton{new QPushButton(tr("Export"), this)}
    , m_selectedBandCombo{new QComboBox(this)}
    , m_selectedBandSpin{new QDoubleSpinBox(this)}
    , m_preampSlider{makeGainSlider(this)}
    , m_preampValueLabel{makeValueLabel(this)}
    , m_bandSliders{}
    , m_bandValueLabels{}
    , m_scaleTrackWidget{new ScaleLabelsWidget(m_preampSlider, this)}
{
    setWindowTitle(tr("Equaliser Settings"));

    setRestoreDefaultsVisible(false);

    auto* root = contentLayout();

    auto* stripWidget = new QWidget(this);
    stripWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* row = new QHBoxLayout(stripWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(7);

    std::vector<QWidget*> sliderColumns;
    std::vector<QLabel*> bandLabels;
    std::vector<QLabel*> valueLabels;
    int sliderColumnMinWidth{0};

    const auto addSliderColumn = [this, row, &sliderColumns, &bandLabels, &valueLabels, &sliderColumnMinWidth](
                                     QSlider* slider, QLabel* valueLabel, const QString& labelText) {
        auto* col = new QVBoxLayout();
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(6);
        auto* label = makeBandLabel(labelText, this);
        col->addWidget(label);
        col->addWidget(slider, 1, Qt::AlignHCenter);
        col->addWidget(valueLabel);

        auto* colWidget = new QWidget(this);
        colWidget->setLayout(col);
        colWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sliderColumnMinWidth = std::max({sliderColumnMinWidth, slider->sizeHint().width(), label->sizeHint().width(),
                                         valueLabel->sizeHint().width()});
        sliderColumns.push_back(colWidget);
        bandLabels.push_back(label);
        valueLabels.push_back(valueLabel);

        row->addWidget(colWidget, 0, Qt::AlignTop);
    };

    addSliderColumn(m_preampSlider, m_preampValueLabel, tr("Preamp"));
    connectSliderSignals(m_preampSlider, false);

    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandSliders[i]     = makeGainSlider(this);
        m_bandValueLabels[i] = makeValueLabel(this);
        addSliderColumn(m_bandSliders[i], m_bandValueLabels[i], QString::fromLatin1(BandLabels[i]));
        connectSliderSignals(m_bandSliders[i], true);
    }

    for(auto* colWidget : sliderColumns) {
        colWidget->setFixedWidth(sliderColumnMinWidth);
    }

    int bandLabelHeight{0};
    for(auto* label : bandLabels) {
        bandLabelHeight = std::max(bandLabelHeight, label->sizeHint().height());
    }
    for(auto* label : bandLabels) {
        label->setFixedHeight(bandLabelHeight);
    }

    int valueLabelHeight{0};
    for(auto* label : valueLabels) {
        valueLabelHeight = std::max(valueLabelHeight, label->sizeHint().height());
    }
    for(auto* label : valueLabels) {
        label->setFixedHeight(valueLabelHeight);
    }

    auto* scaleCol = new QVBoxLayout();
    scaleCol->setContentsMargins(0, 0, 0, 0);
    scaleCol->setSpacing(6);

    scaleCol->addSpacerItem(new QSpacerItem(0, bandLabelHeight, QSizePolicy::Minimum, QSizePolicy::Fixed));

    m_scaleTrackWidget->syncToSlider();

    scaleCol->addWidget(m_scaleTrackWidget, 0, Qt::AlignTop | Qt::AlignRight);
    scaleCol->addSpacerItem(new QSpacerItem(0, valueLabelHeight, QSizePolicy::Minimum, QSizePolicy::Fixed));

    row->addSpacing(4);
    row->addLayout(scaleCol);
    root->addWidget(stripWidget, 0, Qt::AlignHCenter | Qt::AlignTop);

    auto* controlsWidget = new QWidget(this);
    controlsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(6);

    auto* zeroButton      = new QPushButton(tr("Zero all"), this);
    auto* autoButton      = new QPushButton(tr("Auto level"), this);
    auto* bandEditorLabel = new QLabel(tr("Band:"), this);
    auto* presetsLabel    = new QLabel(tr("Presets:"), this);

    QObject::connect(zeroButton, &QPushButton::clicked, this, [this]() { zeroAll(); });
    QObject::connect(autoButton, &QPushButton::clicked, this, [this]() { autoLevel(); });

    for(const auto& bandName : BandLabels) {
        m_selectedBandCombo->addItem(QString::fromLatin1(bandName));
    }

    m_selectedBandSpin->setRange(-20.0, 20.0);
    m_selectedBandSpin->setSingleStep(0.1);
    m_selectedBandSpin->setDecimals(1);
    m_selectedBandSpin->setSuffix(tr(" dB"));

    m_presetBox->setEditable(true);
    m_presetBox->setMinimumContentsLength(18);
    m_presetBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_presetBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    controlsLayout->addWidget(zeroButton);
    controlsLayout->addWidget(autoButton);
    controlsLayout->addWidget(bandEditorLabel);
    controlsLayout->addWidget(m_selectedBandCombo);
    controlsLayout->addWidget(m_selectedBandSpin);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(presetsLabel);
    controlsLayout->addWidget(m_presetBox);
    controlsLayout->addWidget(m_loadPresetButton);
    controlsLayout->addWidget(m_savePresetButton);
    controlsLayout->addWidget(m_deletePresetButton);
    controlsLayout->addWidget(m_importPresetButton);
    controlsLayout->addWidget(m_exportPresetButton);

    controlsWidget->setFixedWidth(stripWidget->sizeHint().width());

    root->addWidget(controlsWidget, 0, Qt::AlignHCenter | Qt::AlignTop);
    root->addStretch(1);

    QObject::connect(m_loadPresetButton, &QPushButton::clicked, this, [this]() { loadPreset(); });
    QObject::connect(m_savePresetButton, &QPushButton::clicked, this, [this]() { savePreset(); });
    QObject::connect(m_deletePresetButton, &QPushButton::clicked, this, [this]() { deletePreset(); });
    QObject::connect(m_importPresetButton, &QPushButton::clicked, this, [this]() { importPreset(); });
    QObject::connect(m_exportPresetButton, &QPushButton::clicked, this, [this]() { exportPreset(); });
    QObject::connect(m_selectedBandCombo, &QComboBox::currentIndexChanged, this,
                     [this](int) { refreshSelectedBandEditor(); });
    QObject::connect(m_selectedBandSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        const int bandIndex = m_selectedBandCombo->currentIndex();
        if(bandIndex < 0 || std::cmp_greater_equal(bandIndex, m_bandSliders.size())) {
            return;
        }
        m_bandSliders[static_cast<size_t>(bandIndex)]->setValue(gainToSliderValue(value));
    });
    QObject::connect(m_presetBox, &QComboBox::currentTextChanged, this, [this]() { updatePresetButtons(); });

    QObject::connect(&m_presetStore, &EqualiserPresetStore::presetsChanged, this,
                     &EqualiserSettingsWidget::refreshPresets);

    refreshPresets();
    refreshSelectedBandEditor();
    refreshValueLabels();

    if(auto* dialogLayout = layout()) {
        dialogLayout->setSizeConstraint(QLayout::SetFixedSize);
    }
}

void EqualiserSettingsWidget::loadSettings(const QByteArray& settings)
{
    applySliderState(sliderStateFromSettings(settings).value_or(zeroSliderState()));
}

QByteArray EqualiserSettingsWidget::saveSettings() const
{
    return settingsFromSliderState(sliderState());
}

EqualiserSliderState EqualiserSettingsWidget::sliderState() const
{
    EqualiserSliderState state;
    state.preamp = m_preampSlider->value();

    for(size_t i{0}; i < state.bands.size(); ++i) {
        state.bands[i] = m_bandSliders[i]->value();
    }

    return state;
}

void EqualiserSettingsWidget::applySliderState(const EqualiserSliderState& state)
{
    std::vector<QSignalBlocker> signalBlockers;
    signalBlockers.reserve(1 + m_bandSliders.size());
    signalBlockers.emplace_back(m_preampSlider);
    for(auto* slider : m_bandSliders) {
        signalBlockers.emplace_back(slider);
    }

    m_preampSlider->setValue(state.preamp);
    for(size_t i{0}; i < state.bands.size(); ++i) {
        m_bandSliders[i]->setValue(state.bands[i]);
    }

    refreshTooltips();
    refreshValueLabels();
    refreshSelectedBandEditor();
}

void EqualiserSettingsWidget::connectSliderSignals(QSlider* slider, const bool refreshBandEditor)
{
    if(!slider) {
        return;
    }

    QObject::connect(slider, &QSlider::valueChanged, this, [this, slider, refreshBandEditor](int) {
        refreshTooltips();
        refreshValueLabels();
        if(refreshBandEditor) {
            refreshSelectedBandEditor();
        }
        m_previewTimer.start(PreviewDebounceMs, this);
        if(slider->isSliderDown()) {
            showSliderToolTip(this, m_sliderToolTip, slider);
        }
    });

    QObject::connect(slider, &QSlider::sliderPressed, this,
                     [this, slider]() { showSliderToolTip(this, m_sliderToolTip, slider); });
    QObject::connect(slider, &QSlider::sliderMoved, this,
                     [this, slider](int) { showSliderToolTip(this, m_sliderToolTip, slider); });
    QObject::connect(slider, &QSlider::sliderReleased, this, [this]() { hideSliderToolTip(m_sliderToolTip); });
}

void EqualiserSettingsWidget::restoreDefaults()
{
    zeroAll();
}

void EqualiserSettingsWidget::refreshPresets()
{
    const QString currentText = m_presetBox->currentText().trimmed();

    m_presetBox->clear();

    const auto presets = m_presetStore.presets();
    for(const auto& preset : presets) {
        m_presetBox->addItem(preset.name);
    }

    if(!currentText.isEmpty()) {
        const int idx = m_presetBox->findText(currentText);
        if(idx >= 0) {
            m_presetBox->setCurrentIndex(idx);
        }
        else {
            m_presetBox->setEditText(currentText);
        }
    }

    updatePresetButtons();
}

void EqualiserSettingsWidget::updatePresetButtons()
{
    const QString name   = m_presetBox->currentText().trimmed();
    const bool hasPreset = m_presetStore.indexByName(name) >= 0;

    m_loadPresetButton->setEnabled(hasPreset);
    m_deletePresetButton->setEnabled(hasPreset);
    m_savePresetButton->setEnabled(!name.isEmpty());
}

void EqualiserSettingsWidget::loadPreset()
{
    const int presetIndex = m_presetStore.indexByName(m_presetBox->currentText());
    if(presetIndex < 0) {
        return;
    }

    const auto state = sliderStateFromSettings(m_presetStore.presets().at(static_cast<size_t>(presetIndex)).settings);
    if(!state) {
        QMessageBox::warning(this, tr("Presets"), tr("Unable to load the selected preset."));
        return;
    }

    applySliderState(*state);
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::savePreset()
{
    const QString name = m_presetBox->currentText().trimmed();
    if(name.isEmpty()) {
        return;
    }

    const int existingIndex = m_presetStore.indexByName(name);
    if(existingIndex >= 0) {
        QMessageBox msg{QMessageBox::Question, tr("Preset already exists"),
                        tr("Preset \"%1\" already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() != QMessageBox::Yes) {
            return;
        }
    }

    m_presetStore.setPreset(name, saveSettings());
    m_presetBox->setCurrentText(name);
}

void EqualiserSettingsWidget::deletePreset()
{
    const int presetIndex = m_presetStore.indexByName(m_presetBox->currentText());
    if(presetIndex < 0) {
        return;
    }

    const auto& presets = m_presetStore.presets();

    QString nextPresetName;
    if(std::cmp_less(presetIndex + 1, presets.size())) {
        nextPresetName = presets[static_cast<size_t>(presetIndex + 1)].name;
    }
    else if(presets.size() > 1) {
        nextPresetName = presets[static_cast<size_t>(presetIndex - 1)].name;
    }

    m_presetStore.removePreset(m_presetBox->currentText());
    m_presetBox->setEditText(nextPresetName);
}

void EqualiserSettingsWidget::importPreset()
{
    QSettings settings;
    const QString initialPath = settings.value(QLatin1String(LastPresetPathKey), QDir::homePath()).toString();

    const QString filePath = QFileDialog::getOpenFileName(this, tr("Import Equaliser Preset"), initialPath,
                                                          tr("Equaliser Preset (*.feq)"));
    if(filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import Equaliser Preset"),
                             tr("Unable to open \"%1\" for reading.").arg(filePath));
        return;
    }

    std::array<int, EqualiserDsp::BandCount> gains{};
    int parsedBands{0};
    int lineNumber{0};

    QTextStream stream(&file);
    while(!stream.atEnd() && parsedBands < EqualiserDsp::BandCount) {
        const QString line = stream.readLine();
        ++lineNumber;

        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty()) {
            continue;
        }

        bool ok{false};
        const int value = trimmed.toInt(&ok);
        if(!ok) {
            QMessageBox::warning(this, tr("Import Equaliser Preset"),
                                 tr("Invalid value on line %L1.").arg(lineNumber) + u"\n"_s
                                     + tr("The first %Ln non-empty line(s) must contain integer values.", nullptr,
                                          EqualiserDsp::BandCount));
            return;
        }

        gains[static_cast<size_t>(parsedBands)] = value;
        ++parsedBands;
    }

    if(parsedBands < EqualiserDsp::BandCount) {
        QMessageBox::warning(this, tr("Import Equaliser Preset"),
                             tr("The preset file contains %Ln band value(s).", nullptr, parsedBands) + u"\n"_s
                                 + tr("Expected %Ln band value(s).", nullptr, EqualiserDsp::BandCount));
        return;
    }

    auto state{sliderState()};
    for(size_t i{0}; i < state.bands.size(); ++i) {
        state.bands[i] = gainToSliderValue(gains[i]);
    }

    applySliderState(state);
    settings.setValue(QLatin1String(LastPresetPathKey), QFileInfo(filePath).absolutePath());
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::exportPreset()
{
    FySettings settings;
    const QString initialPath = settings.value(QLatin1String(LastPresetPathKey), QDir::homePath()).toString();

    QFileDialog saveDialog(this, tr("Export Equaliser Preset"), initialPath, tr("Equaliser Preset (*.feq)"));
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setOption(QFileDialog::DontResolveSymlinks);
    saveDialog.setDefaultSuffix(QStringLiteral("feq"));

    if(QFileInfo{initialPath}.isDir()) {
        saveDialog.selectFile(QStringLiteral("preset.feq"));
    }
    else {
        saveDialog.selectFile(QFileInfo{initialPath}.fileName());
    }

    if(saveDialog.exec() == 0) {
        return;
    }

    const QStringList selectedFiles = saveDialog.selectedFiles();
    if(selectedFiles.isEmpty()) {
        return;
    }

    const QString filePath = selectedFiles.constFirst();
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export Equaliser Preset"),
                             tr("Unable to open \"%1\" for writing.").arg(filePath));
        return;
    }

    QTextStream stream(&file);
    for(auto* slider : m_bandSliders) {
        const int value = static_cast<int>(std::lround(sliderValueToGain(slider->value())));
        stream << value << '\n';
    }

    if(stream.status() != QTextStream::Ok) {
        QMessageBox::warning(this, tr("Export Equaliser Preset"),
                             tr("An error occurred while writing \"%1\".").arg(filePath));
        return;
    }

    settings.setValue(QLatin1String(LastPresetPathKey), QFileInfo(filePath).absolutePath());
}

void EqualiserSettingsWidget::zeroAll()
{
    applySliderState(zeroSliderState());
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::autoLevel()
{
    auto state{sliderState()};
    if(!autoLevelSliderState(state)) {
        return;
    }

    applySliderState(state);
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::refreshTooltips()
{
    m_preampSlider->setToolTip(gainTooltip(sliderValueToGain(m_preampSlider->value())));

    for(auto* slider : m_bandSliders) {
        slider->setToolTip(gainTooltip(sliderValueToGain(slider->value())));
    }
}

void EqualiserSettingsWidget::refreshValueLabels()
{
    m_preampValueLabel->setText(gainText(m_preampSlider->value()));

    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandValueLabels[i]->setText(gainText(m_bandSliders[i]->value()));
    }
}

void EqualiserSettingsWidget::refreshSelectedBandEditor()
{
    const int bandIndex  = m_selectedBandCombo->currentIndex();
    const bool validBand = (bandIndex >= 0 && std::cmp_less(bandIndex, m_bandSliders.size()));

    m_selectedBandSpin->setEnabled(validBand);
    if(!validBand) {
        return;
    }

    const int sliderValue = m_bandSliders[static_cast<size_t>(bandIndex)]->value();
    const QSignalBlocker blockSpin(m_selectedBandSpin);
    m_selectedBandSpin->setValue(sliderValueToGain(sliderValue));
}

void EqualiserSettingsWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        publishPreviewSettings();
        return;
    }

    DspSettingsDialog::timerEvent(event);
}

EqualiserSettingsProvider::EqualiserSettingsProvider()
    : m_presetStore{std::make_unique<EqualiserPresetStore>()}
{ }

EqualiserSettingsProvider::~EqualiserSettingsProvider() = default;

QString EqualiserSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.equaliser");
}

QString EqualiserSettingsProvider::displayName() const
{
    return QCoreApplication::translate("Fooyin::Equaliser::EqualiserSettingsProvider", "Equaliser");
}

QString EqualiserSettingsProvider::viewMenuText() const
{
    return QCoreApplication::translate("Fooyin::Equaliser::EqualiserSettingsProvider", "Equaliser");
}

QString EqualiserSettingsProvider::viewMenuStatusTip() const
{
    return QCoreApplication::translate("Fooyin::Equaliser::EqualiserSettingsProvider", "Open Equaliser settings");
}

bool EqualiserSettingsProvider::showInViewMenu() const
{
    return true;
}

bool EqualiserSettingsProvider::showAsLayoutWidget() const
{
    return true;
}

DspLayoutEditor* EqualiserSettingsProvider::createLayoutEditor(QWidget* parent)
{
    return new EqualiserLayoutEditor(*m_presetStore, parent);
}

DspSettingsDialog* EqualiserSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new EqualiserSettingsWidget(*m_presetStore, parent);
}
} // namespace Fooyin::Equaliser
