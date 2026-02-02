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

#include "skipsilencesettingswidget.h"

#include "core/engine/dsp/skipsilencesettings.h"

#include <QCheckBox>
#include <QDataStream>
#include <QFrame>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QSlider>

#include <algorithm>

namespace Fooyin {
SkipSilenceSettingsWidget::SkipSilenceSettingsWidget(QWidget* parent)
    : DspSettingsDialog{parent}
    , m_minSilenceSlider{new QSlider(Qt::Horizontal, this)}
    , m_thresholdSlider{new QSlider(Qt::Horizontal, this)}
    , m_keepInitialCheck{new QCheckBox(QObject::tr("Do not skip initial silence"), this)}
    , m_skipMiddleCheck{new QCheckBox(QObject::tr("Skip silence in the middle of a track"), this)}
    , m_minSilenceValueLabel{new QLabel(this)}
    , m_thresholdValueLabel{new QLabel(this)}
{
    auto* layout = contentLayout();

    auto* minSilenceLabel = new QLabel(QObject::tr("Minimum silence duration to activate removal:"), this);
    layout->addWidget(minSilenceLabel);

    m_minSilenceSlider->setRange(SkipSilenceSettings::MinSilenceMinMs, SkipSilenceSettings::MinSilenceMaxMs);
    m_minSilenceSlider->setSingleStep(50);
    layout->addWidget(m_minSilenceSlider);

    auto* minSilenceRangeLayout = new QHBoxLayout();
    minSilenceRangeLayout->setContentsMargins(0, 0, 0, 0);
    minSilenceRangeLayout->addWidget(new QLabel(QObject::tr("Short"), this));
    minSilenceRangeLayout->addStretch();
    minSilenceRangeLayout->addWidget(new QLabel(QObject::tr("Long"), this));
    layout->addLayout(minSilenceRangeLayout);

    m_minSilenceValueLabel->setAlignment(Qt::AlignHCenter);
    m_minSilenceValueLabel->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_minSilenceValueLabel);

    layout->addWidget(m_keepInitialCheck);
    layout->addWidget(m_skipMiddleCheck);

    auto* thresholdLabel = new QLabel(QObject::tr("Silence detection threshold:"), this);
    layout->addWidget(thresholdLabel);

    m_thresholdSlider->setRange(SkipSilenceSettings::ThresholdMinDb, SkipSilenceSettings::ThresholdMaxDb);
    m_thresholdSlider->setSingleStep(1);
    layout->addWidget(m_thresholdSlider);

    auto* thresholdRangeLayout = new QHBoxLayout();
    thresholdRangeLayout->setContentsMargins(0, 0, 0, 0);
    thresholdRangeLayout->addWidget(new QLabel(QObject::tr("Quiet"), this));
    thresholdRangeLayout->addStretch();
    thresholdRangeLayout->addWidget(new QLabel(QObject::tr("Loud"), this));
    layout->addLayout(thresholdRangeLayout);

    m_thresholdValueLabel->setAlignment(Qt::AlignHCenter);
    m_thresholdValueLabel->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_thresholdValueLabel);

    QObject::connect(m_minSilenceSlider, &QSlider::valueChanged, this, [this]() { updateLabels(); });
    QObject::connect(m_thresholdSlider, &QSlider::valueChanged, this, [this]() { updateLabels(); });
}

void SkipSilenceSettingsWidget::loadSettings(const QByteArray& settings)
{
    qint32 minSilence  = SkipSilenceSettings::DefaultMinSilenceDurationMs;
    qint32 threshold   = SkipSilenceSettings::DefaultThresholdDb;
    bool keepInitial   = SkipSilenceSettings::DefaultKeepInitialPeriod;
    bool includeMiddle = SkipSilenceSettings::DefaultIncludeMiddleSilence;

    if(!settings.isEmpty()) {
        QDataStream stream{settings};
        stream.setVersion(QDataStream::Qt_6_0);

        quint32 version{0};
        stream >> version;

        if(stream.status() == QDataStream::Ok && version == SkipSilenceSettings::SerializationVersion) {
            stream >> minSilence >> threshold >> keepInitial >> includeMiddle;
            if(stream.status() != QDataStream::Ok) {
                minSilence    = SkipSilenceSettings::DefaultMinSilenceDurationMs;
                threshold     = SkipSilenceSettings::DefaultThresholdDb;
                keepInitial   = SkipSilenceSettings::DefaultKeepInitialPeriod;
                includeMiddle = SkipSilenceSettings::DefaultIncludeMiddleSilence;
            }
        }
    }

    m_minSilenceSlider->setValue(
        std::clamp(minSilence, SkipSilenceSettings::MinSilenceMinMs, SkipSilenceSettings::MinSilenceMaxMs));
    m_thresholdSlider->setValue(
        std::clamp(threshold, SkipSilenceSettings::ThresholdMinDb, SkipSilenceSettings::ThresholdMaxDb));
    m_keepInitialCheck->setChecked(keepInitial);
    m_skipMiddleCheck->setChecked(!includeMiddle);
    updateLabels();
}

QByteArray SkipSilenceSettingsWidget::saveSettings() const
{
    QByteArray settingsData;
    QDataStream stream{&settingsData, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(SkipSilenceSettings::SerializationVersion);
    stream << static_cast<qint32>(m_minSilenceSlider->value());
    stream << static_cast<qint32>(m_thresholdSlider->value());
    stream << m_keepInitialCheck->isChecked();
    stream << !m_skipMiddleCheck->isChecked();

    return settingsData;
}

void SkipSilenceSettingsWidget::restoreDefaults()
{
    m_minSilenceSlider->setValue(SkipSilenceSettings::DefaultMinSilenceDurationMs);
    m_thresholdSlider->setValue(SkipSilenceSettings::DefaultThresholdDb);
    m_keepInitialCheck->setChecked(SkipSilenceSettings::DefaultKeepInitialPeriod);
    m_skipMiddleCheck->setChecked(!SkipSilenceSettings::DefaultIncludeMiddleSilence);
    updateLabels();
}

void SkipSilenceSettingsWidget::updateLabels() const
{
    m_minSilenceValueLabel->setText(QString::number(m_minSilenceSlider->value()) + QObject::tr(" ms"));
    m_thresholdValueLabel->setText(QString::number(m_thresholdSlider->value()) + QObject::tr(" dB"));
}

QString SkipSilenceSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.skipsilence");
}

DspSettingsDialog* SkipSilenceSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SkipSilenceSettingsWidget(parent);
}
} // namespace Fooyin
