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

#include "librarymetadatapage.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>

namespace Fooyin {
class LibraryMetadataPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryMetadataPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_useVariousCompilations;
    QCheckBox* m_saveRatings;
    QCheckBox* m_savePlaycounts;
    QCheckBox* m_overwriteRatingsOnReload;
    QCheckBox* m_overwritePlaycountsOnReload;
};

LibraryMetadataPageWidget::LibraryMetadataPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_useVariousCompilations{new QCheckBox(tr("Use 'Various Artists' for compilations"), this)}
    , m_saveRatings{new QCheckBox(tr("Save ratings to file tags when possible"), this)}
    , m_savePlaycounts{new QCheckBox(tr("Save playcounts to file tags when possible"), this)}
    , m_overwriteRatingsOnReload{new QCheckBox(tr("Overwrite rating in database when songs are re-read"), this)}
    , m_overwritePlaycountsOnReload{new QCheckBox(tr("Overwrite playcount in database when files are re-read"), this)}
{
    auto* metadataGroup  = new QGroupBox(tr("Metadata"), this);
    auto* metadataLayout = new QGridLayout(metadataGroup);

    int row{0};
    metadataLayout->addWidget(m_useVariousCompilations, row++, 0);

    auto* playbackStatsGroup  = new QGroupBox(tr("Playback Statistics"), this);
    auto* playbackStatsLayout = new QGridLayout(playbackStatsGroup);

    m_overwriteRatingsOnReload->setToolTip(tr(
        "When enabled, a rating found in file tags replaces the database rating.\n"
        "When disabled, the database rating is kept and file tags are only used when the database rating is empty."));
    m_overwritePlaycountsOnReload->setToolTip(
        tr("When enabled, playcount and played timestamps found in file tags replace the database values.\n"
           "Missing values still fall back to the database.\n"
           "When disabled, playcount uses the higher value, first played uses the earlier non-empty value,\n"
           "and last played uses the later value."));

    row = 0;
    playbackStatsLayout->addWidget(m_savePlaycounts, row++, 0);
    playbackStatsLayout->addWidget(m_saveRatings, row++, 0);
    playbackStatsLayout->addWidget(m_overwritePlaycountsOnReload, row++, 0);
    playbackStatsLayout->addWidget(m_overwriteRatingsOnReload, row++, 0);

    auto* mainLayout = new QGridLayout(this);
    row              = 0;
    mainLayout->addWidget(metadataGroup, row++, 0);
    mainLayout->addWidget(playbackStatsGroup, row++, 0);
    mainLayout->setRowStretch(row, 1);
}

void LibraryMetadataPageWidget::load()
{
    m_useVariousCompilations->setChecked(m_settings->value<Settings::Core::UseVariousForCompilations>());
    m_saveRatings->setChecked(m_settings->value<Settings::Core::SaveRatingToMetadata>());
    m_savePlaycounts->setChecked(m_settings->value<Settings::Core::SavePlaycountToMetadata>());
    m_overwriteRatingsOnReload->setChecked(m_settings->value<Settings::Core::OverwriteRatingOnReload>());
    m_overwritePlaycountsOnReload->setChecked(m_settings->value<Settings::Core::OverwritePlaycountOnReload>());
}

void LibraryMetadataPageWidget::apply()
{
    m_settings->set<Settings::Core::UseVariousForCompilations>(m_useVariousCompilations->isChecked());
    m_settings->set<Settings::Core::SaveRatingToMetadata>(m_saveRatings->isChecked());
    m_settings->set<Settings::Core::SavePlaycountToMetadata>(m_savePlaycounts->isChecked());
    m_settings->set<Settings::Core::OverwriteRatingOnReload>(m_overwriteRatingsOnReload->isChecked());
    m_settings->set<Settings::Core::OverwritePlaycountOnReload>(m_overwritePlaycountsOnReload->isChecked());
}

void LibraryMetadataPageWidget::reset()
{
    m_settings->reset<Settings::Core::UseVariousForCompilations>();
    m_settings->reset<Settings::Core::SaveRatingToMetadata>();
    m_settings->reset<Settings::Core::SavePlaycountToMetadata>();
    m_settings->reset<Settings::Core::OverwriteRatingOnReload>();
    m_settings->reset<Settings::Core::OverwritePlaycountOnReload>();
}

LibraryMetadataPage::LibraryMetadataPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibraryMetadata);
    setName(tr("General"));
    setCategory({tr("Library"), tr("Metadata")});
    setWidgetCreator([settings] { return new LibraryMetadataPageWidget(settings); });
}
} // namespace Fooyin

#include "librarymetadatapage.moc"
#include "moc_librarymetadatapage.cpp"
