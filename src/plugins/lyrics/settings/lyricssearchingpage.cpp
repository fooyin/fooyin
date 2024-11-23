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

#include "lyricssearchingpage.h"

#include "lyricsconstants.h"
#include "lyricssettings.h"

#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
class LyricsSearchingPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LyricsSearchingPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_autoSearch;
    QCheckBox* m_skipRemaining;
    QCheckBox* m_skipExternal;

    QLineEdit* m_titleParam;
    QLineEdit* m_artistParam;
    QLineEdit* m_albumParam;
    SliderEditor* m_matchThreshold;
};

LyricsSearchingPageWidget::LyricsSearchingPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_autoSearch{new QCheckBox(tr("Automatically search for lyrics on starting playback"), this)}
    , m_skipRemaining{new QCheckBox(tr("Skip remaining sources when lyrics are found"), this)}
    , m_skipExternal{new QCheckBox(tr("Skip external sources if local lyrics are found"), this)}
    , m_titleParam{new QLineEdit(this)}
    , m_artistParam{new QLineEdit(this)}
    , m_albumParam{new QLineEdit(this)}
    , m_matchThreshold{new SliderEditor(tr("Minimum match threshold"), this)}
{
    auto* paramsGroup  = new QGroupBox(tr("Search parameters"), this);
    auto* paramsLayout = new QGridLayout(paramsGroup);

    m_matchThreshold->setRange(0, 100);

    int row{0};
    paramsLayout->addWidget(new QLabel(tr("Title") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_titleParam, row++, 1);
    paramsLayout->addWidget(new QLabel(tr("Artist") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_artistParam, row++, 1);
    paramsLayout->addWidget(new QLabel(tr("Album") + ":"_L1, this), row, 0);
    paramsLayout->addWidget(m_albumParam, row++, 1);
    paramsLayout->addWidget(m_matchThreshold, row++, 0, 1, 2);

    m_autoSearch->setToolTip(tr("Only local lyrics will be used if unchecked"));

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(m_autoSearch, row++, 0);
    layout->addWidget(m_skipRemaining, row++, 0);
    layout->addWidget(m_skipExternal, row++, 0);
    layout->addWidget(paramsGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void LyricsSearchingPageWidget::load()
{
    m_autoSearch->setChecked(m_settings->value<Settings::Lyrics::AutoSearch>());
    m_skipRemaining->setChecked(m_settings->value<Settings::Lyrics::SkipRemaining>());
    m_skipExternal->setChecked(m_settings->value<Settings::Lyrics::SkipExternal>());
    m_titleParam->setText(m_settings->value<Settings::Lyrics::TitleField>());
    m_artistParam->setText(m_settings->value<Settings::Lyrics::ArtistField>());
    m_albumParam->setText(m_settings->value<Settings::Lyrics::AlbumField>());
    m_matchThreshold->setValue(m_settings->value<Settings::Lyrics::MatchThreshold>());
}

void LyricsSearchingPageWidget::apply()
{
    m_settings->set<Settings::Lyrics::AutoSearch>(m_autoSearch->isChecked());
    m_settings->set<Settings::Lyrics::SkipRemaining>(m_skipRemaining->isChecked());
    m_settings->set<Settings::Lyrics::SkipExternal>(m_skipExternal->isChecked());
    m_settings->set<Settings::Lyrics::TitleField>(m_titleParam->text());
    m_settings->set<Settings::Lyrics::ArtistField>(m_artistParam->text());
    m_settings->set<Settings::Lyrics::AlbumField>(m_albumParam->text());
    m_settings->set<Settings::Lyrics::MatchThreshold>(m_matchThreshold->value());
}

void LyricsSearchingPageWidget::reset()
{
    m_settings->reset<Settings::Lyrics::AutoSearch>();
    m_settings->reset<Settings::Lyrics::SkipRemaining>();
    m_settings->reset<Settings::Lyrics::SkipExternal>();
    m_settings->reset<Settings::Lyrics::TitleField>();
    m_settings->reset<Settings::Lyrics::ArtistField>();
    m_settings->reset<Settings::Lyrics::AlbumField>();
    m_settings->reset<Settings::Lyrics::MatchThreshold>();
}

LyricsSearchingPage::LyricsSearchingPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsSearching);
    setName(tr("Searching"));
    setCategory({tr("Lyrics")});
    setWidgetCreator([settings] { return new LyricsSearchingPageWidget(settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricssearchingpage.moc"
#include "moc_lyricssearchingpage.cpp"
