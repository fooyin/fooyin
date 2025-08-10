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

#include "artworkfinder.h"

#include "internalguisettings.h"
#include "sources/discogsartwork.h"
#include "sources/lastfmartwork.h"
#include "sources/musicbrainzartwork.h"

#include <core/network/networkaccessmanager.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QDir>
#include <QMimeDatabase>
#include <QNetworkReply>

constexpr auto SourceState = "Artwork/SourceState";

namespace {
void sortSources(std::vector<Fooyin::ArtworkSource*>& sources)
{
    std::ranges::sort(sources, {}, &Fooyin::ArtworkSource::index);
    std::ranges::for_each(sources, [i = 0](auto& source) mutable { source->setIndex(i++); });
}
} // namespace

namespace Fooyin {
ArtworkFinder::ArtworkFinder(std::shared_ptr<NetworkAccessManager> networkManager, SettingsManager* settings,
                             QObject* parent)
    : QObject{parent}
    , m_networkManager{std::move(networkManager)}
    , m_settings{settings}
{
    loadDefaults();
    restoreState();
}

void ArtworkFinder::findArtwork(Track::Cover type, const QString& artist, const QString& album, const QString& title)
{
    for(const auto& source : m_sources) {
        QObject::disconnect(source, nullptr, this, nullptr);
    }

    m_params = {.type = type, .artist = artist, .album = album, .title = title};

    m_currentSourceIndex = -1;
    m_currentSource      = nullptr;
    finishOrStartNextSource();
}

std::vector<ArtworkSource*> ArtworkFinder::sources() const
{
    return m_sources;
}

void ArtworkFinder::saveState()
{
    FySettings settings;

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};

    stream << static_cast<qsizetype>(m_sources.size());
    for(const auto& source : m_sources) {
        stream << source->name() << source->index() << source->enabled();
    }

    settings.setValue(QLatin1String{SourceState}, data);
}

void ArtworkFinder::restoreState()
{
    const FySettings settings;

    QByteArray data = settings.value(QLatin1String{SourceState}, {}).toByteArray();
    QDataStream stream{&data, QIODevice::ReadOnly};

    qsizetype size{0};
    stream >> size;

    while(size > 0) {
        --size;
        QString name;
        int index{0};
        bool enabled{true};

        stream >> name >> index >> enabled;

        auto sourceIt = std::ranges::find_if(m_sources, [&name](const auto& source) { return source->name() == name; });
        if(sourceIt != m_sources.cend()) {
            (*sourceIt)->setIndex(index);
            (*sourceIt)->setEnabled(enabled);
        }
    }

    sortSources(m_sources);
}

void ArtworkFinder::sort()
{
    sortSources(m_sources);
}

void ArtworkFinder::reset()
{
    FySettings settings;
    settings.remove(QLatin1String{SourceState});

    loadDefaults();
}

void ArtworkFinder::loadDefaults()
{
    m_sources = {new MusicBrainzArtwork(m_networkManager.get(), m_settings, 0, true, this),
                 new LastFmArtwork(m_networkManager.get(), m_settings, 1, true, this),
                 new DiscogsArtwork(m_networkManager.get(), m_settings, 2, true, this)};
}

void ArtworkFinder::finishOrStartNextSource(bool forceFinish)
{
    if(m_currentSource) {
        QObject::disconnect(m_currentSource, nullptr, this, nullptr);
    }

    if(!forceFinish && findNextAvailableSource()) {
        QObject::connect(m_currentSource, &ArtworkSource::searchResult, this, &ArtworkFinder::onSearchResults,
                         Qt::SingleShotConnection);
        m_currentSource->search(m_params);
    }
    else if(m_downloads.empty()) {
        emit searchFinished();
    }
}

bool ArtworkFinder::findNextAvailableSource()
{
    ++m_currentSourceIndex;

    while(std::cmp_less(m_currentSourceIndex, m_sources.size())) {
        auto* source = m_sources.at(m_currentSourceIndex);
        if(source->enabled() && source->supportedTypes().contains(m_params.type)) {
            m_currentSource = source;
            return true;
        }
        ++m_currentSourceIndex;
    }

    m_currentSource = nullptr;
    return false;
}

void ArtworkFinder::onSearchResults(const SearchResults& results)
{
    if(results.empty()) {
        finishOrStartNextSource();
        return;
    }

    const int matchThreshold = m_settings->value<Settings::Gui::Internal::ArtworkMatchThreshold>();

    const auto isSimilar = [matchThreshold](const QString& param, const QString& resultParam) {
        if(param.isEmpty() || resultParam.isEmpty()) {
            return true;
        }
        return Utils::similarityRatio(param, resultParam, Qt::CaseInsensitive) >= matchThreshold;
    };

    for(const auto& result : results) {
        if(!std::ranges::any_of(result.artist,
                                [this, isSimilar](const auto& artist) { return isSimilar(m_params.artist, artist); })) {
            continue;
        }
        if(!isSimilar(m_params.album, result.album)) {
            continue;
        }

        emit coverFound(result);

        const QNetworkRequest req{result.imageUrl};
        auto* reply = m_downloads.emplace_back(m_networkManager->get(req));
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                         [this, url = result.imageUrl](qint64 bytesReceived, qint64 bytesTotal) {
                             onDownloadProgress(url, bytesReceived, bytesTotal);
                         });
        QObject::connect(reply, &QNetworkReply::errorOccurred, this,
                         [this, url = result.imageUrl]() { emit coverLoadError(url); });
        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, reply, url = result.imageUrl]() { onImageResult(url, reply); });
    }

    finishOrStartNextSource();
}

void ArtworkFinder::onDownloadProgress(const QUrl& url, qint64 bytesReceived, qint64 bytesTotal)
{
    int progress = static_cast<int>((static_cast<double>(bytesReceived) / static_cast<double>(bytesTotal)) * 100);
    progress     = std::clamp(progress, 0, 100);
    emit coverLoadProgress(url, progress);
}

void ArtworkFinder::onImageResult(const QUrl& url, QNetworkReply* reply)
{
    if(std::erase(m_downloads, reply) && reply->error() == QNetworkReply::NoError) {
        const QByteArray coverData = reply->readAll();
        reply->deleteLater();

        const QMimeDatabase mimeDb;
        const QString mimeType = mimeDb.mimeTypeForData(coverData).name();

        emit coverLoaded(url, {.mimeType = mimeType, .image = coverData});
    }

    if(m_downloads.empty() && std::cmp_greater_equal(m_currentSourceIndex, m_sources.size())) {
        emit searchFinished();
    }
}
} // namespace Fooyin
