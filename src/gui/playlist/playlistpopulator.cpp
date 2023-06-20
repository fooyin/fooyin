/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistpopulator.h"

#include "playlistpreset.h"
#include "playlistscriptregistry.h"

#include <QCryptographicHash>
#include <QTimer>

#include <ranges>

namespace Fy::Gui::Widgets::Playlist {
constexpr int InitialBatchSize = 1000;

QString generateHeaderKey(const QString& titleText, const QString& subtitleText, const QString& sideText)
{
    QCryptographicHash hash{QCryptographicHash::Md5};
    hash.addData(titleText.toUtf8());
    hash.addData(subtitleText.toUtf8());
    hash.addData(sideText.toUtf8());

    QString headerKey = hash.result().toHex();
    return headerKey;
}

void parseBlock(Core::Scripting::Parser& parser, TextBlock& block)
{
    block.script = parser.parse(block.text);
}

void parseBlockList(Core::Scripting::Parser& parser, TextBlockList& blocks)
{
    for(TextBlock& block : blocks) {
        parseBlock(parser, block);
    }
}

struct PlaylistPopulator::Private : QObject
{
    PlaylistPopulator* populator;
    PlaylistPreset currentPreset;

    std::unique_ptr<PlaylistScriptRegistry> registry;
    Core::Scripting::Parser parser;

    PlaylistItem root;
    PendingData data;
    Core::TrackList pendingTracks;

    explicit Private(PlaylistPopulator* populator)
        : populator{populator}
        , registry{std::make_unique<PlaylistScriptRegistry>()}
        , parser{registry.get()}
        , data{}
    { }

    void updateScripts()
    {
        parseBlock(parser, currentPreset.header.title);
        parseBlock(parser, currentPreset.header.subtitle);
        parseBlock(parser, currentPreset.header.sideText);
        parseBlock(parser, currentPreset.header.info);

        for(auto& row : currentPreset.subHeaders.rows) {
            parseBlock(parser, row.title);
            parseBlock(parser, row.info);
        }

        parseBlockList(parser, currentPreset.track.text);
    }

    PlaylistItem* getOrInsertItem(const QString& key, PlaylistItem::ItemType type, const Data& item,
                                  PlaylistItem* parent)
    {
        auto [node, inserted] = data.items.try_emplace(key, PlaylistItem{type, item, parent});
        if(inserted) {
            node->second.setKey(key);
        }
        PlaylistItem* child = &node->second;

        if(child->pending()) {
            child->setPending(false);
            data.nodes[parent->key()].push_back(child->key());
        }
        return child;
    }

    void updateContainers()
    {
        for(const auto& [key, container] : data.headers) {
            if(container->trackCount() > 0) {
                registry->changeCurrentContainer(container);
                TextBlock headerInfo{container->info()};
                headerInfo.text = parser.evaluate(headerInfo.script, container->tracks().front());
                container->modifyInfo(headerInfo);
            }
        }
    }

    void iterateHeader(const Core::Track& track, PlaylistItem*& parent)
    {
        HeaderRow row{currentPreset.header};
        if(!row.isValid()) {
            return;
        }
        row.title.text    = parser.evaluate(currentPreset.header.title.script, track);
        row.subtitle.text = parser.evaluate(currentPreset.header.subtitle.script, track);
        row.sideText.text = parser.evaluate(currentPreset.header.sideText.script, track);

        const QString key = generateHeaderKey(row.title.text, row.subtitle.text, row.sideText.text);

        if(!data.headers.contains(key)) {
            Container header;
            header.setTitle(row.title);
            header.setSubtitle(row.subtitle);
            header.setSideText(row.sideText);
            header.setCoverPath(track.thumbnailPath());
            header.modifyInfo(row.info);

            auto* headerItem      = getOrInsertItem(key, PlaylistItem::Header, header, parent);
            auto& headerContainer = std::get<1>(headerItem->data());
            data.headers.emplace(key, &headerContainer);
        }
        Container* header = data.headers.at(key);
        header->addTrack(track);

        auto* headerItem = &data.items.at(key);
        parent           = headerItem;
    }

    void iterateSubheaders(const Core::Track& track, PlaylistItem*& parent)
    {
        const auto rows = currentPreset.subHeaders.rows;
        for(const SubheaderRow& row : rows) {
            TextBlock rowLeft{row.title};
            if(!rowLeft.isValid()) {
                return;
            }

            const QString result = parser.evaluate(rowLeft.script, track);
            if(result.isEmpty()) {
                continue;
            }

            rowLeft.text = result;

            const QString key = generateHeaderKey(parent->key(), rowLeft.text, "");

            if(!data.headers.contains(key)) {
                Container subheader;
                subheader.setTitle(rowLeft);
                subheader.modifyInfo(row.info);

                auto* subheaderItem      = getOrInsertItem(key, PlaylistItem::Subheader, subheader, parent);
                auto& subheaderContainer = std::get<1>(subheaderItem->data());
                data.headers.emplace(key, &subheaderContainer);
            }
            Container* subheader = data.headers.at(key);
            subheader->addTrack(track);

            auto* subheaderItem = &data.items.at(key);
            if(subheaderItem->parent()->type() != PlaylistItem::Header) {
                subheaderItem->setIndentation(subheaderItem->parent()->indentation() + 20);
            }
            parent = subheaderItem;
        }
    }

    void iterateTrack(const Core::Track& track)
    {
        PlaylistItem* parent = &root;

        iterateHeader(track, parent);
        iterateSubheaders(track, parent);

        TrackRow trackLeft{currentPreset.track};
        TrackRow trackRight{currentPreset.track};

        if(!trackLeft.isValid() && !trackRight.isValid()) {
            return;
        }

        trackLeft.text.clear();
        trackRight.text.clear();

        bool leftFilled{false};
        for(const TextBlock& block : currentPreset.track.text) {
            TextBlock trackBlock;
            trackBlock.cloneProperties(block);

            const QString result = parser.evaluate(block.script, track);
            if(result.isEmpty()) {
                continue;
            }
            if(result.contains("||") && !leftFilled) {
                leftFilled          = true;
                const QString left  = result.section("||", 0, 0);
                const QString right = result.section("||", 1);
                if(!left.isEmpty()) {
                    trackBlock.text = left;
                    trackLeft.text.push_back(trackBlock);
                }
                if(!right.isEmpty()) {
                    trackBlock.text = right;
                    trackRight.text.push_back(trackBlock);
                }
            }
            else {
                trackBlock.text = result;
                leftFilled ? trackRight.text.push_back(trackBlock) : trackLeft.text.push_back(trackBlock);
            }
        }
        Track playlistTrack{trackLeft.text, trackRight.text, track};

        const QString key = generateHeaderKey(parent->key(), track.hash(), "");

        auto* trackItem = getOrInsertItem(key, PlaylistItem::Track, playlistTrack, parent);
        if(parent->type() != PlaylistItem::Header) {
            trackItem->setIndentation(parent->indentation() + 20);
        }
    }

    void runBatch(int size)
    {
        auto tracksBatch = std::ranges::views::take(pendingTracks, size);

        for(const Core::Track& track : tracksBatch) {
            if(!populator->mayRun()) {
                return;
            }
            if(!track.enabled()) {
                continue;
            }
            iterateTrack(track);
        }

        updateContainers();

        if(!populator->mayRun()) {
            return;
        }

        emit populator->populated(data);

        auto tracksToKeep = std::ranges::views::drop(pendingTracks, size);
        Core::TrackList tempTracks;
        std::ranges::copy(tracksToKeep, std::back_inserter(tempTracks));
        pendingTracks = std::move(tempTracks);

        data.nodes.clear();

        const auto remaining = static_cast<int>(pendingTracks.size());
        if(remaining > 0) {
            runBatch(remaining);
        }
    }
};

PlaylistPopulator::PlaylistPopulator(QObject* parent)
    : Utils::Worker{parent}
    , p{std::make_unique<Private>(this)}
{
    qRegisterMetaType<PendingData>();
}

void PlaylistPopulator::run(const PlaylistPreset& preset, const Core::TrackList& tracks)
{
    setState(Running);

    p->data.clear();
    p->currentPreset = preset;
    p->updateScripts();
    p->pendingTracks = tracks;

    p->runBatch(InitialBatchSize);

    setState(Idle);
}

PlaylistPopulator::~PlaylistPopulator() = default;
} // namespace Fy::Gui::Widgets::Playlist
