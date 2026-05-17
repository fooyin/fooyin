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

#include "nowplayingoutputpage.h"

#include "internalguisettings.h"
#include "nowplayingoutput/nowplayingoutputservice.h"

#include <core/player/playercontroller.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class NowPlayingOutputPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    NowPlayingOutputPageWidget(PlayerController* playerController, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateWidgetState();
    void updatePreview();
    void browseOutputFile();

    [[nodiscard]] NowPlayingOutputService::UpdateEvents updateEvents() const;
    void setUpdateEvents(NowPlayingOutputService::UpdateEvents events);
    [[nodiscard]] NowPlayingOutputService::OutputTargets outputTargets() const;
    void setOutputTargets(NowPlayingOutputService::OutputTargets targets);
    [[nodiscard]] NowPlayingOutputService::OutputOptions outputOptions() const;
    void setOutputOptions(NowPlayingOutputService::OutputOptions options);

    SettingsManager* m_settings;
    PlayerController* m_playerController;
    ScriptParser m_scriptParser;

    QCheckBox* m_enabled;
    ScriptTextEdit* m_script;
    QPlainTextEdit* m_preview;
    QCheckBox* m_updateOnTrackChange;
    QCheckBox* m_updateOnPlayPause;
    QCheckBox* m_updateOnStop;
    QCheckBox* m_updateEverySecond;
    QCheckBox* m_copyToClipboard;
    QCheckBox* m_writeToFile;
    QLabel* m_outputFilePathLabel;
    QLineEdit* m_outputFilePath;
    QCheckBox* m_appendToFile;
    QLabel* m_appendLineLimitLabel;
    QSpinBox* m_appendLineLimit;
};

NowPlayingOutputPageWidget::NowPlayingOutputPageWidget(PlayerController* playerController, SettingsManager* settings)
    : m_settings{settings}
    , m_playerController{playerController}
    , m_enabled{new QCheckBox(tr("Enabled"), this)}
    , m_script{new ScriptTextEdit(this)}
    , m_preview{new QPlainTextEdit(this)}
    , m_updateOnTrackChange{new QCheckBox(tr("Track changes"), this)}
    , m_updateOnPlayPause{new QCheckBox(tr("Play and pause"), this)}
    , m_updateOnStop{new QCheckBox(tr("Stop"), this)}
    , m_updateEverySecond{new QCheckBox(tr("Every second"), this)}
    , m_copyToClipboard{new QCheckBox(tr("Clipboard"), this)}
    , m_writeToFile{new QCheckBox(tr("File"), this)}
    , m_outputFilePathLabel{new QLabel(tr("Path") + ":"_L1, this)}
    , m_outputFilePath{new QLineEdit(this)}
    , m_appendToFile{new QCheckBox(tr("Append instead of overwrite"), this)}
    , m_appendLineLimitLabel{new QLabel(tr("Maximum lines") + ":"_L1, this)}
    , m_appendLineLimit{new QSpinBox(this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    int row{0};
    generalLayout->addWidget(m_enabled, row++, 0);

    auto* scriptGroup  = new QGroupBox(tr("Format"), this);
    auto* scriptLayout = new QGridLayout(scriptGroup);

    m_script->setMinimumHeight(120);
    m_preview->setReadOnly(true);
    m_preview->setMaximumHeight(56);
    m_preview->setWordWrapMode(QTextOption::NoWrap);
    m_preview->setPlaceholderText(tr("No current track"));

    row = 0;
    scriptLayout->addWidget(m_script, row++, 0);
    scriptLayout->addWidget(m_preview, row++, 0);
    scriptLayout->setColumnStretch(0, 1);

    auto* eventsGroup  = new QGroupBox(tr("Update events"), this);
    auto* eventsLayout = new QGridLayout(eventsGroup);

    row = 0;
    eventsLayout->addWidget(m_updateOnTrackChange, row, 0);
    eventsLayout->addWidget(m_updateOnPlayPause, row++, 1);
    eventsLayout->addWidget(m_updateOnStop, row, 0);
    eventsLayout->addWidget(m_updateEverySecond, row++, 1);

    auto* outputGroup  = new QGroupBox(tr("Output"), this);
    auto* outputLayout = new QGridLayout(outputGroup);

    m_appendLineLimit->setRange(0, 1000000);
    m_appendLineLimit->setSpecialValueText(tr("Unlimited"));

    auto* browseAction = new QAction({}, this);
    Gui::setThemeIcon(browseAction, Constants::Icons::Options);
    browseAction->setToolTip(tr("Browse to an output file"));
    QObject::connect(browseAction, &QAction::triggered, this, &NowPlayingOutputPageWidget::browseOutputFile);
    m_outputFilePath->addAction(browseAction, QLineEdit::TrailingPosition);

    auto* filePathLayout = new QHBoxLayout();
    filePathLayout->addWidget(m_outputFilePathLabel);
    filePathLayout->addWidget(m_outputFilePath, 1);

    auto* appendLineLimitLayout = new QHBoxLayout();
    appendLineLimitLayout->addWidget(m_appendLineLimitLabel);
    appendLineLimitLayout->addWidget(m_appendLineLimit);
    appendLineLimitLayout->addStretch(1);

    row = 0;
    outputLayout->addWidget(m_copyToClipboard, row++, 0, 1, 3);
    outputLayout->addWidget(m_writeToFile, row++, 0, 1, 3);
    outputLayout->addItem(new QSpacerItem(10, 0), row, 0);
    outputLayout->addLayout(filePathLayout, row++, 1, 1, 3);
    outputLayout->addItem(new QSpacerItem(10, 0), row, 0);
    outputLayout->addWidget(m_appendToFile, row++, 1, 1, 2);
    outputLayout->addItem(new QSpacerItem(10, 0), row, 0);
    outputLayout->addLayout(appendLineLimitLayout, row++, 1, 1, 3);
    outputLayout->setColumnStretch(3, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0);
    layout->addWidget(scriptGroup, row++, 0);
    layout->addWidget(eventsGroup, row++, 0);
    layout->addWidget(outputGroup, row++, 0);
    layout->setRowStretch(row, 1);

    QObject::connect(m_enabled, &QCheckBox::toggled, this, &NowPlayingOutputPageWidget::updateWidgetState);
    QObject::connect(m_writeToFile, &QCheckBox::toggled, this, &NowPlayingOutputPageWidget::updateWidgetState);
    QObject::connect(m_appendToFile, &QCheckBox::toggled, this, &NowPlayingOutputPageWidget::updateWidgetState);
    QObject::connect(m_script, &QPlainTextEdit::textChanged, this, &NowPlayingOutputPageWidget::updatePreview);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     &NowPlayingOutputPageWidget::updatePreview);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                     &NowPlayingOutputPageWidget::updatePreview);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &NowPlayingOutputPageWidget::updatePreview);
    QObject::connect(m_playerController, &PlayerController::positionChangedSeconds, this,
                     &NowPlayingOutputPageWidget::updatePreview);
}

void NowPlayingOutputPageWidget::load()
{
    using namespace Settings::Gui::Internal;

    m_enabled->setChecked(m_settings->value<NowPlayingOutputEnabled>());
    m_script->setPlainText(m_settings->value<NowPlayingOutputScript>());
    setUpdateEvents(NowPlayingOutputService::UpdateEvents::fromInt(m_settings->value<NowPlayingOutputUpdateEvents>()));
    setOutputTargets(NowPlayingOutputService::OutputTargets::fromInt(m_settings->value<NowPlayingOutputTargets>()));
    m_outputFilePath->setText(m_settings->value<NowPlayingOutputFilePath>());
    setOutputOptions(NowPlayingOutputService::OutputOptions::fromInt(m_settings->value<NowPlayingOutputOptions>()));
    m_appendLineLimit->setValue(m_settings->value<NowPlayingOutputAppendLineLimit>());

    updateWidgetState();
    updatePreview();
}

void NowPlayingOutputPageWidget::apply()
{
    using namespace Settings::Gui::Internal;

    m_settings->set<NowPlayingOutputEnabled>(m_enabled->isChecked());
    m_settings->set<NowPlayingOutputScript>(m_script->toPlainText());
    m_settings->set<NowPlayingOutputUpdateEvents>(static_cast<int>(updateEvents().toInt()));
    m_settings->set<NowPlayingOutputTargets>(static_cast<int>(outputTargets().toInt()));
    m_settings->set<NowPlayingOutputFilePath>(m_outputFilePath->text());
    m_settings->set<NowPlayingOutputOptions>(static_cast<int>(outputOptions().toInt()));
    m_settings->set<NowPlayingOutputAppendLineLimit>(m_appendLineLimit->value());
}

void NowPlayingOutputPageWidget::reset()
{
    using namespace Settings::Gui::Internal;

    m_settings->reset<NowPlayingOutputEnabled>();
    m_settings->reset<NowPlayingOutputScript>();
    m_settings->reset<NowPlayingOutputUpdateEvents>();
    m_settings->reset<NowPlayingOutputTargets>();
    m_settings->reset<NowPlayingOutputFilePath>();
    m_settings->reset<NowPlayingOutputOptions>();
    m_settings->reset<NowPlayingOutputAppendLineLimit>();
}

void NowPlayingOutputPageWidget::updateWidgetState()
{
    const bool enabled      = m_enabled->isChecked();
    const bool writeToFile  = enabled && m_writeToFile->isChecked();
    const bool trimFileMode = writeToFile && m_appendToFile->isChecked();

    m_script->setEnabled(enabled);
    m_preview->setEnabled(enabled);
    m_updateOnTrackChange->setEnabled(enabled);
    m_updateOnPlayPause->setEnabled(enabled);
    m_updateOnStop->setEnabled(enabled);
    m_updateEverySecond->setEnabled(enabled);
    m_copyToClipboard->setEnabled(enabled);
    m_writeToFile->setEnabled(enabled);
    m_outputFilePathLabel->setEnabled(writeToFile);
    m_outputFilePath->setEnabled(writeToFile);
    m_appendToFile->setEnabled(writeToFile);
    m_appendLineLimitLabel->setEnabled(trimFileMode);
    m_appendLineLimit->setEnabled(trimFileMode);
}

void NowPlayingOutputPageWidget::updatePreview()
{
    const Track track = m_playerController ? m_playerController->currentTrack() : Track{};
    if(!track.isValid()) {
        m_preview->clear();
        return;
    }

    auto playbackContext = makePlaybackScriptContext(m_playerController, nullptr, TrackListContextPolicy::Placeholder);
    playbackContext.context.track = &track;
    m_preview->setPlainText(m_scriptParser.evaluate(m_script->toPlainText(), track, playbackContext.context));
}

NowPlayingOutputService::UpdateEvents NowPlayingOutputPageWidget::updateEvents() const
{
    NowPlayingOutputService::UpdateEvents events;
    if(m_updateOnTrackChange->isChecked()) {
        events.setFlag(NowPlayingOutputService::UpdateTrack);
    }
    if(m_updateOnPlayPause->isChecked()) {
        events.setFlag(NowPlayingOutputService::UpdatePlayPause);
    }
    if(m_updateOnStop->isChecked()) {
        events.setFlag(NowPlayingOutputService::UpdateStop);
    }
    if(m_updateEverySecond->isChecked()) {
        events.setFlag(NowPlayingOutputService::UpdateSecond);
    }
    return events;
}

void NowPlayingOutputPageWidget::setUpdateEvents(NowPlayingOutputService::UpdateEvents events)
{
    m_updateOnTrackChange->setChecked(events.testFlag(NowPlayingOutputService::UpdateTrack));
    m_updateOnPlayPause->setChecked(events.testFlag(NowPlayingOutputService::UpdatePlayPause));
    m_updateOnStop->setChecked(events.testFlag(NowPlayingOutputService::UpdateStop));
    m_updateEverySecond->setChecked(events.testFlag(NowPlayingOutputService::UpdateSecond));
}

NowPlayingOutputService::OutputTargets NowPlayingOutputPageWidget::outputTargets() const
{
    NowPlayingOutputService::OutputTargets targets;
    if(m_copyToClipboard->isChecked()) {
        targets.setFlag(NowPlayingOutputService::OutputClipboard);
    }
    if(m_writeToFile->isChecked()) {
        targets.setFlag(NowPlayingOutputService::OutputFile);
    }
    return targets;
}

void NowPlayingOutputPageWidget::setOutputTargets(NowPlayingOutputService::OutputTargets targets)
{
    m_copyToClipboard->setChecked(targets.testFlag(NowPlayingOutputService::OutputClipboard));
    m_writeToFile->setChecked(targets.testFlag(NowPlayingOutputService::OutputFile));
}

NowPlayingOutputService::OutputOptions NowPlayingOutputPageWidget::outputOptions() const
{
    NowPlayingOutputService::OutputOptions options;
    if(m_appendToFile->isChecked()) {
        options.setFlag(NowPlayingOutputService::OutputAppendFile);
    }
    return options;
}

void NowPlayingOutputPageWidget::setOutputOptions(NowPlayingOutputService::OutputOptions options)
{
    m_appendToFile->setChecked(options.testFlag(NowPlayingOutputService::OutputAppendFile));
}

void NowPlayingOutputPageWidget::browseOutputFile()
{
    QFileDialog dialog{this, tr("Now Playing"), m_outputFilePath->text(), tr("Text Files (*.txt);;All Files (*)")};
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontResolveSymlinks);

    if(dialog.exec() == QDialog::Accepted) {
        if(const auto files = dialog.selectedFiles(); !files.empty()) {
            m_outputFilePath->setText(files.front());
        }
    }
}

NowPlayingOutputPage::NowPlayingOutputPage(PlayerController* playerController, SettingsManager* settings,
                                           QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::NowPlaying);
    setName(tr("Now Playing"));
    setCategory({tr("Playback"), tr("Now Playing")});
    setWidgetCreator(
        [playerController, settings] { return new NowPlayingOutputPageWidget(playerController, settings); });
}
} // namespace Fooyin

#include "moc_nowplayingoutputpage.cpp"
#include "nowplayingoutputpage.moc"
