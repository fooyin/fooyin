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

#include "fadingpage.h"

#include <core/engine/enginedefs.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class FadingPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit FadingPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QGroupBox* m_fadingBox;
    QCheckBox* m_fadingPauseEnabled;
    QCheckBox* m_fadingStopEnabled;
    QSpinBox* m_fadingPauseIn;
    QSpinBox* m_fadingPauseOut;
    QSpinBox* m_fadingStopIn;
    QSpinBox* m_fadingStopOut;
    QCheckBox* m_fadingBoundaryEnabled;
    QSpinBox* m_fadingBoundaryIn;
    QSpinBox* m_fadingBoundaryOut;
    QComboBox* m_pauseFadeCurve;
    QComboBox* m_stopFadeCurve;
    QComboBox* m_boundaryFadeCurve;

    QGroupBox* m_crossfadeBox;
    QCheckBox* m_crossfadeSeekEnabled;
    QCheckBox* m_crossfadeManualEnabled;
    QCheckBox* m_crossfadeAutoEnabled;
    QSpinBox* m_crossfadeManualIn;
    QSpinBox* m_crossfadeManualOut;
    QSpinBox* m_crossfadeAutoIn;
    QSpinBox* m_crossfadeAutoOut;
    QLabel* m_crossfadeAutoSwitchPolicyLabel;
    QComboBox* m_crossfadeAutoSwitchPolicy;
    QSpinBox* m_crossfadeSeekIn;
    QSpinBox* m_crossfadeSeekOut;
};

FadingPageWidget::FadingPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_fadingBox{new QGroupBox(tr("Fading"), this)}
    , m_fadingPauseEnabled{new QCheckBox(tr("Pause"), this)}
    , m_fadingStopEnabled{new QCheckBox(tr("Stop"), this)}
    , m_fadingPauseIn{new QSpinBox(this)}
    , m_fadingPauseOut{new QSpinBox(this)}
    , m_fadingStopIn{new QSpinBox(this)}
    , m_fadingStopOut{new QSpinBox(this)}
    , m_fadingBoundaryEnabled{new QCheckBox(tr("Boundary"), this)}
    , m_fadingBoundaryIn{new QSpinBox(this)}
    , m_fadingBoundaryOut{new QSpinBox(this)}
    , m_pauseFadeCurve{new QComboBox(this)}
    , m_stopFadeCurve{new QComboBox(this)}
    , m_boundaryFadeCurve{new QComboBox(this)}
    , m_crossfadeBox{new QGroupBox(tr("Crossfading"), this)}
    , m_crossfadeSeekEnabled{new QCheckBox(tr("Seek"), this)}
    , m_crossfadeManualEnabled{new QCheckBox(tr("Manual track change"), this)}
    , m_crossfadeAutoEnabled{new QCheckBox(tr("Auto track change"), this)}
    , m_crossfadeManualIn{new QSpinBox(this)}
    , m_crossfadeManualOut{new QSpinBox(this)}
    , m_crossfadeAutoIn{new QSpinBox(this)}
    , m_crossfadeAutoOut{new QSpinBox(this)}
    , m_crossfadeAutoSwitchPolicyLabel{new QLabel(tr("Auto switch policy") + u":"_s, this)}
    , m_crossfadeAutoSwitchPolicy{new QComboBox(this)}
    , m_crossfadeSeekIn{new QSpinBox(this)}
    , m_crossfadeSeekOut{new QSpinBox(this)}
{
    auto* fadingLayout = new QGridLayout(m_fadingBox);
    m_fadingBox->setCheckable(true);

    m_fadingPauseIn->setSuffix(u"ms"_s);
    m_fadingPauseOut->setSuffix(u"ms"_s);
    m_fadingStopIn->setSuffix(u"ms"_s);
    m_fadingStopOut->setSuffix(u"ms"_s);
    m_fadingBoundaryIn->setSuffix(u"ms"_s);
    m_fadingBoundaryOut->setSuffix(u"ms"_s);
    m_crossfadeManualIn->setSuffix(u"ms"_s);
    m_crossfadeManualOut->setSuffix(u"ms"_s);
    m_crossfadeAutoIn->setSuffix(u"ms"_s);
    m_crossfadeAutoOut->setSuffix(u"ms"_s);
    m_crossfadeSeekIn->setSuffix(u"ms"_s);
    m_crossfadeSeekOut->setSuffix(u"ms"_s);

    m_fadingPauseIn->setMaximum(10000);
    m_fadingPauseOut->setMaximum(10000);
    m_fadingStopIn->setMaximum(10000);
    m_fadingStopOut->setMaximum(10000);
    m_fadingBoundaryIn->setMaximum(10000);
    m_fadingBoundaryOut->setMaximum(10000);

    m_fadingPauseIn->setSingleStep(100);
    m_fadingPauseOut->setSingleStep(100);
    m_fadingStopIn->setSingleStep(100);
    m_fadingStopOut->setSingleStep(100);
    m_fadingBoundaryIn->setSingleStep(100);
    m_fadingBoundaryOut->setSingleStep(100);

    const auto addFadeCurveItems = [](QComboBox* combo) {
        combo->addItem(tr("Linear"), static_cast<int>(Engine::FadeCurve::Linear));
        combo->addItem(tr("Ease Out (Sine)"), static_cast<int>(Engine::FadeCurve::Sine));
        combo->addItem(tr("Ease In (Cosine)"), static_cast<int>(Engine::FadeCurve::Cosine));
        combo->addItem(tr("Ease Out (Exponential)"), static_cast<int>(Engine::FadeCurve::Logarithmic));
        combo->addItem(tr("Ease In (Exponential)"), static_cast<int>(Engine::FadeCurve::Exponential));
        combo->addItem(tr("Smooth S-Curve"), static_cast<int>(Engine::FadeCurve::SmootherStep));
    };

    addFadeCurveItems(m_pauseFadeCurve);
    addFadeCurveItems(m_stopFadeCurve);
    addFadeCurveItems(m_boundaryFadeCurve);

    const auto configureEditor = [](QWidget* widget, const int minWidth) {
        widget->setMinimumWidth(minWidth);
        widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    };
    const auto configureRowToggle = [](QCheckBox* toggle) {
        toggle->setMinimumWidth(170);
    };

    for(auto* toggle : {m_fadingPauseEnabled, m_fadingStopEnabled, m_fadingBoundaryEnabled, m_crossfadeSeekEnabled,
                        m_crossfadeManualEnabled, m_crossfadeAutoEnabled}) {
        configureRowToggle(toggle);
    }
    for(auto* spinBox : {m_fadingPauseIn, m_fadingPauseOut, m_fadingStopIn, m_fadingStopOut, m_fadingBoundaryIn,
                         m_fadingBoundaryOut, m_crossfadeSeekIn, m_crossfadeSeekOut, m_crossfadeManualIn,
                         m_crossfadeManualOut, m_crossfadeAutoIn, m_crossfadeAutoOut}) {
        configureEditor(spinBox, 108);
    }
    for(auto* combo : {m_pauseFadeCurve, m_stopFadeCurve, m_boundaryFadeCurve, m_crossfadeAutoSwitchPolicy}) {
        configureEditor(combo, 170);
    }

    auto* fadingHelp = new QLabel(
        tr("Pause, stop, and boundary fades without overlap. Boundary fades are used instead of auto track-change "
           "crossfades."),
        this);
    fadingHelp->setWordWrap(true);

    int row{0};
    fadingLayout->setContentsMargins(14, 18, 14, 14);
    fadingLayout->setHorizontalSpacing(12);
    fadingLayout->setVerticalSpacing(10);

    fadingLayout->addWidget(fadingHelp, row++, 0, 1, 4);
    fadingLayout->addWidget(new QLabel(tr("Fade In"), this), row, 1);
    fadingLayout->addWidget(new QLabel(tr("Fade Out"), this), row, 2);
    fadingLayout->addWidget(new QLabel(tr("Curve"), this), row++, 3);

    fadingLayout->addWidget(m_fadingPauseEnabled, row, 0);
    fadingLayout->addWidget(m_fadingPauseIn, row, 1);
    fadingLayout->addWidget(m_fadingPauseOut, row, 2);
    fadingLayout->addWidget(m_pauseFadeCurve, row++, 3);

    fadingLayout->addWidget(m_fadingStopEnabled, row, 0);
    fadingLayout->addWidget(m_fadingStopIn, row, 1);
    fadingLayout->addWidget(m_fadingStopOut, row, 2);
    fadingLayout->addWidget(m_stopFadeCurve, row++, 3);

    fadingLayout->addWidget(m_fadingBoundaryEnabled, row, 0);
    fadingLayout->addWidget(m_fadingBoundaryIn, row, 1);
    fadingLayout->addWidget(m_fadingBoundaryOut, row, 2);
    fadingLayout->addWidget(m_boundaryFadeCurve, row++, 3);
    fadingLayout->setColumnStretch(4, 1);

    auto* crossmixLayout = new QGridLayout(m_crossfadeBox);
    m_crossfadeBox->setCheckable(true);

    m_crossfadeManualIn->setMaximum(30000);
    m_crossfadeManualOut->setMaximum(30000);
    m_crossfadeAutoIn->setRange(100, 30000);
    m_crossfadeAutoOut->setRange(100, 30000);
    m_crossfadeSeekIn->setMaximum(10000);
    m_crossfadeSeekOut->setMaximum(10000);

    m_crossfadeManualIn->setSingleStep(100);
    m_crossfadeManualOut->setSingleStep(100);
    m_crossfadeAutoIn->setSingleStep(100);
    m_crossfadeAutoOut->setSingleStep(100);
    m_crossfadeSeekIn->setSingleStep(100);
    m_crossfadeSeekOut->setSingleStep(100);

    m_crossfadeAutoSwitchPolicy->addItem(tr("Overlap start"),
                                         static_cast<int>(Engine::CrossfadeSwitchPolicy::OverlapStart));
    m_crossfadeAutoSwitchPolicy->addItem(tr("Boundary"), static_cast<int>(Engine::CrossfadeSwitchPolicy::Boundary));
    m_crossfadeAutoSwitchPolicy->setToolTip(
        tr("Controls when the UI switches tracks during automatic crossfade transitions"));

    auto* crossfadeHelp = new QLabel(
        tr("Overlapping fades for seek and track changes. Auto track change uses overlap; use Boundary above for "
           "non-overlapping fades."),
        this);
    crossfadeHelp->setWordWrap(true);

    row = 0;
    crossmixLayout->setContentsMargins(14, 18, 14, 14);
    crossmixLayout->setHorizontalSpacing(12);
    crossmixLayout->setVerticalSpacing(10);

    crossmixLayout->addWidget(crossfadeHelp, row++, 0, 1, 4);
    crossmixLayout->addWidget(new QLabel(tr("Fade In"), this), row, 1);
    crossmixLayout->addWidget(new QLabel(tr("Fade Out"), this), row++, 2);

    crossmixLayout->addWidget(m_crossfadeSeekEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeSeekIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeSeekOut, row++, 2);

    crossmixLayout->addWidget(m_crossfadeManualEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeManualIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeManualOut, row++, 2);

    crossmixLayout->addWidget(m_crossfadeAutoEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeAutoIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeAutoOut, row++, 2);
    crossmixLayout->setRowMinimumHeight(row - 1, 30);
    crossmixLayout->addWidget(m_crossfadeAutoSwitchPolicyLabel, row, 0);
    crossmixLayout->addWidget(m_crossfadeAutoSwitchPolicy, row, 1, 1, 2);
    crossmixLayout->setColumnStretch(4, 1);
    ++row;
    crossmixLayout->setColumnStretch(3, 1);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_fadingBox);
    mainLayout->addWidget(m_crossfadeBox);
    mainLayout->addStretch(1);

    const auto enforceExclusiveOptions = [this]() {
        if(m_fadingBoundaryEnabled->isChecked()) {
            m_crossfadeAutoEnabled->setChecked(false);
        }
        else if(m_crossfadeAutoEnabled->isChecked()) {
            m_fadingBoundaryEnabled->setChecked(false);
        }
    };

    const auto updateRowStates = [this]() {
        auto setEnabled = []<typename... Widgets>(const bool enabled, Widgets... widgets) {
            (widgets->setEnabled(enabled), ...);
        };

        const bool fadingEnabled      = m_fadingBox->isChecked();
        const bool crossfadingEnabled = m_crossfadeBox->isChecked();

        setEnabled(fadingEnabled && !m_crossfadeAutoEnabled->isChecked(), m_fadingBoundaryEnabled);
        setEnabled(crossfadingEnabled && !m_fadingBoundaryEnabled->isChecked(), m_crossfadeAutoEnabled,
                   m_crossfadeAutoSwitchPolicyLabel, m_crossfadeAutoSwitchPolicy);

        const bool boundaryFadeActive    = m_fadingBoundaryEnabled->isChecked();
        const bool autoCrossfadeActive   = m_crossfadeAutoEnabled->isChecked();
        const bool pauseFadeActive       = m_fadingPauseEnabled->isChecked();
        const bool stopFadeActive        = m_fadingStopEnabled->isChecked();
        const bool seekCrossfadeActive   = m_crossfadeSeekEnabled->isChecked();
        const bool manualCrossfadeActive = m_crossfadeManualEnabled->isChecked();

        setEnabled(fadingEnabled && pauseFadeActive, m_fadingPauseIn, m_fadingPauseOut, m_pauseFadeCurve);
        setEnabled(fadingEnabled && stopFadeActive, m_fadingStopIn, m_fadingStopOut, m_stopFadeCurve);
        setEnabled(fadingEnabled && boundaryFadeActive, m_fadingBoundaryIn, m_fadingBoundaryOut, m_boundaryFadeCurve);

        setEnabled(crossfadingEnabled && seekCrossfadeActive, m_crossfadeSeekIn, m_crossfadeSeekOut);
        setEnabled(crossfadingEnabled && manualCrossfadeActive, m_crossfadeManualIn, m_crossfadeManualOut);
        setEnabled(crossfadingEnabled && autoCrossfadeActive, m_crossfadeAutoIn, m_crossfadeAutoOut);
        setEnabled(crossfadingEnabled && autoCrossfadeActive && m_crossfadeAutoEnabled->isEnabled(),
                   m_crossfadeAutoSwitchPolicyLabel, m_crossfadeAutoSwitchPolicy);
    };

    const auto updateState = [enforceExclusiveOptions, updateRowStates]() {
        enforceExclusiveOptions();
        updateRowStates();
    };

    QObject::connect(m_fadingBox, &QGroupBox::toggled, this, [updateRowStates]() { updateRowStates(); });
    QObject::connect(m_crossfadeBox, &QGroupBox::toggled, this, [updateRowStates]() { updateRowStates(); });
    QObject::connect(m_fadingPauseEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_fadingStopEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_fadingBoundaryEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeSeekEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeManualEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeAutoEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });

    updateRowStates();
}

void FadingPageWidget::load()
{
    m_fadingBox->setChecked(m_settings->value<Settings::Core::Internal::EngineFading>());
    const auto fadingValues = m_settings->value<Settings::Core::Internal::FadingValues>().value<Engine::FadingValues>();
    const auto loadFadeSpec = [](const Engine::FadeSpec& spec, QCheckBox* enabled, QSpinBox* in, QSpinBox* out,
                                 QComboBox* curve = nullptr) {
        enabled->setChecked(spec.enabled);
        in->setValue(spec.in);
        out->setValue(spec.out);
        if(curve) {
            curve->setCurrentIndex(static_cast<int>(spec.curve));
        }
    };

    loadFadeSpec(fadingValues.pause, m_fadingPauseEnabled, m_fadingPauseIn, m_fadingPauseOut, m_pauseFadeCurve);
    loadFadeSpec(fadingValues.stop, m_fadingStopEnabled, m_fadingStopIn, m_fadingStopOut, m_stopFadeCurve);
    loadFadeSpec(fadingValues.boundary, m_fadingBoundaryEnabled, m_fadingBoundaryIn, m_fadingBoundaryOut,
                 m_boundaryFadeCurve);

    m_crossfadeBox->setChecked(m_settings->value<Settings::Core::Internal::EngineCrossfading>());
    const auto crossfadingValues
        = m_settings->value<Settings::Core::Internal::CrossfadingValues>().value<Engine::CrossfadingValues>();

    loadFadeSpec(crossfadingValues.manualChange, m_crossfadeManualEnabled, m_crossfadeManualIn, m_crossfadeManualOut);
    loadFadeSpec(crossfadingValues.autoChange, m_crossfadeAutoEnabled, m_crossfadeAutoIn, m_crossfadeAutoOut);
    loadFadeSpec(crossfadingValues.seek, m_crossfadeSeekEnabled, m_crossfadeSeekIn, m_crossfadeSeekOut);

    const auto policy = static_cast<Engine::CrossfadeSwitchPolicy>(
        m_settings->value<Settings::Core::Internal::CrossfadeSwitchPolicy>());
    const int policyIndex
        = m_crossfadeAutoSwitchPolicy->findData(static_cast<int>(policy), Qt::UserRole, Qt::MatchExactly);
    m_crossfadeAutoSwitchPolicy->setCurrentIndex(policyIndex >= 0 ? policyIndex : 0);

    if(m_crossfadeAutoEnabled->isChecked() && m_fadingBoundaryEnabled->isChecked()) {
        m_fadingBoundaryEnabled->setChecked(false);
    }
}

void FadingPageWidget::apply()
{
    const auto saveFadeSpec = [](Engine::FadeSpec& spec, const QCheckBox* enabled, const QSpinBox* in,
                                 const QSpinBox* out, const QComboBox* curve = nullptr) {
        spec.enabled = enabled->isChecked();
        spec.in      = in->value();
        spec.out     = out->value();
        if(curve) {
            spec.curve = static_cast<Engine::FadeCurve>(curve->currentData().toInt());
        }
    };

    Engine::FadingValues fadingValues;
    saveFadeSpec(fadingValues.pause, m_fadingPauseEnabled, m_fadingPauseIn, m_fadingPauseOut, m_pauseFadeCurve);
    saveFadeSpec(fadingValues.stop, m_fadingStopEnabled, m_fadingStopIn, m_fadingStopOut, m_stopFadeCurve);
    saveFadeSpec(fadingValues.boundary, m_fadingBoundaryEnabled, m_fadingBoundaryIn, m_fadingBoundaryOut,
                 m_boundaryFadeCurve);

    Engine::CrossfadingValues crossfadingValues;
    saveFadeSpec(crossfadingValues.manualChange, m_crossfadeManualEnabled, m_crossfadeManualIn, m_crossfadeManualOut);
    saveFadeSpec(crossfadingValues.autoChange, m_crossfadeAutoEnabled, m_crossfadeAutoIn, m_crossfadeAutoOut);
    saveFadeSpec(crossfadingValues.seek, m_crossfadeSeekEnabled, m_crossfadeSeekIn, m_crossfadeSeekOut);

    if(crossfadingValues.autoChange.enabled && fadingValues.boundary.enabled) {
        fadingValues.boundary.enabled = false;
    }

    m_settings->set<Settings::Core::Internal::EngineFading>(m_fadingBox->isChecked());
    m_settings->set<Settings::Core::Internal::FadingValues>(QVariant::fromValue(fadingValues));
    m_settings->set<Settings::Core::Internal::EngineCrossfading>(m_crossfadeBox->isChecked());
    m_settings->set<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues));
    m_settings->set<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        m_crossfadeAutoSwitchPolicy->currentData().toInt());
}

void FadingPageWidget::reset()
{
    m_settings->reset<Settings::Core::Internal::EngineFading>();
    m_settings->reset<Settings::Core::Internal::FadingValues>();
    m_settings->reset<Settings::Core::Internal::EngineCrossfading>();
    m_settings->reset<Settings::Core::Internal::CrossfadingValues>();
    m_settings->reset<Settings::Core::Internal::CrossfadeSwitchPolicy>();
}

FadingPage::FadingPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Fading);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("Fading")});
    setWidgetCreator([settings] { return new FadingPageWidget(settings); });
}
} // namespace Fooyin

#include "fadingpage.moc"
#include "moc_fadingpage.cpp"
