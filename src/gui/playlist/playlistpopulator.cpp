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

#include <utils/crypto.h>

#include <QCryptographicHash>
#include <QTimer>

#include <ranges>

using namespace Qt::Literals::StringLiterals;

constexpr int InitialBatchSize = 1000;
constexpr int BatchSize        = 4000;

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistPopulator::Private
{
    PlaylistPopulator* self;

    PlaylistPreset currentPreset;

    std::unique_ptr<PlaylistScriptRegistry> registry;
    Core::Scripting::Parser parser;

    QString prevBaseHeaderKey;
    QString prevHeaderKey;
    QString prevBaseSubheaderKey;
    QString prevSubheaderKey;

    std::vector<Container> subheaders;

    PlaylistItem root;
    PendingData data;
    ContainerKeyMap headers;
    Core::TrackList pendingTracks;
    int currentIndex{0};

    explicit Private(PlaylistPopulator* self)
        : self{self}
        , registry{std::make_unique<PlaylistScriptRegistry>()}
        , parser{registry.get()}
        , data{}
    { }

    void updateScripts()
    {
        auto parseBlockList = [this](TextBlockList& blocks) {
            for(TextBlock& block : blocks) {
                block.script = parser.parse(block.text);
            }
        };

        parseBlockList(currentPreset.header.title);
        parseBlockList(currentPreset.header.subtitle);
        parseBlockList(currentPreset.header.sideText);
        parseBlockList(currentPreset.header.info);

        parseBlockList(currentPreset.subHeader.text);

        parseBlockList(currentPreset.track.text);
    }

    PlaylistItem* getOrInsertItem(const QString& key, PlaylistItem::ItemType type, const Data& item,
                                  PlaylistItem* parent, const QString& baseKey = {})
    {
        auto [node, inserted] = data.items.try_emplace(key, PlaylistItem{type, item, parent});
        if(inserted) {
            node->second.setBaseKey(baseKey.isEmpty() ? key : baseKey);
            node->second.setKey(key);
        }
        PlaylistItem* child = &node->second;

        if(!child->pending()) {
            child->setPending(true);
            data.nodes[parent->key()].push_back(key);
            if(type != PlaylistItem::Track) {
                data.containerOrder.push_back(key);
            }
        }
        return child;
    }

    void updateContainers()
    {
        for(const auto& [key, container] : headers) {
            container->updateGroupText(&parser, registry.get());
        }
    }

    void iterateHeader(const Core::Track& track, PlaylistItem*& parent)
    {
        HeaderRow row{currentPreset.header};
        if(!row.isValid()) {
            return;
        }

        auto evaluateBlocks = [this, track](const TextBlockList& presetBlocks, TextBlockList& headerBlocks) -> QString {
            QString key;
            headerBlocks.clear();
            for(const TextBlock& block : presetBlocks) {
                TextBlock headerBlock{block};
                headerBlock.text = parser.evaluate(headerBlock.script, track);
                if(!headerBlock.text.isEmpty()) {
                    headerBlocks.push_back(headerBlock);
                }
                key = key + headerBlock.text;
            }
            return key;
        };

        const QString titleKey    = evaluateBlocks(currentPreset.header.title, row.title);
        const QString subtitleKey = evaluateBlocks(currentPreset.header.subtitle, row.subtitle);
        const QString sideKey     = evaluateBlocks(currentPreset.header.sideText, row.sideText);

        const QString baseKey = Utils::generateHash(titleKey, subtitleKey, sideKey);
        QString key           = Utils::generateRandomHash();
        if(!prevHeaderKey.isEmpty() && prevBaseHeaderKey == baseKey) {
            key = prevHeaderKey;
        }
        prevBaseHeaderKey = baseKey;
        prevHeaderKey     = key;

        if(!headers.contains(key)) {
            Container header;
            header.setTitle(row.title);
            header.setSubtitle(row.subtitle);
            header.setSideText(row.sideText);
            header.setInfo(row.info);

            auto* headerItem      = getOrInsertItem(key, PlaylistItem::Header, header, parent, baseKey);
            auto& headerContainer = std::get<1>(headerItem->data());
            headers.emplace(key, &headerContainer);
        }
        Container* header = headers.at(key);
        header->addTrack(track);
        data.trackParents[track.id()].push_back(key);

        auto* headerItem = &data.items.at(key);
        parent           = headerItem;
    }

    void calculateSubheaders(const TextBlockList& textBlockList)
    {
        if(textBlockList.empty()) {
            return;
        }

        Container currentContainer;
        SubheaderRow leftRow;
        SubheaderRow rightRow;

        bool isRightComponent = false;

        auto addContainer = [this, &currentContainer, &leftRow, &rightRow, &isRightComponent]() {
            currentContainer.setTitle(leftRow.text);
            currentContainer.setInfo(rightRow.text);
            subheaders.push_back(currentContainer);

            leftRow.text.clear();
            rightRow.text.clear();
            isRightComponent = false;
        };

        auto processBlock = [this, &isRightComponent, &rightRow, &leftRow](TextBlock& block, const QString& text,
                                                                           bool setRight = false) {
            block.text       = text;
            block.script     = parser.parse(block.text);
            const bool valid = !block.text.isEmpty();

            if(isRightComponent && valid) {
                rightRow.text.push_back(block);
            }
            else {
                if(valid) {
                    leftRow.text.push_back(block);
                }
                if(setRight) {
                    isRightComponent = true;
                }
            }
        };

        for(const TextBlock& textBlock : textBlockList) {
            TextBlock block;
            block.cloneProperties(textBlock);

            const QStringList levels = textBlock.text.split(u"|||"_s);
            for(const QString& level : levels) {
                if(!level.isEmpty()) {
                    const qsizetype separatorIndex = level.indexOf("||"_L1);
                    if(separatorIndex >= 0) {
                        processBlock(block, level.left(separatorIndex), true);
                        processBlock(block, level.mid(separatorIndex + 2), true);
                    }
                    else {
                        processBlock(block, level);
                    }
                }

                if(&level != &levels.back()) {
                    addContainer();
                }
            }
        }

        if(leftRow.isValid() || rightRow.isValid()) {
            addContainer();
        }
    }

    void iterateSubheaders(const Core::Track& track, PlaylistItem*& parent)
    {
        if(subheaders.empty()) {
            calculateSubheaders(currentPreset.subHeader.text);
        }

        for(const Container& container : subheaders) {
            TextBlockList title = container.title();
            QString subheaderKey;
            for(TextBlock& block : title) {
                block.text   = parser.evaluate(block.script, track);
                subheaderKey = subheaderKey + block.text;
            }

            if(subheaderKey.isEmpty()) {
                return;
            }

            const QString baseKey = Utils::generateHash(parent->baseKey(), subheaderKey);
            QString key           = Utils::generateRandomHash();
            if(!prevSubheaderKey.isEmpty() && prevBaseSubheaderKey == baseKey) {
                key = prevSubheaderKey;
            }
            prevBaseSubheaderKey = baseKey;
            prevSubheaderKey     = key;

            if(!headers.contains(key)) {
                Container subheader;
                subheader.setTitle(title);
                subheader.setInfo(container.info());

                auto* subheaderItem      = getOrInsertItem(key, PlaylistItem::Subheader, subheader, parent, baseKey);
                auto& subheaderContainer = std::get<1>(subheaderItem->data());
                headers.emplace(key, &subheaderContainer);
            }
            Container* subheader = headers.at(key);
            subheader->addTrack(track);
            data.trackParents[track.id()].push_back(key);

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
            if(result.contains("||"_L1) && !leftFilled) {
                leftFilled          = true;
                const QString left  = result.section(u"||"_s, 0, 0);
                const QString right = result.section(u"||"_s, 1);
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

        const QString key = Utils::generateHash(parent->key(), track.hash(), QString::number(data.items.size()));

        auto* trackItem = getOrInsertItem(key, PlaylistItem::Track, playlistTrack, parent);
        trackItem->setIndex(currentIndex++);
        data.trackParents[track.id()].push_back(key);

        if(parent->type() != PlaylistItem::Header) {
            trackItem->setIndentation(parent->indentation() + 20);
        }
    }

    void runBatch(int size)
    {
        if(size <= 0) {
            return;
        }

        auto tracksBatch = std::ranges::views::take(pendingTracks, size);

        for(const Core::Track& track : tracksBatch) {
            if(!self->mayRun()) {
                return;
            }
            if(!track.enabled()) {
                continue;
            }
            iterateTrack(track);
        }

        updateContainers();

        if(!self->mayRun()) {
            return;
        }

        QMetaObject::invokeMethod(self, "populated", Q_ARG(PendingData, data));

        auto tracksToKeep = std::ranges::views::drop(pendingTracks, size);
        Core::TrackList tempTracks;
        std::ranges::copy(tracksToKeep, std::back_inserter(tempTracks));
        pendingTracks = std::move(tempTracks);

        data.nodes.clear();

        const auto remaining = static_cast<int>(pendingTracks.size());
        runBatch(std::min(remaining, BatchSize));
    }

    void runTracks()
    {
        for(const Core::Track& track : pendingTracks) {
            if(!self->mayRun()) {
                return;
            }
            if(!track.enabled()) {
                continue;
            }
            iterateTrack(track);
        }

        updateContainers();

        if(!self->mayRun()) {
            return;
        }

        QMetaObject::invokeMethod(self, "populatedTracks", Q_ARG(PendingData, data));
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
    p->headers.clear();
    p->currentIndex = 0;

    if(std::exchange(p->currentPreset, preset) != preset) {
        p->subheaders.clear();
    }

    p->updateScripts();
    p->pendingTracks = tracks;

    p->runBatch(InitialBatchSize);

    setState(Idle);
}

void PlaylistPopulator::runTracks(const PlaylistPreset& preset, const Core::TrackList& tracks, const QString& parent,
                                  int row)
{
    setState(Running);

    p->data.clear();
    p->headers.clear();
    p->currentIndex = 0;
    p->data.parent  = parent;
    p->data.row     = row;

    if(std::exchange(p->currentPreset, preset) != preset) {
        p->subheaders.clear();
    }

    p->updateScripts();
    p->pendingTracks = tracks;

    p->runTracks();

    setState(Idle);
}

void PlaylistPopulator::updateHeaders(const ItemList& headers)
{
    setState(Running);

    ItemKeyMap updatedHeaders;

    for(const PlaylistItem& item : headers) {
        Container& header = std::get<1>(item.data());
        header.updateGroupText(&p->parser, p->registry.get());
        updatedHeaders.emplace(item.key(), item);
    }

    emit headersUpdated(updatedHeaders);

    setState(Idle);
}

PlaylistPopulator::~PlaylistPopulator() = default;
} // namespace Fy::Gui::Widgets::Playlist
