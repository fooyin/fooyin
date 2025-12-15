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

#include "scrobblerpage.h"

#include "../customservicedialog.h"
#include "../scrobbler.h"
#include "scrobblersettings.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::Scrobbler {
class ScrobblerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ScrobblerPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_scrobblingEnabled;
    QSpinBox* m_scrobbleDelay;

    QGroupBox* m_filterScrobblesGroup;
    ScriptLineEdit* m_scrobbleFilter;

    QLineEdit* m_titleParam;
    QLineEdit* m_artistParam;
    QLineEdit* m_albumParam;
    QCheckBox* m_sendAlbumArtist;
    QLineEdit* m_albumArtistParam;
};

ScrobblerPageWidget::ScrobblerPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_scrobblingEnabled{new QCheckBox(tr("Enable scrobbling"), this)}
    , m_scrobbleDelay{new QSpinBox(this)}
    , m_filterScrobblesGroup{new QGroupBox(tr("Filter scrobbles"), this)}
    , m_scrobbleFilter{new ScriptLineEdit(this)}
    , m_titleParam{new QLineEdit(this)}
    , m_artistParam{new QLineEdit(this)}
    , m_albumParam{new QLineEdit(this)}
    , m_sendAlbumArtist{new QCheckBox(tr("Album Artist"), this)}
    , m_albumArtistParam{new QLineEdit(this)}
{
    auto* delayLabel       = new QLabel(tr("Scrobble delay") + ":"_L1, this);
    const QString delayTip = tr("Time to wait before submitting scrobbles");

    delayLabel->setToolTip(delayTip);
    m_scrobbleDelay->setToolTip(delayTip);

    m_scrobbleDelay->setRange(0, 600);
    m_scrobbleDelay->setSuffix(" "_L1 + tr("seconds"));

    auto* filterLabel       = new QLabel(tr("Query") + ":"_L1, this);
    const QString filterTip = tr("Enter a query - tracks that match the query will NOT be scrobbled");
    filterLabel->setToolTip(filterTip);
    m_scrobbleFilter->setToolTip(filterTip);

    int row{0};
    auto* filterLayout = new QGridLayout(m_filterScrobblesGroup);
    filterLayout->addWidget(filterLabel, row, 0, 1, 1);
    filterLayout->addWidget(m_scrobbleFilter, row++, 1, 1, 3);

    m_filterScrobblesGroup->setCheckable(true);

    auto* paramsGroup  = new QGroupBox(tr("Fields"), this);
    auto* paramsLayout = new QGridLayout(paramsGroup);

    row = 0;
    paramsLayout->addWidget(new QLabel(tr("Title") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_titleParam, row++, 1);
    paramsLayout->addWidget(new QLabel(tr("Artist") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_artistParam, row++, 1);
    paramsLayout->addWidget(new QLabel(tr("Album") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_albumParam, row++, 1);
    paramsLayout->addWidget(m_sendAlbumArtist, row, 0);
    paramsLayout->addWidget(m_albumArtistParam, row++, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(m_scrobblingEnabled, row++, 0, 1, 3);
    layout->addWidget(delayLabel, row, 0, 1, 2);
    layout->addWidget(m_scrobbleDelay, row++, 2);

    layout->addWidget(m_filterScrobblesGroup, row++, 0, 1, 3);

    layout->addWidget(paramsGroup, row++, 0, 1, 3);
    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);

    QObject::connect(m_filterScrobblesGroup, &QGroupBox::clicked, m_scrobbleFilter, &QWidget::setEnabled);
    QObject::connect(m_sendAlbumArtist, &QCheckBox::clicked, m_albumArtistParam, &QWidget::setEnabled);
}

void ScrobblerPageWidget::load()
{
    m_scrobblingEnabled->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    m_scrobbleDelay->setValue(m_settings->value<Settings::Scrobbler::ScrobblingDelay>());

    m_titleParam->setText(m_settings->value<Settings::Scrobbler::TitleField>());
    m_artistParam->setText(m_settings->value<Settings::Scrobbler::ArtistField>());
    m_albumParam->setText(m_settings->value<Settings::Scrobbler::AlbumField>());
    m_sendAlbumArtist->setChecked(m_settings->value<Settings::Scrobbler::SendAlbumArtist>());
    m_albumArtistParam->setText(m_settings->value<Settings::Scrobbler::AlbumArtistField>());
    m_albumArtistParam->setEnabled(m_sendAlbumArtist->isChecked());

    m_filterScrobblesGroup->setChecked(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>());
    m_scrobbleFilter->setText(m_settings->value<Settings::Scrobbler::ScrobbleFilter>());

    m_scrobbleFilter->setEnabled(m_filterScrobblesGroup->isChecked());
}

void ScrobblerPageWidget::apply()
{
    m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(m_scrobblingEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobblingDelay>(m_scrobbleDelay->value());

    m_settings->set<Settings::Scrobbler::TitleField>(m_titleParam->text());
    m_settings->set<Settings::Scrobbler::ArtistField>(m_artistParam->text());
    m_settings->set<Settings::Scrobbler::AlbumField>(m_albumParam->text());
    m_settings->set<Settings::Scrobbler::SendAlbumArtist>(m_sendAlbumArtist->isChecked());
    m_settings->set<Settings::Scrobbler::AlbumArtistField>(m_albumArtistParam->text());

    m_settings->set<Settings::Scrobbler::EnableScrobbleFilter>(m_filterScrobblesGroup->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobbleFilter>(m_scrobbleFilter->text());
}

void ScrobblerPageWidget::reset()
{
    m_settings->reset<Settings::Scrobbler::ScrobblingEnabled>();
    m_settings->reset<Settings::Scrobbler::ScrobblingDelay>();

    m_settings->reset<Settings::Scrobbler::TitleField>();
    m_settings->reset<Settings::Scrobbler::ArtistField>();
    m_settings->reset<Settings::Scrobbler::AlbumField>();
    m_settings->reset<Settings::Scrobbler::SendAlbumArtist>();
    m_settings->reset<Settings::Scrobbler::AlbumArtistField>();

    m_settings->reset<Settings::Scrobbler::EnableScrobbleFilter>();
    m_settings->reset<Settings::Scrobbler::ScrobbleFilter>();
}

ScrobblerPage::ScrobblerPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId({"Fooyin.Page.Network.Scrobbling.General"});
    setName(tr("General"));
    setCategory({tr("Networking"), tr("Scrobbling")});
    setWidgetCreator([settings] { return new ScrobblerPageWidget(settings); });
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblerpage.cpp"
#include "scrobblerpage.moc"
