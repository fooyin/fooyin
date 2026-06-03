/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "discordpage.h"

#include "discordsettings.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>

using namespace Qt::StringLiterals;

namespace Fooyin::Discord {
class DiscordPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DiscordPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateWidgetState();

    SettingsManager* m_settings;

    QCheckBox* m_enable;
    QCheckBox* m_showPlayState;
    QCheckBox* m_clearOnPause;
    QLabel* m_clientIdLabel;
    QLineEdit* m_clientId;

    QLabel* m_titleLabel;
    ScriptLineEdit* m_titleField;
    QLabel* m_artistLabel;
    ScriptLineEdit* m_artistField;
    QLabel* m_albumLabel;
    ScriptLineEdit* m_albumField;
};

DiscordPageWidget::DiscordPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enable{new QCheckBox(tr("Enable"), this)}
    , m_showPlayState{new QCheckBox(tr("Show playstate"), this)}
    , m_clearOnPause{new QCheckBox(tr("Clear on pause"), this)}
    , m_clientIdLabel{new QLabel(tr("Client ID") + ":"_L1, this)}
    , m_clientId{new QLineEdit(this)}
    , m_titleLabel{new QLabel(tr("Title") + ":"_L1, this)}
    , m_titleField{new ScriptLineEdit(this)}
    , m_artistLabel{new QLabel(tr("Artist") + ":"_L1, this)}
    , m_artistField{new ScriptLineEdit(this)}
    , m_albumLabel{new QLabel(tr("Album") + ":"_L1, this)}
    , m_albumField{new ScriptLineEdit(this)}
{
    auto* connectionGroup  = new QGroupBox(tr("Connection"), this);
    auto* connectionLayout = new QGridLayout(connectionGroup);

    int row = 0;
    connectionLayout->addWidget(m_enable, row++, 0, 1, 2);
    connectionLayout->addWidget(m_clientIdLabel, row, 0);
    connectionLayout->addWidget(m_clientId, row++, 1);
    connectionLayout->setColumnStretch(1, 1);

    auto* presenceGroup  = new QGroupBox(tr("Rich Presence"), this);
    auto* presenceLayout = new QGridLayout(presenceGroup);

    row = 0;
    presenceLayout->addWidget(m_showPlayState, row++, 0, 1, 2);
    presenceLayout->addWidget(m_clearOnPause, row++, 0, 1, 2);
    presenceLayout->addWidget(m_titleLabel, row, 0);
    presenceLayout->addWidget(m_titleField, row++, 1);
    presenceLayout->addWidget(m_artistLabel, row, 0);
    presenceLayout->addWidget(m_artistField, row++, 1);
    presenceLayout->addWidget(m_albumLabel, row, 0);
    presenceLayout->addWidget(m_albumField, row++, 1);
    presenceLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    m_showPlayState->setToolTip(tr("Display a small icon in Discord showing the current playback state"));
    m_clearOnPause->setToolTip(tr("Clear status when paused"));

    row = 0;
    layout->addWidget(connectionGroup, row++, 0, 1, 2);
    layout->addWidget(presenceGroup, row++, 0, 1, 2);
    layout->setRowStretch(row, 1);

    QObject::connect(m_enable, &QCheckBox::toggled, this, &DiscordPageWidget::updateWidgetState);
}

void DiscordPageWidget::load()
{
    m_enable->setChecked(m_settings->value<Settings::Discord::DiscordEnabled>());
    m_showPlayState->setChecked(m_settings->value<Settings::Discord::ShowStateIcon>());
    m_clearOnPause->setChecked(m_settings->value<Settings::Discord::ClearOnPause>());
    m_clientId->setText(m_settings->value<Settings::Discord::ClientId>());
    m_titleField->setText(m_settings->value<Settings::Discord::TitleField>());
    m_artistField->setText(m_settings->value<Settings::Discord::ArtistField>());
    m_albumField->setText(m_settings->value<Settings::Discord::AlbumField>());

    updateWidgetState();
}

void DiscordPageWidget::apply()
{
    m_settings->set<Settings::Discord::DiscordEnabled>(m_enable->isChecked());
    m_settings->set<Settings::Discord::ShowStateIcon>(m_showPlayState->isChecked());
    m_settings->set<Settings::Discord::ClearOnPause>(m_clearOnPause->isChecked());
    m_settings->set<Settings::Discord::ClientId>(m_clientId->text());
    m_settings->set<Settings::Discord::TitleField>(m_titleField->text());
    m_settings->set<Settings::Discord::ArtistField>(m_artistField->text());
    m_settings->set<Settings::Discord::AlbumField>(m_albumField->text());
}

void DiscordPageWidget::reset()
{
    m_settings->reset<Settings::Discord::DiscordEnabled>();
    m_settings->reset<Settings::Discord::ShowStateIcon>();
    m_settings->reset<Settings::Discord::ClearOnPause>();
    m_settings->reset<Settings::Discord::ClientId>();
    m_settings->reset<Settings::Discord::TitleField>();
    m_settings->reset<Settings::Discord::ArtistField>();
    m_settings->reset<Settings::Discord::AlbumField>();
}

void DiscordPageWidget::updateWidgetState()
{
    const bool enabled = m_enable->isChecked();

    m_clientIdLabel->setEnabled(enabled);
    m_clientId->setEnabled(enabled);
    m_showPlayState->setEnabled(enabled);
    m_clearOnPause->setEnabled(enabled);
    m_titleLabel->setEnabled(enabled);
    m_titleField->setEnabled(enabled);
    m_artistLabel->setEnabled(enabled);
    m_artistField->setEnabled(enabled);
    m_albumLabel->setEnabled(enabled);
    m_albumField->setEnabled(enabled);
}

DiscordPage::DiscordPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId("Fooyin.Page.Integrations.Discord");
    setName(tr("General"));
    setCategory({tr("Integrations"), tr("Discord")});
    setWidgetCreator([settings] { return new DiscordPageWidget(settings); });
}
} // namespace Fooyin::Discord

#include "discordpage.moc"
#include "moc_discordpage.cpp"
