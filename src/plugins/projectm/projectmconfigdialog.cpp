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

#include "projectmconfigdialog.h"

#include <gui/framerate.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>

using namespace Qt::StringLiterals;

namespace Fooyin::ProjectM {
namespace {
QDoubleSpinBox* createDurationSpin(QWidget* parent, double minimum, double maximum, double singleStep)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(singleStep);
    spin->setDecimals(1);
    spin->setSuffix(ProjectMConfigDialog::tr(" s"));
    return spin;
}

int currentFrameRate(const QComboBox* combo)
{
    bool ok{false};
    const int fps = combo->currentText().toInt(&ok);
    return ok ? std::clamp(fps, 1, Gui::FrameRate::maxFps()) : Gui::FrameRate::toFps(Gui::FrameRate::Preset::Fps60);
}

void setFrameRate(QComboBox* combo, int fps)
{
    const int value = std::clamp(fps, 1, Gui::FrameRate::maxFps());
    const int index = combo->findData(value);
    if(index >= 0) {
        combo->setCurrentIndex(index);
    }
    else {
        combo->setCurrentIndex(-1);
        combo->setEditText(QString::number(value));
    }
}

void setMeshSize(QComboBox* combo, int width, int height)
{
    const QString value = u"%1x%2"_s.arg(width).arg(height);
    const int index     = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : combo->findData(u"48x36"_s));
}

QSize currentMeshSize(const QComboBox* combo)
{
    const QStringList parts = combo->currentData().toString().split(u'x');
    if(parts.size() != 2) {
        return {48, 36};
    }
    return {parts.at(0).toInt(), parts.at(1).toInt()};
}

void addGridRow(QGridLayout* layout, int row, const QString& text, QWidget* widget, QWidget* parent)
{
    auto* label = new QLabel(text + u':', parent);
    label->setToolTip(widget->toolTip());
    layout->addWidget(label, row, 0);
    layout->addWidget(widget, row, 1);
}
} // namespace

ProjectMConfigDialog::ProjectMConfigDialog(ProjectMWidget* widget, QWidget* parent)
    : WidgetConfigDialog{widget, tr("projectM Settings"), parent}
    , m_presetDir{new QLineEdit(this)}
    , m_summary{new QLabel(this)}
    , m_browseDir{new QPushButton(tr("Browse…"), this)}
    , m_scanRecursive{new QCheckBox(tr("Scan for presets recursively"), this)}
    , m_maxFps{new QComboBox(this)}
    , m_meshSize{new QComboBox(this)}
    , m_aspectCorrection{new QCheckBox(tr("Correct aspect ratio"), this)}
    , m_presetDuration{createDurationSpin(this, 1.0, 300.0, 1.0)}
    , m_softCutDuration{createDurationSpin(this, 0.0, 30.0, 0.1)}
    , m_hardCutsEnabled{new QCheckBox(tr("Enable hard cuts"), this)}
    , m_hardCutDuration{createDurationSpin(this, 1.0, 300.0, 1.0)}
    , m_hardCutSensitivity{new QDoubleSpinBox(this)}
    , m_beatSensitivity{new QDoubleSpinBox(this)}
{
    auto* layout       = new QGridLayout();
    auto* presetGroup  = new QGroupBox(tr("Preset library"), this);
    auto* presetLayout = new QGridLayout(presetGroup);
    auto* softCutGroup = new QGroupBox(tr("Soft cuts"), this);
    auto* softCutGrid  = new QGridLayout(softCutGroup);
    auto* hardCutGroup = new QGroupBox(tr("Hard cuts"), this);
    auto* hardCutGrid  = new QGridLayout(hardCutGroup);
    auto* renderGroup  = new QGroupBox(tr("Rendering"), this);
    auto* renderGrid   = new QGridLayout(renderGroup);

    m_presetDir->setClearButtonEnabled(true);
    m_presetDir->setPlaceholderText(tr("Folder containing .milk presets"));
    m_summary->setWordWrap(true);
    m_scanRecursive->setChecked(true);

    m_maxFps->setEditable(true);
    m_maxFps->setInsertPolicy(QComboBox::NoInsert);
    m_maxFps->setValidator(new QIntValidator(1, Gui::FrameRate::maxFps(), m_maxFps));
    m_maxFps->setToolTip(tr("Maximum rendering frame rate in frames per second"));
    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_maxFps->addItem(QString::number(fps), fps);
    }
    m_meshSize->setToolTip(
        tr("Resolution used to evaluate preset equations; higher values improve detail but require more processing"));
    m_meshSize->addItem(tr("32 x 24"), u"32x24"_s);
    m_meshSize->addItem(tr("48 x 36 (default)"), u"48x36"_s);
    m_meshSize->addItem(tr("64 x 48"), u"64x48"_s);
    m_meshSize->addItem(tr("96 x 72"), u"96x72"_s);
    m_meshSize->addItem(tr("128 x 96"), u"128x96"_s);

    m_aspectCorrection->setToolTip(
        tr("Allow supporting presets to correct shapes when the visualisation is not square"));
    m_presetDuration->setToolTip(
        tr("Maximum time a preset is shown before projectM starts a smooth transition to the next preset"));
    m_softCutDuration->setToolTip(tr("Time taken to blend smoothly from one preset to the next"));
    m_hardCutsEnabled->setToolTip(
        tr("Allow strong beats to switch presets immediately instead of using a smooth transition"));
    m_hardCutDuration->setToolTip(tr("Minimum time a preset must be shown before a beat can trigger a hard cut"));
    m_hardCutSensitivity->setToolTip(
        tr("Volume-change threshold for hard cuts; higher values require a stronger beat"));
    m_beatSensitivity->setToolTip(tr("Sensitivity of projectM's beat detection"));

    for(auto* spin : {m_hardCutSensitivity, m_beatSensitivity}) {
        spin->setRange(0.0, 5.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.05);
    }

    presetLayout->addWidget(m_presetDir, 0, 0);
    presetLayout->addWidget(m_browseDir, 0, 1);
    presetLayout->addWidget(m_scanRecursive, 1, 0, 1, 2);
    presetLayout->addWidget(m_summary, 2, 0, 1, 2);
    presetLayout->setColumnStretch(0, 1);

    addGridRow(softCutGrid, 0, tr("Time between auto preset changes"), m_presetDuration, this);
    addGridRow(softCutGrid, 1, tr("Auto preset blend time"), m_softCutDuration, this);
    softCutGrid->setColumnStretch(2, 1);

    hardCutGrid->addWidget(m_hardCutsEnabled, 0, 0, 1, 2);
    addGridRow(hardCutGrid, 1, tr("Average time between hard cuts"), m_hardCutDuration, this);
    addGridRow(hardCutGrid, 2, tr("Hard cut sensitivity"), m_hardCutSensitivity, this);
    addGridRow(hardCutGrid, 3, tr("Beat sensitivity"), m_beatSensitivity, this);
    hardCutGrid->setColumnStretch(2, 1);

    addGridRow(renderGrid, 0, tr("Max. frame rate"), m_maxFps, this);
    addGridRow(renderGrid, 1, tr("Mesh size"), m_meshSize, this);
    renderGrid->addWidget(m_aspectCorrection, 2, 0, 1, 2);
    renderGrid->setColumnStretch(2, 1);

    layout->addWidget(presetGroup, 0, 0, 1, 2);
    layout->addWidget(softCutGroup, 1, 0);
    layout->addWidget(hardCutGroup, 1, 1);
    layout->addWidget(renderGroup, 2, 0, 1, 2);
    contentLayout()->addLayout(layout, 0, 0);

    QObject::connect(m_browseDir, &QPushButton::clicked, this, &ProjectMConfigDialog::browsePresetDir);
    QObject::connect(m_scanRecursive, &QCheckBox::toggled, this, &ProjectMConfigDialog::updateSummary);
    QObject::connect(m_hardCutsEnabled, &QCheckBox::toggled, m_hardCutDuration, &QWidget::setEnabled);
    QObject::connect(m_hardCutsEnabled, &QCheckBox::toggled, m_hardCutSensitivity, &QWidget::setEnabled);
    QObject::connect(m_hardCutsEnabled, &QCheckBox::toggled, m_beatSensitivity, &QWidget::setEnabled);
    QObject::connect(m_presetDir, &QLineEdit::editingFinished, this, &ProjectMConfigDialog::updateSummary);

    loadCurrentConfig();
}

ProjectMWidget::ConfigData ProjectMConfigDialog::config() const
{
    ProjectMWidget::ConfigData config;
    config.presetDir                   = m_presetDir->text();
    config.settings.scanRecursive      = m_scanRecursive->isChecked();
    config.settings.maxFps             = currentFrameRate(m_maxFps);
    config.settings.meshWidth          = currentMeshSize(m_meshSize).width();
    config.settings.meshHeight         = currentMeshSize(m_meshSize).height();
    config.settings.aspectCorrection   = m_aspectCorrection->isChecked();
    config.settings.presetDuration     = m_presetDuration->value();
    config.settings.softCutDuration    = m_softCutDuration->value();
    config.settings.hardCutsEnabled    = m_hardCutsEnabled->isChecked();
    config.settings.hardCutDuration    = m_hardCutDuration->value();
    config.settings.hardCutSensitivity = static_cast<float>(m_hardCutSensitivity->value());
    config.settings.beatSensitivity    = static_cast<float>(m_beatSensitivity->value());
    return config;
}

void ProjectMConfigDialog::setConfig(const ProjectMWidget::ConfigData& config)
{
    m_presetDir->setText(config.presetDir);
    m_scanRecursive->setChecked(config.settings.scanRecursive);
    setFrameRate(m_maxFps, config.settings.maxFps);
    setMeshSize(m_meshSize, config.settings.meshWidth, config.settings.meshHeight);
    m_aspectCorrection->setChecked(config.settings.aspectCorrection);
    m_presetDuration->setValue(config.settings.presetDuration);
    m_softCutDuration->setValue(config.settings.softCutDuration);
    m_hardCutsEnabled->setChecked(config.settings.hardCutsEnabled);
    m_hardCutDuration->setValue(config.settings.hardCutDuration);
    m_hardCutSensitivity->setValue(config.settings.hardCutSensitivity);
    m_beatSensitivity->setValue(config.settings.beatSensitivity);
    m_hardCutDuration->setEnabled(config.settings.hardCutsEnabled);
    m_hardCutSensitivity->setEnabled(config.settings.hardCutsEnabled);
    m_beatSensitivity->setEnabled(config.settings.hardCutsEnabled);
    updateSummary();
}

void ProjectMConfigDialog::browsePresetDir()
{
    const QString startDir = m_presetDir->text().trimmed().isEmpty() ? QDir::homePath() : m_presetDir->text();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Preset Folder"), startDir,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    const QString trimmed = dir.trimmed();
    if(trimmed.isEmpty()) {
        return;
    }
    m_presetDir->setText(QDir::cleanPath(trimmed));
}

void ProjectMConfigDialog::updateSummary()
{
    const QString path = m_presetDir->text().trimmed();
    if(path.isEmpty()) {
        m_summary->setText(tr("No user preset library is configured."));
        return;
    }
    if(!QDir{path}.exists()) {
        m_summary->setText(tr("Folder does not exist."));
        return;
    }

    ProjectMPresetLibrary library;
    library.setRootDir(path);
    library.setRecursive(m_scanRecursive->isChecked());
    library.scan();
    m_summary->setText(tr("%1 presets found.").arg(library.presetCount()));
}
} // namespace Fooyin::ProjectM

#include "moc_projectmconfigdialog.cpp"
