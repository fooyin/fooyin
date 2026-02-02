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

#include "dspchain.h"

#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace Fooyin {
DspChain::DspChain()
    : m_totalLatencyFrames{0}
    , m_totalLatencySeconds{0.0}
{ }

void DspChain::prepare(const AudioFormat& format)
{
    m_inputFormat         = format;
    m_totalLatencyFrames  = 0;
    m_totalLatencySeconds = 0.0;

    if(!m_inputFormat.isValid()) {
        m_outputFormat = {};
        return;
    }

    AudioFormat current = m_inputFormat;
    for(auto& node : m_nodes) {
        if(!node) {
            continue;
        }

        node->prepare(current);

        const int latencyFrames = std::max(0, node->latencyFrames());
        m_totalLatencyFrames += latencyFrames;

        if(current.sampleRate() > 0 && latencyFrames > 0) {
            m_totalLatencySeconds += static_cast<double>(latencyFrames) / static_cast<double>(current.sampleRate());
        }

        current = node->outputFormat(current);
    }

    m_outputFormat = current;
}

AudioFormat DspChain::inputFormat() const
{
    return m_inputFormat;
}

AudioFormat DspChain::outputFormat() const
{
    return m_outputFormat;
}

AudioFormat DspChain::outputFormatFor(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    AudioFormat current{input};

    for(const auto& node : m_nodes) {
        if(node) {
            current = node->outputFormat(current);
        }
    }

    return current;
}

void DspChain::addNode(DspNodePtr node)
{
    m_nodes.push_back(std::move(node));

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
    else {
        m_outputFormat = {};
    }
}

void DspChain::insertNode(size_t index, DspNodePtr node)
{
    if(index >= m_nodes.size()) {
        m_nodes.push_back(std::move(node));
    }
    else {
        m_nodes.insert(m_nodes.begin() + static_cast<ptrdiff_t>(index), std::move(node));
    }

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
}

void DspChain::removeNode(DspNode* node)
{
    std::erase_if(m_nodes, [node](const auto& n) { return n.get() == node; });

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
}

void DspChain::removeNode(size_t index)
{
    if(index < m_nodes.size()) {
        m_nodes.erase(m_nodes.begin() + static_cast<ptrdiff_t>(index));
    }

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
}

void DspChain::moveNode(size_t from, size_t to)
{
    if(from >= m_nodes.size() || to >= m_nodes.size() || from == to) {
        return;
    }

    auto node = std::move(m_nodes[from]);
    m_nodes.erase(m_nodes.begin() + static_cast<ptrdiff_t>(from));
    m_nodes.insert(m_nodes.begin() + static_cast<ptrdiff_t>(to), std::move(node));

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
}

void DspChain::clear()
{
    m_nodes.clear();

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
    else {
        m_outputFormat = {};
    }
}

size_t DspChain::size() const
{
    return m_nodes.size();
}

DspNode* DspChain::nodeAt(size_t index) const
{
    return index < m_nodes.size() ? m_nodes[index].get() : nullptr;
}

void DspChain::process(ProcessingBufferList& chunks)
{
    for(auto& node : m_nodes) {
        node->process(chunks);
    }
}

void DspChain::reset()
{
    for(auto& node : m_nodes) {
        node->reset();
    }
}

void DspChain::flush(ProcessingBufferList& chunks, DspNode::FlushMode mode)
{
    ProcessingBufferList flushedChunks;
    const size_t nodeCount = m_nodes.size();

    for(size_t nodeIndex{0}; nodeIndex < nodeCount; ++nodeIndex) {
        auto& node = m_nodes[nodeIndex];
        if(!node) {
            continue;
        }

        ProcessingBufferList nodeTailChunks;
        node->flush(nodeTailChunks, mode);
        if(nodeTailChunks.count() == 0) {
            continue;
        }

        for(size_t downstreamIndex{nodeIndex + 1}; downstreamIndex < nodeCount; ++downstreamIndex) {
            auto& downstreamNode = m_nodes[downstreamIndex];
            if(downstreamNode) {
                downstreamNode->process(nodeTailChunks);
            }
        }

        const size_t tailCount = nodeTailChunks.count();
        for(size_t i{0}; i < tailCount; ++i) {
            const auto* chunk = nodeTailChunks.item(i);
            if(chunk && chunk->isValid()) {
                flushedChunks.addChunk(*chunk);
            }
        }
    }

    const size_t flushedCount = flushedChunks.count();
    for(size_t i{0}; i < flushedCount; ++i) {
        const auto* chunk = flushedChunks.item(i);
        if(chunk && chunk->isValid()) {
            chunks.addChunk(*chunk);
        }
    }
}

void DspChain::replaceNodes(std::vector<DspNodePtr> nodes)
{
    m_nodes = std::move(nodes);

    if(m_inputFormat.isValid()) {
        prepare(m_inputFormat);
    }
}

std::vector<DspChain::NodeSnapshot> DspChain::snapshot() const
{
    std::vector<NodeSnapshot> result;
    result.reserve(m_nodes.size());

    for(const auto& node : m_nodes) {
        NodeSnapshot snapshot;
        snapshot.id       = node->id();
        snapshot.name     = node->name();
        snapshot.settings = node->saveSettings();
        result.push_back(std::move(snapshot));
    }

    return result;
}

int DspChain::totalLatencyFrames() const
{
    return m_totalLatencyFrames;
}

double DspChain::totalLatencySeconds() const
{
    return m_totalLatencySeconds;
}
} // namespace Fooyin
