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

#include "artworkpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <QButtonGroup>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ArtworkPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateCacheSize();

    SettingsManager* m_settings;

    QRadioButton* m_preferPlaying;
    QRadioButton* m_preferSelection;

    QTabWidget* m_coverPaths;
    QPlainTextEdit* m_frontCovers;
    QPlainTextEdit* m_backCovers;
    QPlainTextEdit* m_artistCovers;

    QSpinBox* m_pixmapCache;
    QLabel* m_cacheSizeLabel;
};

ArtworkPageWidget::ArtworkPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_preferPlaying{new QRadioButton(tr("Prefer currently playing track"), this)}
    , m_preferSelection{new QRadioButton(tr("Prefer current selection"), this)}
    , m_coverPaths{new QTabWidget(this)}
    , m_frontCovers{new QPlainTextEdit(this)}
    , m_backCovers{new QPlainTextEdit(this)}
    , m_artistCovers{new QPlainTextEdit(this)}
    , m_pixmapCache{new QSpinBox(this)}
    , m_cacheSizeLabel{new QLabel(this)}
{
    auto* layout = new QGridLayout(this);

    auto* displayGroupBox = new QGroupBox(tr("Display"), this);
    auto* displayGroup    = new QButtonGroup(this);
    auto* displayLayout   = new QVBoxLayout(displayGroupBox);

    displayGroup->addButton(m_preferPlaying);
    displayGroup->addButton(m_preferSelection);

    displayLayout->addWidget(m_preferPlaying);
    displayLayout->addWidget(m_preferSelection);

    m_coverPaths->addTab(m_frontCovers, tr("Front Cover"));
    m_coverPaths->addTab(m_backCovers, tr("Back Cover"));
    m_coverPaths->addTab(m_artistCovers, tr("Artist"));

    auto* cacheGroupBox = new QGroupBox(tr("Cache"), this);
    auto* cacheLayout   = new QGridLayout(cacheGroupBox);

    auto* pixmapCacheLabel = new QLabel(tr("Pixmap cache size") + u":"_s, this);

    m_pixmapCache->setMinimum(10);
    m_pixmapCache->setMaximum(1000);
    m_pixmapCache->setSuffix(u" MB"_s);

    auto* clearCacheButton = new QPushButton(tr("Clear Cache"), this);
    QObject::connect(clearCacheButton, &QPushButton::clicked, this, [this]() {
        QDir{Gui::coverPath()}.removeRecursively();
        updateCacheSize();
    });

    int row{0};
    cacheLayout->addWidget(pixmapCacheLabel, row, 0);
    cacheLayout->addWidget(m_pixmapCache, row++, 1);
    cacheLayout->addWidget(m_cacheSizeLabel, row, 0);
    cacheLayout->addWidget(clearCacheButton, row++, 1);
    cacheLayout->setColumnStretch(cacheLayout->columnCount(), 1);

    layout->addWidget(displayGroupBox, 0, 0);
    layout->addWidget(m_coverPaths, 1, 0);
    layout->addWidget(cacheGroupBox, 2, 0);
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

    const auto paths = m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>();

    m_frontCovers->setPlainText(paths.frontCoverPaths.join("\n"_L1));
    m_backCovers->setPlainText(paths.backCoverPaths.join("\n"_L1));
    m_artistCovers->setPlainText(paths.artistPaths.join("\n"_L1));

    m_pixmapCache->setValue(m_settings->value<Settings::Gui::Internal::PixmapCacheSize>());
    updateCacheSize();
}

void ArtworkPageWidget::apply()
{
    const SelectionDisplay option
        = m_preferPlaying->isChecked() ? SelectionDisplay::PreferPlaying : SelectionDisplay::PreferSelection;

    m_settings->set<Settings::Gui::Internal::TrackCoverDisplayOption>(static_cast<int>(option));

    CoverPaths paths;

    paths.frontCoverPaths = m_frontCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.backCoverPaths  = m_backCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.artistPaths     = m_artistCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);

    m_settings->set<Settings::Gui::Internal::TrackCoverPaths>(QVariant::fromValue(paths));
    m_settings->set<Settings::Gui::Internal::PixmapCacheSize>(m_pixmapCache->value());
}

void ArtworkPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::TrackCoverDisplayOption>();
    m_settings->reset<Settings::Gui::Internal::TrackCoverPaths>();
    m_settings->reset<Settings::Gui::Internal::PixmapCacheSize>();
}

void ArtworkPageWidget::updateCacheSize()
{
    const QString cacheSize = Utils::formatFileSize(Utils::File::directorySize(Gui::coverPath()));
    m_cacheSizeLabel->setText(tr("Disk cache usage") + u": %1"_s.arg(cacheSize));
}

ArtworkPage::ArtworkPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Artwork);
    setName(tr("Artwork"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([settings] { return new ArtworkPageWidget(settings); });
}
} // namespace Fooyin

#include "artworkpage.moc"
#include "moc_artworkpage.cpp"
