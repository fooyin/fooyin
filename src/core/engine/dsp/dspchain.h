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

#pragma once

#include "fycore_export.h"

#include <core/engine/audioformat.h>
#include <core/engine/dsp/dspnode.h>
#include <core/engine/dsp/processingbufferlist.h>

#include <QByteArray>
#include <QString>

#include <vector>

namespace Fooyin {
/*!
 * Chain of DSP processors.
 * Audio flows through each node in sequence.
 * Intended to be used from a single owner thread (the audio thread).
 */
class FYCORE_EXPORT DspChain
{
public:
    struct NodeSnapshot
    {
        QString id;
        QString name;
        QByteArray settings;
    };

    DspChain();

    DspChain(const DspChain&)            = delete;
    DspChain& operator=(const DspChain&) = delete;

    //! Set the audio format for all nodes
    void prepare(const AudioFormat& format);

    //! Get the input (pre-DSP) format
    [[nodiscard]] AudioFormat inputFormat() const;

    //! Get the output (post-DSP) format for the current input
    [[nodiscard]] AudioFormat outputFormat() const;

    //! Predict output format for a given input using current chain
    [[nodiscard]] AudioFormat outputFormatFor(const AudioFormat& input) const;

    //! Add a node to the end of the chain
    void addNode(DspNodePtr node);

    //! Insert a node at a specific position
    void insertNode(size_t index, DspNodePtr node);

    //! Remove a node from the chain
    void removeNode(DspNode* node);

    //! Remove node at index
    void removeNode(size_t index);

    //! Move node from one position to another
    void moveNode(size_t from, size_t to);

    //! Clear all nodes
    void clear();

    //! Get number of nodes
    [[nodiscard]] size_t size() const;

    //! Get node at index (for UI)
    [[nodiscard]] DspNode* nodeAt(size_t index) const;

    //! Process audio through all enabled nodes
    void process(ProcessingBufferList& chunks);

    //! Reset all nodes
    void reset();

    //! Flush all nodes
    void flush(ProcessingBufferList& chunks, DspNode::FlushMode mode);

    //! Replace all user DSP nodes with the provided list
    void replaceNodes(std::vector<DspNodePtr> userNodes);

    //! Snapshot of current DSP nodes
    [[nodiscard]] std::vector<NodeSnapshot> snapshot() const;

    //! Get total latency in frames
    [[nodiscard]] int totalLatencyFrames() const;
    //! Get total latency in seconds.
    [[nodiscard]] double totalLatencySeconds() const;

private:
    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;

    std::vector<DspNodePtr> m_nodes;

    int m_totalLatencyFrames;
    double m_totalLatencySeconds;
};
} // namespace Fooyin
