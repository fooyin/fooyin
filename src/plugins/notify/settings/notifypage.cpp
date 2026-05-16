/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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

#include "notifypage.h"

#include "notifyplugin.h"
#include "notifysettings.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Notify {
class NotifyPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit NotifyPageWidget(SettingsManager* settings, NotifyPlugin* plugin);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateAvailability();
    void updateWidgetState();
    [[nodiscard]] PlaybackControls playbackControls() const;
    void setPlaybackControls(PlaybackControls controls);

    SettingsManager* m_settings;
    NotifyPlugin* m_plugin;

    QCheckBox* m_enable;
    QLabel* m_titleLabel;
    ScriptLineEdit* m_titleField;
    QLabel* m_bodyLabel;
    ScriptLineEdit* m_bodyField;
    QCheckBox* m_showAlbumArt;
    QCheckBox* m_showPrevious;
    QCheckBox* m_showPlayPause;
    QCheckBox* m_showNext;
    QLabel* m_timeoutLabel;
    QSpinBox* m_timeout;
};

NotifyPageWidget::NotifyPageWidget(SettingsManager* settings, NotifyPlugin* plugin)
    : m_settings{settings}
    , m_plugin{plugin}
    , m_enable{new QCheckBox(tr("Enable notifications"), this)}
    , m_titleLabel{new QLabel(tr("Title") + ":"_L1, this)}
    , m_titleField{new ScriptLineEdit(this)}
    , m_bodyLabel{new QLabel(tr("Body") + ":"_L1, this)}
    , m_bodyField{new ScriptLineEdit(this)}
    , m_showAlbumArt{new QCheckBox(tr("Show album art"), this)}
    , m_showPrevious{new QCheckBox(tr("Show previous"), this)}
    , m_showPlayPause{new QCheckBox(tr("Show play/pause"), this)}
    , m_showNext{new QCheckBox(tr("Show next"), this)}
    , m_timeoutLabel{new QLabel(tr("Timeout") + ":"_L1, this)}
    , m_timeout{new QSpinBox(this)}
{
    auto* generalGroup   = new QGroupBox(tr("General"), this);
    auto* generalLayout  = new QGridLayout(generalGroup);
    auto* fieldsGroup    = new QGroupBox(tr("Content"), this);
    auto* fieldsLayout   = new QGridLayout(fieldsGroup);
    auto* controlsGroup  = new QGroupBox(tr("Playback Controls"), this);
    auto* controlsLayout = new QVBoxLayout(controlsGroup);

    controlsLayout->addWidget(m_showPrevious);
    controlsLayout->addWidget(m_showPlayPause);
    controlsLayout->addWidget(m_showNext);

    int row{0};
    fieldsLayout->addWidget(m_titleLabel, row, 0);
    fieldsLayout->addWidget(m_titleField, row++, 1);
    fieldsLayout->addWidget(m_bodyLabel, row, 0);
    fieldsLayout->addWidget(m_bodyField, row++, 1);
    fieldsLayout->addWidget(m_showAlbumArt, row++, 0, 1, 2);
    fieldsLayout->setColumnStretch(1, 1);

    m_timeout->setRange(-1, 60000);
    m_timeout->setSuffix(u" ms"_s);
    m_timeout->setSpecialValueText(tr("System default"));
    m_timeout->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    m_timeout->setAccelerated(true);
    m_timeout->setToolTip(tr("Notification display time in milliseconds"));

    auto* layout        = new QGridLayout(this);
    auto* timeoutLayout = new QHBoxLayout();
    timeoutLayout->setContentsMargins(0, 0, 0, 0);
    timeoutLayout->addWidget(m_timeout);
    timeoutLayout->addStretch();

    row = 0;
    generalLayout->addWidget(m_enable, row++, 0, 1, 2);
    generalLayout->addWidget(m_timeoutLabel, row, 0);
    generalLayout->addLayout(timeoutLayout, row++, 1);
    generalLayout->setColumnStretch(1, 1);

    row = 0;
    layout->addWidget(generalGroup, row++, 0, 1, 2);
    layout->addWidget(fieldsGroup, row++, 0, 1, 2);
    layout->addWidget(controlsGroup, row++, 0, 1, 2);
    layout->setRowStretch(row, 1);

    QObject::connect(m_enable, &QCheckBox::toggled, this, &NotifyPageWidget::updateWidgetState);
}

PlaybackControls NotifyPageWidget::playbackControls() const
{
    PlaybackControls controls;
    if(m_showPrevious->isChecked()) {
        controls.setFlag(PlaybackControlFlag::Previous);
    }
    if(m_showPlayPause->isChecked()) {
        controls.setFlag(PlaybackControlFlag::PlayPause);
    }
    if(m_showNext->isChecked()) {
        controls.setFlag(PlaybackControlFlag::Next);
    }
    return controls;
}

void NotifyPageWidget::setPlaybackControls(PlaybackControls controls)
{
    m_showPrevious->setChecked(controls.testFlag(PlaybackControlFlag::Previous));
    m_showPlayPause->setChecked(controls.testFlag(PlaybackControlFlag::PlayPause));
    m_showNext->setChecked(controls.testFlag(PlaybackControlFlag::Next));
}

void NotifyPageWidget::updateAvailability()
{
    updateWidgetState();
}

void NotifyPageWidget::updateWidgetState()
{
    const bool albumArtSupported = m_plugin && m_plugin->supportsAlbumArt();
    const bool controlsSupported = m_plugin && m_plugin->supportsPlaybackControls();
    const bool timeoutSupported  = m_plugin && m_plugin->supportsTimeout();
    const bool enabled           = m_enable->isChecked();

    const QString albumArtTooltip
        = albumArtSupported ? QString{} : tr("Album art is not available for the active notification backend");
    const QString controlsTooltip
        = controlsSupported ? QString{} : tr("Playback controls are not available for the active notification backend");
    const QString timeoutTooltip = timeoutSupported
                                     ? tr("Notification display time in milliseconds")
                                     : tr("Notification timeout is not available for the active notification backend");

    m_titleLabel->setEnabled(enabled);
    m_titleField->setEnabled(enabled);
    m_bodyLabel->setEnabled(enabled);
    m_bodyField->setEnabled(enabled);

    m_showAlbumArt->setEnabled(enabled && albumArtSupported);
    m_showAlbumArt->setToolTip(albumArtTooltip);

    m_showPrevious->setEnabled(enabled && controlsSupported);
    m_showPrevious->setToolTip(controlsTooltip);
    m_showPlayPause->setEnabled(enabled && controlsSupported);
    m_showPlayPause->setToolTip(controlsTooltip);
    m_showNext->setEnabled(enabled && controlsSupported);
    m_showNext->setToolTip(controlsTooltip);

    m_timeoutLabel->setEnabled(enabled && timeoutSupported);
    m_timeoutLabel->setToolTip(timeoutTooltip);
    m_timeout->setEnabled(enabled && timeoutSupported);
    m_timeout->setToolTip(timeoutTooltip);
}

void NotifyPageWidget::load()
{
    m_enable->setChecked(m_settings->value<Settings::Notify::Enabled>());
    m_titleField->setText(m_settings->value<Settings::Notify::TitleField>());
    m_bodyField->setText(m_settings->value<Settings::Notify::BodyField>());
    m_showAlbumArt->setChecked(m_settings->value<Settings::Notify::ShowAlbumArt>());
    setPlaybackControls(PlaybackControls::fromInt(m_settings->value<Settings::Notify::Controls>()));
    m_timeout->setValue(m_settings->value<Settings::Notify::Timeout>());
    updateAvailability();
}

void NotifyPageWidget::apply()
{
    m_settings->set<Settings::Notify::Enabled>(m_enable->isChecked());
    m_settings->set<Settings::Notify::TitleField>(m_titleField->text());
    m_settings->set<Settings::Notify::BodyField>(m_bodyField->text());
    m_settings->set<Settings::Notify::ShowAlbumArt>(m_showAlbumArt->isChecked());
    m_settings->set<Settings::Notify::Controls>(playbackControls().toInt());
    m_settings->set<Settings::Notify::Timeout>(m_timeout->value());
}

void NotifyPageWidget::reset()
{
    m_settings->reset<Settings::Notify::Enabled>();
    m_settings->reset<Settings::Notify::TitleField>();
    m_settings->reset<Settings::Notify::BodyField>();
    m_settings->reset<Settings::Notify::ShowAlbumArt>();
    m_settings->reset<Settings::Notify::Controls>();
    m_settings->reset<Settings::Notify::Timeout>();
}

NotifyPage::NotifyPage(SettingsManager* settings, NotifyPlugin* plugin, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId("Fooyin.Page.Integrations.Notifications");
    setName(tr("General"));
    setCategory({tr("Integrations"), tr("Notifications")});
    setWidgetCreator([settings, plugin] { return new NotifyPageWidget(settings, plugin); });
}
} // namespace Fooyin::Notify

#include "moc_notifypage.cpp"
#include "notifypage.moc"
