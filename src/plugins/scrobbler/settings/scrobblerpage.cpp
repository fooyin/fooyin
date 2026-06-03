/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "customservicedialog.h"
#include "scrobbler.h"
#include "scrobblerconstants.h"
#include "scrobblersettings.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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
    void updateWidgetState();

    SettingsManager* m_settings;

    QCheckBox* m_scrobblingEnabled;
    QLabel* m_delayLabel;
    QSpinBox* m_scrobbleDelay;

    QCheckBox* m_filterScrobbles;
    QLabel* m_filterLabel;
    ScriptLineEdit* m_scrobbleFilter;

    QLabel* m_titleLabel;
    ScriptLineEdit* m_titleParam;
    QLabel* m_artistLabel;
    ScriptLineEdit* m_artistParam;
    QLabel* m_albumLabel;
    ScriptLineEdit* m_albumParam;
    QCheckBox* m_sendAlbumArtist;
    ScriptLineEdit* m_albumArtistParam;
};

ScrobblerPageWidget::ScrobblerPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_scrobblingEnabled{new QCheckBox(tr("Enable scrobbling"), this)}
    , m_delayLabel{new QLabel(tr("Scrobble delay") + ":"_L1, this)}
    , m_scrobbleDelay{new QSpinBox(this)}
    , m_filterScrobbles{new QCheckBox(tr("Filter scrobbles"), this)}
    , m_filterLabel{new QLabel(tr("Query") + ":"_L1, this)}
    , m_scrobbleFilter{new ScriptLineEdit(this)}
    , m_titleLabel{new QLabel(tr("Title") + ":"_L1, this)}
    , m_titleParam{new ScriptLineEdit(this)}
    , m_artistLabel{new QLabel(tr("Artist") + ":"_L1, this)}
    , m_artistParam{new ScriptLineEdit(this)}
    , m_albumLabel{new QLabel(tr("Album") + ":"_L1, this)}
    , m_albumParam{new ScriptLineEdit(this)}
    , m_sendAlbumArtist{new QCheckBox(tr("Album Artist"), this)}
    , m_albumArtistParam{new ScriptLineEdit(this)}
{
    const QString delayTip = tr("Time to wait before submitting scrobbles");

    m_delayLabel->setToolTip(delayTip);
    m_scrobbleDelay->setToolTip(delayTip);

    m_scrobbleDelay->setRange(0, 600);
    m_scrobbleDelay->setSuffix(u" s"_s);
    m_scrobbleDelay->setMaximumWidth(120);

    const QString filterTip = tr("Enter a query - tracks that match the query will NOT be scrobbled");
    m_filterLabel->setToolTip(filterTip);
    m_scrobbleFilter->setToolTip(filterTip);

    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    auto* delayLayout = new QHBoxLayout();
    delayLayout->setContentsMargins(0, 0, 0, 0);
    delayLayout->addWidget(m_scrobbleDelay);
    delayLayout->addStretch();

    int row{0};
    generalLayout->addWidget(m_scrobblingEnabled, row++, 0, 1, 2);
    generalLayout->addWidget(m_delayLabel, row, 0);
    generalLayout->addLayout(delayLayout, row++, 1);
    generalLayout->setColumnStretch(1, 1);

    auto* filterGroup  = new QGroupBox(tr("Filtering"), this);
    auto* filterLayout = new QGridLayout(filterGroup);

    row = 0;
    filterLayout->addWidget(m_filterScrobbles, row++, 0, 1, 2);
    filterLayout->addWidget(m_filterLabel, row, 0);
    filterLayout->addWidget(m_scrobbleFilter, row++, 1);
    filterLayout->setColumnStretch(1, 1);

    auto* paramsGroup  = new QGroupBox(tr("Fields"), this);
    auto* paramsLayout = new QGridLayout(paramsGroup);

    row = 0;
    paramsLayout->addWidget(m_titleLabel, row, 0);
    paramsLayout->addWidget(m_titleParam, row++, 1);
    paramsLayout->addWidget(m_artistLabel, row, 0);
    paramsLayout->addWidget(m_artistParam, row++, 1);
    paramsLayout->addWidget(m_albumLabel, row, 0);
    paramsLayout->addWidget(m_albumParam, row++, 1);
    paramsLayout->addWidget(m_sendAlbumArtist, row, 0);
    paramsLayout->addWidget(m_albumArtistParam, row++, 1);
    paramsLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0, 1, 2);
    layout->addWidget(filterGroup, row++, 0, 1, 2);
    layout->addWidget(paramsGroup, row++, 0, 1, 2);
    layout->setRowStretch(layout->rowCount(), 1);

    QObject::connect(m_scrobblingEnabled, &QCheckBox::toggled, this, &ScrobblerPageWidget::updateWidgetState);
    QObject::connect(m_filterScrobbles, &QCheckBox::toggled, this, &ScrobblerPageWidget::updateWidgetState);
    QObject::connect(m_sendAlbumArtist, &QCheckBox::toggled, this, &ScrobblerPageWidget::updateWidgetState);
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

    m_filterScrobbles->setChecked(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>());
    m_scrobbleFilter->setText(m_settings->value<Settings::Scrobbler::ScrobbleFilter>());

    updateWidgetState();
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

    m_settings->set<Settings::Scrobbler::EnableScrobbleFilter>(m_filterScrobbles->isChecked());
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

void ScrobblerPageWidget::updateWidgetState()
{
    const bool enabled = m_scrobblingEnabled->isChecked();

    m_delayLabel->setEnabled(enabled);
    m_scrobbleDelay->setEnabled(enabled);
    m_filterScrobbles->setEnabled(enabled);

    const bool filterEnabled = enabled && m_filterScrobbles->isChecked();
    m_filterLabel->setEnabled(filterEnabled);
    m_scrobbleFilter->setEnabled(filterEnabled);

    m_titleLabel->setEnabled(enabled);
    m_titleParam->setEnabled(enabled);
    m_artistLabel->setEnabled(enabled);
    m_artistParam->setEnabled(enabled);
    m_albumLabel->setEnabled(enabled);
    m_albumParam->setEnabled(enabled);
    m_sendAlbumArtist->setEnabled(enabled);
    m_albumArtistParam->setEnabled(enabled && m_sendAlbumArtist->isChecked());
}

ScrobblerPage::ScrobblerPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::General);
    setName(tr("General"));
    setCategory({tr("Integrations"), tr("Scrobbling")});
    setWidgetCreator([settings] { return new ScrobblerPageWidget(settings); });
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblerpage.cpp"
#include "scrobblerpage.moc"
