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

#include "artworkproperties.h"

#include "artworkrow.h"

#include <core/engine/audioloader.h>
#include <gui/coverprovider.h>

#include <QFutureWatcher>
#include <QGridLayout>
#include <QPainter>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

namespace {
struct ArtworkLoadEntry
{
    Fooyin::Track::Cover type;
    QByteArray imageData;
    int imageCount{0};
    bool multipleImages{false};
    bool sawMissing{false};
};

} // namespace

namespace Fooyin {
struct ArtworkLoadResult
{
    std::array<ArtworkLoadEntry, 3> entries;
    bool cancelled{false};
};

ArtworkProperties::ArtworkProperties(AudioLoader* loader, MusicLibrary* library, TrackList tracks, bool readOnly,
                                     QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_audioLoader{loader}
    , m_library{library}
    , m_tracks{std::move(tracks)}
    , m_watcher{new QFutureWatcher<std::shared_ptr<ArtworkLoadResult>>(this)}
    , m_cancelLoading{std::make_shared<std::atomic_bool>(false)}
    , m_loaded{false}
    , m_loading{false}
    , m_writing{false}
    , m_artworkWidget{new QWidget(this)}
    , m_rows{new ArtworkRow(" "_L1 + tr("Front Cover"), Track::Cover::Front, readOnly, this),
             new ArtworkRow(" "_L1 + tr("Back Cover"), Track::Cover::Back, readOnly, this),
             new ArtworkRow(" "_L1 + tr("Artist"), Track::Cover::Artist, readOnly, this)}
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins({});

    auto* artworkLayout = new QGridLayout(m_artworkWidget);
    artworkLayout->setContentsMargins({});

    layout->addWidget(m_artworkWidget);
    layout->setRowStretch(layout->rowCount(), 1);

    int row{0};
    for(ArtworkRow* artworkRow : m_rows) {
        artworkLayout->addWidget(artworkRow, row++, 0);
    }
    artworkLayout->setRowStretch(artworkLayout->rowCount(), 1);

    m_artworkWidget->hide();
}

ArtworkProperties::~ArtworkProperties()
{
    m_cancelLoading->store(true);
}

void ArtworkProperties::loadTrackArtwork()
{
    if(m_loaded) {
        return;
    }

    m_loaded  = true;
    m_loading = true;
    m_cancelLoading->store(false);

    constexpr std::array coverTypes = {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist};

    const auto processTracks = [tracks = m_tracks, loader = m_audioLoader, cancel = m_cancelLoading, coverTypes]() {
        auto result = std::make_shared<ArtworkLoadResult>();

        for(size_t i{0}; i < coverTypes.size(); ++i) {
            result->entries[i].type = coverTypes[i];
        }

        for(const Track& track : tracks) {
            if(cancel->load()) {
                result->cancelled = true;
                return result;
            }

            for(size_t i{0}; i < coverTypes.size(); ++i) {
                auto& entry            = result->entries[i];
                const QByteArray cover = loader->readTrackCover(track, entry.type);

                if(cancel->load()) {
                    result->cancelled = true;
                    return result;
                }

                if(cover.isEmpty()) {
                    entry.sawMissing = true;
                    if(!entry.imageData.isEmpty()) {
                        entry.multipleImages = true;
                    }
                    continue;
                }

                ++entry.imageCount;

                if(entry.multipleImages) {
                    continue;
                }

                if(entry.sawMissing) {
                    entry.multipleImages = true;
                    entry.imageData.clear();
                    continue;
                }

                if(entry.imageData.isEmpty()) {
                    entry.imageData = cover;
                    continue;
                }

                if(entry.imageData != cover) {
                    entry.multipleImages = true;
                    entry.imageData.clear();
                }
            }
        }

        return result;
    };

    const int trackCount = static_cast<int>(m_tracks.size());

    const auto finishLoading = [this, trackCount]() {
        const auto result = m_watcher->result();
        if(!result || result->cancelled || m_cancelLoading->load()) {
            return;
        }

        for(size_t i{0}; i < m_rows.size(); ++i) {
            const auto& entry = result->entries[i];
            m_rows[i]->setLoadedState(entry.imageData, entry.imageCount, trackCount, entry.multipleImages);
        }

        m_loading = false;
        m_artworkWidget->show();
        update();
    };

    QObject::disconnect(m_watcher, nullptr, this, nullptr);
    const auto future = QtConcurrent::run(processTracks);
    QObject::connect(m_watcher, &QFutureWatcher<void>::finished, this, finishLoading);
    m_watcher->setFuture(future);
}

void ArtworkProperties::load()
{
    loadTrackArtwork();
}

QString ArtworkProperties::name() const
{
    return tr("Artwork Properties");
}

QString ArtworkProperties::layoutName() const
{
    return u"ArtworkProperties"_s;
}

void ArtworkProperties::apply()
{
    TrackCoverData coverData;
    coverData.tracks = m_tracks;

    for(ArtworkRow* row : m_rows) {
        if(row->status() != ArtworkRow::Status::None) {
            coverData.coverData.emplace(row->type(), CoverImage{row->mimeType(), row->image()});
            row->reset();
        }
    }

    if(coverData.coverData.empty()) {
        return;
    }

    QObject::connect(
        m_library, &MusicLibrary::tracksMetadataChanged, this,
        [this]() {
            m_writeRequest = {};
            m_writing      = false;
            m_artworkWidget->show();
            update();
        },
        Qt::SingleShotConnection);

    m_writing = true;
    m_artworkWidget->hide();
    update();

    m_writeRequest = m_library->writeTrackCovers(coverData);
    std::ranges::for_each(m_tracks, CoverProvider::removeFromCache);
}

void ArtworkProperties::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};

    auto drawCentreText = [this, &painter](const QString& text) {
        QRect textRect = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    };

    if(m_loading) {
        drawCentreText(tr("Loading artwork…"));
        return;
    }
    if(m_writing) {
        drawCentreText(tr("Saving artwork to files…"));
        return;
    }

    PropertiesTabWidget::paintEvent(event);
}
} // namespace Fooyin

#include "moc_artworkproperties.cpp"
