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

#include "artworksourcespage.h"

#include "artwork/artworkfinder.h"
#include "artworksourcesmodel.h"
#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QTabWidget>

#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
Track::Cover coverTypeForIndex(int index)
{
    if(index == 1) {
        return Track::Cover::Back;
    }
    if(index == 2) {
        return Track::Cover::Artist;
    }
    return Track::Cover::Front;
}
} // namespace

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
    void browsePlaceholderImage() const;
    void storeCurrentPlaceholder();
    void updatePlaceholderInput(int index);

    ArtworkFinder* m_finder;
    SettingsManager* m_settings;

    QTabWidget* m_coverPaths;
    QPlainTextEdit* m_frontCovers;
    QPlainTextEdit* m_backCovers;
    QPlainTextEdit* m_artistCovers;
    QLineEdit* m_placeholder;
    std::map<Track::Cover, QString> m_placeholders;
    Track::Cover m_placeholderType{Track::Cover::Front};

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
    , m_placeholder{new QLineEdit(this)}
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

    auto* placeholderGroup  = new QGroupBox(tr("Placeholder"), this);
    auto* placeholderLayout = new QGridLayout(placeholderGroup);

    const auto placeholderTooltip
        = tr("Path to an image file to use when artwork of this type is missing; leave empty to use the default icon");

    auto* browseAction = new QAction(m_placeholder);
    Gui::setThemeIcon(browseAction, Constants::Icons::Options);
    browseAction->setToolTip(tr("Choose a custom placeholder image file"));
    QObject::connect(browseAction, &QAction::triggered, this, &ArtworkSourcesPageWidget::browsePlaceholderImage);

    m_placeholder->setClearButtonEnabled(true);
    m_placeholder->addAction(browseAction, QLineEdit::TrailingPosition);
    m_placeholder->setPlaceholderText(tr("Use built-in icon theme placeholder"));
    m_placeholder->setToolTip(placeholderTooltip);

    int row{0};
    placeholderLayout->addWidget(new QLabel(tr("Image file") + ":"_L1, this), row, 0);
    placeholderLayout->addWidget(m_placeholder, row++, 1);
    placeholderLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    row = 0;

    layout->addWidget(new QLabel(tr("Local paths") + ":"_L1, this), row++, 0);
    layout->addWidget(m_coverPaths, row++, 0);
    layout->addWidget(placeholderGroup, row++, 0);
    layout->addWidget(new QLabel(tr("Download sources") + ":"_L1, this), row++, 0);
    layout->addWidget(m_sourceList, row++, 0);
    layout->addWidget(new QLabel(u"🛈 "_s + tr("Artwork will be searched for in the above order."), this), row++, 0);

    QObject::connect(m_coverPaths, &QTabWidget::currentChanged, this,
                     &ArtworkSourcesPageWidget::updatePlaceholderInput);
}

void ArtworkSourcesPageWidget::load()
{
    const auto paths = m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>();

    m_frontCovers->setPlainText(paths.frontCoverPaths.join("\n"_L1));
    m_backCovers->setPlainText(paths.backCoverPaths.join("\n"_L1));
    m_artistCovers->setPlainText(paths.artistPaths.join("\n"_L1));

    m_placeholders[Track::Cover::Front]  = paths.frontPlaceholder;
    m_placeholders[Track::Cover::Back]   = paths.backPlaceholder;
    m_placeholders[Track::Cover::Artist] = paths.artistPlaceholder;
    m_placeholderType                    = coverTypeForIndex(m_coverPaths->currentIndex());
    m_placeholder->setText(m_placeholders[m_placeholderType]);

    m_finder->restoreState();
    m_sourceModel->setup(m_finder->sources());
}

void ArtworkSourcesPageWidget::apply()
{
    CoverPaths paths;

    paths.frontCoverPaths = m_frontCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.backCoverPaths  = m_backCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);
    paths.artistPaths     = m_artistCovers->toPlainText().split("\n"_L1, Qt::SkipEmptyParts);

    storeCurrentPlaceholder();

    paths.frontPlaceholder  = m_placeholders[Track::Cover::Front];
    paths.backPlaceholder   = m_placeholders[Track::Cover::Back];
    paths.artistPlaceholder = m_placeholders[Track::Cover::Artist];

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

    const auto paths = m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>();

    m_placeholders[Track::Cover::Front]  = paths.frontPlaceholder;
    m_placeholders[Track::Cover::Back]   = paths.backPlaceholder;
    m_placeholders[Track::Cover::Artist] = paths.artistPlaceholder;
    m_placeholder->setText(m_placeholders[m_placeholderType]);

    m_finder->reset();
    m_sourceModel->setup(m_finder->sources());
}

void ArtworkSourcesPageWidget::browsePlaceholderImage() const
{
    const QString path = !m_placeholder->text().isEmpty() ? m_placeholder->text() : QDir::homePath();
    const QString file = QFileDialog::getOpenFileName(m_placeholder, tr("Select Placeholder Image"), path,
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"), nullptr,
                                                      QFileDialog::DontResolveSymlinks);
    if(!file.isEmpty()) {
        m_placeholder->setText(file);
    }
}

void ArtworkSourcesPageWidget::storeCurrentPlaceholder()
{
    m_placeholders[m_placeholderType] = m_placeholder->text();
}

void ArtworkSourcesPageWidget::updatePlaceholderInput(int index)
{
    storeCurrentPlaceholder();
    m_placeholderType = coverTypeForIndex(index);
    m_placeholder->setText(m_placeholders[m_placeholderType]);
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
