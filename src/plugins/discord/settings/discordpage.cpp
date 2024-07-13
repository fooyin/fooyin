/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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
    SettingsManager* m_settings;

    QCheckBox* m_enable;
    QCheckBox* m_showPlayState;
    QLineEdit* m_clientId;

    QLineEdit* m_titleField;
    QLineEdit* m_artistField;
    QLineEdit* m_albumField;
};

DiscordPageWidget::DiscordPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enable{new QCheckBox(tr("Enable"), this)}
    , m_showPlayState{new QCheckBox(tr("Show playstate"), this)}
    , m_clientId{new QLineEdit(this)}
    , m_titleField{new QLineEdit(this)}
    , m_artistField{new QLineEdit(this)}
    , m_albumField{new QLineEdit(this)}
{
    auto* fieldsGroup  = new QGroupBox(tr("Fields"), this);
    auto* fieldsLayout = new QGridLayout(fieldsGroup);

    int row{0};
    fieldsLayout->addWidget(new QLabel(tr("Title") + ":"_L1, this), row, 0);
    fieldsLayout->addWidget(m_titleField, row++, 1);
    fieldsLayout->addWidget(new QLabel(tr("Artist") + ":"_L1, this), row, 0);
    fieldsLayout->addWidget(m_artistField, row++, 1);
    fieldsLayout->addWidget(new QLabel(tr("Album") + ":"_L1, this), row, 0);
    fieldsLayout->addWidget(m_albumField, row++, 1);

    auto* layout = new QGridLayout(this);

    m_showPlayState->setToolTip(tr("Display a small icon in Discord showing the current playback state"));

    row = 0;
    layout->addWidget(m_enable, row++, 0, 1, 2);
    layout->addWidget(m_showPlayState, row++, 0, 1, 2);
    layout->addWidget(new QLabel(tr("Client ID") + ":"_L1, this), row, 0);
    layout->addWidget(m_clientId, row++, 1);
    layout->addWidget(fieldsGroup, row++, 0, 1, 2);
    layout->setRowStretch(row, 1);
}

void DiscordPageWidget::load()
{
    m_enable->setChecked(m_settings->value<Settings::Discord::DiscordEnabled>());
    m_showPlayState->setChecked(m_settings->value<Settings::Discord::ShowStateIcon>());
    m_clientId->setText(m_settings->value<Settings::Discord::ClientId>());
    m_titleField->setText(m_settings->value<Settings::Discord::TitleField>());
    m_artistField->setText(m_settings->value<Settings::Discord::ArtistField>());
    m_albumField->setText(m_settings->value<Settings::Discord::AlbumField>());
}

void DiscordPageWidget::apply()
{
    m_settings->set<Settings::Discord::DiscordEnabled>(m_enable->isChecked());
    m_settings->set<Settings::Discord::ShowStateIcon>(m_showPlayState->isChecked());
    m_settings->set<Settings::Discord::ClientId>(m_clientId->text());
    m_settings->set<Settings::Discord::TitleField>(m_titleField->text());
    m_settings->set<Settings::Discord::ArtistField>(m_artistField->text());
    m_settings->set<Settings::Discord::AlbumField>(m_albumField->text());
}

void DiscordPageWidget::reset()
{
    m_settings->reset<Settings::Discord::DiscordEnabled>();
    m_settings->reset<Settings::Discord::ShowStateIcon>();
    m_settings->reset<Settings::Discord::ClientId>();
    m_settings->reset<Settings::Discord::TitleField>();
    m_settings->reset<Settings::Discord::ArtistField>();
    m_settings->reset<Settings::Discord::AlbumField>();
}

DiscordPage::DiscordPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId("Fooyin.Page.Network.Discord");
    setName(tr("General"));
    setCategory({tr("Discord")});
    setWidgetCreator([settings] { return new DiscordPageWidget(settings); });
}
} // namespace Fooyin::Discord

#include "discordpage.moc"
#include "moc_discordpage.cpp"
