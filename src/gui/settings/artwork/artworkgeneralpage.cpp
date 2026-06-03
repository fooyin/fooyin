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

#include "artworkgeneralpage.h"

#include "internalguisettings.h"

#include <gui/coverrepository.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ArtworkPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkPageWidget(SettingsManager* settings, CoverRepository* coverRepository);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateCacheSize();

    SettingsManager* m_settings;
    CoverRepository* m_coverRepository;

    QRadioButton* m_preferPlaying;
    QRadioButton* m_preferSelection;
    QRadioButton* m_preferDirectory;
    QRadioButton* m_preferEmbedded;

    ScriptLineEdit* m_thumbnailGroupScript;
    QSpinBox* m_pixmapCache;
    QLabel* m_cacheSizeLabel;
};

ArtworkPageWidget::ArtworkPageWidget(SettingsManager* settings, CoverRepository* coverRepository)
    : m_settings{settings}
    , m_coverRepository{coverRepository}
    , m_preferPlaying{new QRadioButton(tr("Prefer currently playing track"), this)}
    , m_preferSelection{new QRadioButton(tr("Prefer current selection"), this)}
    , m_preferDirectory{new QRadioButton(tr("Prefer directory artwork"), this)}
    , m_preferEmbedded{new QRadioButton(tr("Prefer embedded artwork"), this)}
    , m_thumbnailGroupScript{new ScriptLineEdit(this)}
    , m_pixmapCache{new QSpinBox(this)}
    , m_cacheSizeLabel{new QLabel(this)}
{
    auto* displayGroupBox = new QGroupBox(tr("Display"), this);
    auto* displayGroup    = new QButtonGroup(this);
    auto* displayLayout   = new QVBoxLayout(displayGroupBox);

    displayGroup->addButton(m_preferPlaying);
    displayGroup->addButton(m_preferSelection);

    displayLayout->addWidget(m_preferPlaying);
    displayLayout->addWidget(m_preferSelection);

    auto* sourceGroupBox = new QGroupBox(tr("Local Source"), this);
    auto* sourceGroup    = new QButtonGroup(this);
    auto* sourceLayout   = new QVBoxLayout(sourceGroupBox);

    sourceGroup->addButton(m_preferDirectory);
    sourceGroup->addButton(m_preferEmbedded);

    sourceLayout->addWidget(m_preferDirectory);
    sourceLayout->addWidget(m_preferEmbedded);

    auto* thumbnailsGroupBox = new QGroupBox(tr("Thumbnails"), this);
    auto* thumbnailsLayout   = new QGridLayout(thumbnailsGroupBox);

    auto* thumbnailGroupLabel        = new QLabel(tr("Grouping script") + u":"_s, this);
    const auto thumbnailGroupTooltip = tr("Groups artwork thumbnails that should share the same cached image");
    thumbnailGroupLabel->setToolTip(thumbnailGroupTooltip);
    m_thumbnailGroupScript->setToolTip(thumbnailGroupTooltip);

    thumbnailsLayout->addWidget(thumbnailGroupLabel, 0, 0);
    thumbnailsLayout->addWidget(m_thumbnailGroupScript, 0, 1);
    thumbnailsLayout->setColumnStretch(1, 1);

    auto* cacheGroupBox = new QGroupBox(tr("Cache"), this);
    auto* cacheLayout   = new QGridLayout(cacheGroupBox);

    auto* pixmapCacheLabel = new QLabel(tr("Pixmap cache size") + u":"_s, this);

    m_pixmapCache->setMinimum(10);
    m_pixmapCache->setMaximum(1000);
    m_pixmapCache->setSuffix(u" MB"_s);

    auto* clearCacheButton = new QPushButton(tr("Clear Cache"), this);
    QObject::connect(clearCacheButton, &QPushButton::clicked, this, [this]() {
        m_coverRepository->clearCache();
        updateCacheSize();
    });

    int row{0};
    cacheLayout->addWidget(pixmapCacheLabel, row, 0);
    cacheLayout->addWidget(m_pixmapCache, row++, 1);
    cacheLayout->addWidget(m_cacheSizeLabel, row, 0);
    cacheLayout->addWidget(clearCacheButton, row++, 1);
    cacheLayout->setColumnStretch(cacheLayout->columnCount(), 1);

    auto* layout = new QGridLayout(this);
    layout->addWidget(displayGroupBox, 0, 0);
    layout->addWidget(sourceGroupBox, 1, 0);
    layout->addWidget(thumbnailsGroupBox, 2, 0);
    layout->addWidget(cacheGroupBox, 3, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void ArtworkPageWidget::load()
{
    const auto option
        = static_cast<SelectionDisplay>(m_settings->value<Settings::Gui::Internal::TrackCoverDisplayOption>());

    if(option == SelectionDisplay::PreferPlaying) {
        m_preferPlaying->setChecked(true);
    }
    else {
        m_preferSelection->setChecked(true);
    }

    const auto sourcePref = static_cast<ArtworkSourcePreference>(
        m_settings->value<Settings::Gui::Internal::TrackCoverSourcePreference>());
    if(sourcePref == ArtworkSourcePreference::PreferEmbedded) {
        m_preferEmbedded->setChecked(true);
    }
    else {
        m_preferDirectory->setChecked(true);
    }

    m_pixmapCache->setValue(m_settings->value<Settings::Gui::Internal::PixmapCacheSize>());
    m_thumbnailGroupScript->setText(m_settings->value<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>());
    updateCacheSize();
}

void ArtworkPageWidget::apply()
{
    const SelectionDisplay option
        = m_preferPlaying->isChecked() ? SelectionDisplay::PreferPlaying : SelectionDisplay::PreferSelection;
    const ArtworkSourcePreference sourcePref = m_preferEmbedded->isChecked() ? ArtworkSourcePreference::PreferEmbedded
                                                                             : ArtworkSourcePreference::PreferDirectory;

    m_settings->set<Settings::Gui::Internal::TrackCoverDisplayOption>(static_cast<int>(option));
    m_settings->set<Settings::Gui::Internal::TrackCoverSourcePreference>(static_cast<int>(sourcePref));
    m_settings->set<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>(m_thumbnailGroupScript->text());
    m_settings->set<Settings::Gui::Internal::PixmapCacheSize>(m_pixmapCache->value());
}

void ArtworkPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::TrackCoverDisplayOption>();
    m_settings->reset<Settings::Gui::Internal::TrackCoverSourcePreference>();
    m_settings->reset<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>();
    m_settings->reset<Settings::Gui::Internal::PixmapCacheSize>();
}

void ArtworkPageWidget::updateCacheSize()
{
    const QString cacheSize = Utils::formatFileSize(Utils::File::directorySize(Gui::coverPath()));
    m_cacheSizeLabel->setText(tr("Disk cache usage") + u": %1"_s.arg(cacheSize));
}

ArtworkGeneralPage::ArtworkGeneralPage(SettingsManager* settings, CoverRepository* coverRepository, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ArtworkGeneral);
    setName(tr("General"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([settings, coverRepository] { return new ArtworkPageWidget(settings, coverRepository); });
}
} // namespace Fooyin

#include "artworkgeneralpage.moc"
#include "moc_artworkgeneralpage.cpp"
