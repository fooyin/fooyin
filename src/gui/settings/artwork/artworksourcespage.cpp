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

#include "artworksourcespage.h"

#include "artwork/artworkfinder.h"
#include "artworksourcesmodel.h"
#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ArtworkSourcesPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkSourcesPageWidget(ArtworkFinder* finder, SettingsManager* settings);

    void load() override;
    void apply() override;
    void finish() override;
    void reset() override;

private:
    ArtworkFinder* m_finder;
    SettingsManager* m_settings;

    QTabWidget* m_coverPaths;
    QPlainTextEdit* m_frontCovers;
    QPlainTextEdit* m_backCovers;
    QPlainTextEdit* m_artistCovers;

    QListView* m_sourceList;
    ArtworkSourcesModel* m_sourceModel;
};

ArtworkSourcesPageWidget::ArtworkSourcesPageWidget(ArtworkFinder* finder, SettingsManager* settings)
    : m_finder{finder}
    , m_settings{settings}
    , m_coverPaths{new QTabWidget(this)}
    , m_frontCovers{new QPlainTextEdit(this)}
    , m_backCovers{new QPlainTextEdit(this)}
    , m_artistCovers{new QPlainTextEdit(this)}
    , m_sourceList{new QListView(this)}
    , m_sourceModel{new ArtworkSourcesModel(this)}
{
    m_sourceList->setDragDropMode(QAbstractItemView::InternalMove);
    m_sourceList->setDefaultDropAction(Qt::MoveAction);
    m_sourceList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sourceList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sourceList->setDragEnabled(true);
    m_sourceList->setDropIndicatorShown(true);

    m_sourceList->setModel(m_sourceModel);

    m_coverPaths->addTab(m_frontCovers, tr("Front Cover"));
    m_coverPaths->addTab(m_backCovers, tr("Back Cover"));
    m_coverPaths->addTab(m_artistCovers, tr("Artist"));

    auto* layout = new QGridLayout(this);

    int row{0};

    layout->addWidget(new QLabel(tr("Local paths") + ":"_L1, this), row++, 0);
    layout->addWidget(m_coverPaths, row++, 0);
    layout->addWidget(new QLabel(tr("Download sources") + ":"_L1, this), row++, 0);
    layout->addWidget(m_sourceList, row++, 0);
    layout->addWidget(new QLabel(u"🛈 "_s + tr("Artwork will be searched for in the above order."), this), row++, 0);
}

void ArtworkSourcesPageWidget::load()
{
    const auto paths = m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>();

    m_frontCovers->setPlainText(paths.frontCoverPaths.join("\n"_L1));
    m_backCovers->setPlainText(paths.backCoverPaths.join("\n"_L1));
    m_artistCovers->setPlainText(paths.artistPaths.join("\n"_L1));

    m_finder->restoreState();
    m_sourceModel->setup(m_finder->sources());
}

void ArtworkSourcesPageWidget::apply()
{
    CoverPaths paths;

    paths.frontCoverPaths = m_frontCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.backCoverPaths  = m_backCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.artistPaths     = m_artistCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);

    m_settings->set<Settings::Gui::Internal::TrackCoverPaths>(QVariant::fromValue(paths));

    auto existing = m_finder->sources();
    auto sources  = m_sourceModel->sources();

    std::ranges::sort(existing, {}, &ArtworkSource::name);
    std::ranges::sort(sources, {}, &ArtworkSourceItem::name);

    const size_t minSize = std::min(sources.size(), existing.size());
    for(size_t i{0}; i < minSize; ++i) {
        const auto& modelSource    = sources[i];
        const auto& existingSource = existing[i];
        if(modelSource.name == existingSource->name()) {
            existingSource->setIndex(modelSource.index);
            existingSource->setEnabled(modelSource.enabled);
        }
    }

    m_finder->sort();
}

void ArtworkSourcesPageWidget::finish()
{
    m_finder->saveState();
}

void ArtworkSourcesPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::TrackCoverPaths>();
    m_finder->reset();
    m_sourceModel->setup(m_finder->sources());
}

ArtworkSourcesPage::ArtworkSourcesPage(ArtworkFinder* finder, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ArtworkSources);
    setName(tr("Sources"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([finder, settings] { return new ArtworkSourcesPageWidget(finder, settings); });
}
} // namespace Fooyin

#include "artworksourcespage.moc"
#include "moc_artworksourcespage.cpp"
