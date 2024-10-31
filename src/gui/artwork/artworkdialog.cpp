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

#include "artworkdialog.h"

#include "artwork/artworkdelegate.h"
#include "artworkfinder.h"
#include "artworkmodel.h"
#include "internalguisettings.h"

#include <core/library/musiclibrary.h>
#include <gui/coverprovider.h>
#include <gui/guisettings.h>
#include <gui/widgets/expandedtreeview.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeDatabase>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
ArtworkDialog::ArtworkDialog(std::shared_ptr<NetworkAccessManager> networkManager, MusicLibrary* library,
                             SettingsManager* settings, TrackList tracks, Track::Cover type, QWidget* parent)
    : QDialog{parent}
    , m_networkManager{std::move(networkManager)}
    , m_library{library}
    , m_settings{settings}
    , m_tracks{std::move(tracks)}
    , m_type{type}
    , m_artworkFinder{new ArtworkFinder(m_networkManager, m_settings, this)}
    , m_view{new ExpandedTreeView(this)}
    , m_model{new ArtworkModel(this)}
    , m_saveMethods{m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>().value<ArtworkSaveMethods>()}
    , m_status{new QLabel(this)}
    , m_artist{new QLineEdit(this)}
    , m_album{new QLineEdit(this)}
    , m_startSearch{new QPushButton(tr("Search"), this)}
    , m_manualSearch{false}
{
    setWindowTitle(tr("Artwork Finder"));

    m_artist->setPlaceholderText(tr("Artist"));
    m_album->setPlaceholderText(tr("Album"));

    m_view->setModel(m_model);
    m_view->setItemDelegate(new ArtworkDelegate(this));

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setViewMode(ExpandedTreeView::ViewMode::Icon);
    m_view->setHeaderHidden(true);

    QObject::connect(m_artworkFinder, &ArtworkFinder::coverFound, m_model, &ArtworkModel::addPendingCover);
    QObject::connect(m_artworkFinder, &ArtworkFinder::coverLoaded, m_model, &ArtworkModel::loadCover);
    QObject::connect(m_artworkFinder, &ArtworkFinder::coverLoadError, m_model, &ArtworkModel::removeCover);
    QObject::connect(m_artworkFinder, &ArtworkFinder::coverLoadProgress, m_model, &ArtworkModel::updateCoverProgress);
    QObject::connect(m_artworkFinder, &ArtworkFinder::searchFinished, this, &ArtworkDialog::searchFinished);

    QObject::connect(m_startSearch, &QPushButton::clicked, this, [this]() {
        m_manualSearch = true;
        searchArtwork();
    });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* paramsLayout = new QHBoxLayout();

    paramsLayout->addWidget(m_artist, 1);
    paramsLayout->addWidget(m_album, 1);
    paramsLayout->addWidget(m_startSearch);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addLayout(paramsLayout, row++, 0, 1, 2);
    layout->addWidget(m_view, row++, 0, 1, 2);
    layout->addWidget(m_status, row, 0);
    layout->addWidget(buttonBox, row++, 1);

    searchArtwork();
}

void ArtworkDialog::accept()
{
    const auto selection = m_view->selectionModel()->selectedIndexes();
    if(selection.empty()) {
        return;
    }

    auto result = selection.front().data(ArtworkItem::Result).value<ArtworkResult>();
    if(result.image.isNull()) {
        QDialog::accept();
        return;
    }

    QPixmap cover;
    cover.loadFromData(result.image);
    cover.setDevicePixelRatio(Utils::windowDpr());

    const auto& frontMethod = m_saveMethods[Track::Cover::Front];

    if(frontMethod.method == ArtworkSaveMethod::Embedded) {
        TrackCoverData coverData;
        coverData.tracks = m_tracks;
        coverData.coverData.emplace(Track::Cover::Front, CoverImage{result.mimeType, result.image});
        m_library->writeTrackCovers(coverData);
        CoverProvider::setOverride(m_tracks.front(), cover, Track::Cover::Front);
    }
    else {
        const QMimeDatabase mimeDb;
        const QString suffix = mimeDb.mimeTypeForData(result.image).preferredSuffix().toLower();

        const QString path
            = m_parser.evaluate(u"%1/%2.%3"_s.arg(frontMethod.dir, frontMethod.filename, suffix), m_tracks.front());
        const QString cleanPath = QDir::cleanPath(path);

        QFile file{cleanPath};
        if(file.open(QIODevice::WriteOnly)) {
            cover.save(&file, nullptr, -1);
        }
    }

    std::ranges::for_each(m_tracks, CoverProvider::removeFromCache);
    m_settings->set<Settings::Gui::RefreshCovers>(!m_settings->value<Settings::Gui::RefreshCovers>());

    QDialog::accept();
}

QSize ArtworkDialog::sizeHint() const
{
    return {500, 500};
}

void ArtworkDialog::keyPressEvent(QKeyEvent* event)
{
    if(m_artist->hasFocus() || m_album->hasFocus()) {
        m_startSearch->click();
    }
    else {
        QDialog::keyPressEvent(event);
    }
}

void ArtworkDialog::searchArtwork()
{
    const Track& track = m_tracks.front();

    QString artist = m_artist->text();
    QString album  = m_album->text();
    const QString title{u"%title%"_s};

    if(!m_manualSearch) {
        if(artist.isEmpty()) {
            artist = m_parser.evaluate(m_settings->value<Settings::Gui::Internal::ArtworkArtistField>(), track);
            m_artist->setText(artist);
        }
        if(album.isEmpty()) {
            album = m_parser.evaluate(m_settings->value<Settings::Gui::Internal::ArtworkAlbumField>(), track);
            m_album->setText(album);
        }
    }

    m_model->clear();
    // Only support front cover for now
    m_artworkFinder->findArtwork(Track::Cover::Front, artist, album, title);
    m_status->setText(tr("Searching…"));
}

void ArtworkDialog::searchFinished()
{
    if(m_model->rowCount({}) == 0) {
        m_status->setText(tr("No artwork found"));
    }
    else {
        m_status->clear();
    }
}
} // namespace Fooyin
