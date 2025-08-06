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

#include "artworksearchingpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ArtworkSearchingPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkSearchingPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_autoSearch;

    QLineEdit* m_titleParam;
    QLineEdit* m_artistParam;
    QLineEdit* m_albumParam;
    SliderEditor* m_matchThreshold;
};

ArtworkSearchingPageWidget::ArtworkSearchingPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_autoSearch{new QCheckBox(tr("Automatically search for missing artwork on starting playback"), this)}
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

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(m_autoSearch, row++, 0);
    layout->addWidget(paramsGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void ArtworkSearchingPageWidget::load()
{
    m_autoSearch->setChecked(m_settings->value<Settings::Gui::Internal::ArtworkAutoSearch>());
    m_titleParam->setText(m_settings->value<Settings::Gui::Internal::ArtworkTitleField>());
    m_artistParam->setText(m_settings->value<Settings::Gui::Internal::ArtworkArtistField>());
    m_albumParam->setText(m_settings->value<Settings::Gui::Internal::ArtworkAlbumField>());
    m_matchThreshold->setValue(m_settings->value<Settings::Gui::Internal::ArtworkMatchThreshold>());
}

void ArtworkSearchingPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::ArtworkAutoSearch>(m_autoSearch->isChecked());
    m_settings->set<Settings::Gui::Internal::ArtworkTitleField>(m_titleParam->text());
    m_settings->set<Settings::Gui::Internal::ArtworkArtistField>(m_artistParam->text());
    m_settings->set<Settings::Gui::Internal::ArtworkAlbumField>(m_albumParam->text());
    m_settings->set<Settings::Gui::Internal::ArtworkMatchThreshold>(m_matchThreshold->value());
}

void ArtworkSearchingPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::ArtworkAutoSearch>();
    m_settings->reset<Settings::Gui::Internal::ArtworkTitleField>();
    m_settings->reset<Settings::Gui::Internal::ArtworkArtistField>();
    m_settings->reset<Settings::Gui::Internal::ArtworkAlbumField>();
    m_settings->reset<Settings::Gui::Internal::ArtworkMatchThreshold>();
}

ArtworkSearchingPage::ArtworkSearchingPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ArtworkSearching);
    setName(tr("Searching"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([settings] { return new ArtworkSearchingPageWidget(settings); });
}
} // namespace Fooyin

#include "artworksearchingpage.moc"
#include "moc_artworksearchingpage.cpp"
