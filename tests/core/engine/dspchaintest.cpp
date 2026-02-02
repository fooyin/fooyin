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

#include "core/engine/dsp/dspchain.h"
#include "core/engine/dsp/dspregistry.h"

#include <gtest/gtest.h>

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
class TestDspNode : public DspNode
{
public:
    using OutputFormatFn = std::function<AudioFormat(const AudioFormat&)>;

    explicit TestDspNode(QString nodeId, double gain = 1.0, int latency = 0)
        : m_id{std::move(nodeId)}
        , m_gain{gain}
        , m_latency{latency}
    { }

    QString name() const override
    {
        return m_id;
    }

    QString id() const override
    {
        return m_id;
    }

    void prepare(const AudioFormat& format) override
    {
        ++prepareCount;
        lastPrepared = format;
    }

    AudioFormat outputFormat(const AudioFormat& input) const override
    {
        if(outputFormatFn) {
            return outputFormatFn(input);
        }
        return input;
    }

    void process(ProcessingBufferList& chunks) override
    {
        ++processCount;
        for(size_t i = 0; i < chunks.count(); ++i) {
            auto* chunk = chunks.item(i);
            if(!chunk) {
                continue;
            }
            auto data = chunk->data();
            for(auto& sample : data) {
                sample *= m_gain;
            }
        }
    }

    void reset() override
    {
        ++resetCount;
    }

    void flush(ProcessingBufferList& chunks, FlushMode /*mode*/) override
    {
        ++flushCount;
        if(!emitChunkOnFlush) {
            return;
        }
        const AudioFormat format = lastPrepared.isValid() ? lastPrepared : AudioFormat{SampleFormat::F64, 48000, 1};
        auto* buffer             = chunks.addItem(format, 0, 1);
        if(buffer) {
            auto data = buffer->data();
            if(!data.empty()) {
                data[0] = flushSample;
            }
        }
    }

    int latencyFrames() const override
    {
        return m_latency;
    }

    OutputFormatFn outputFormatFn;
    bool emitChunkOnFlush{false};
    double flushSample{0.5};
    int prepareCount{0};
    int processCount{0};
    int resetCount{0};
    int flushCount{0};
    AudioFormat lastPrepared;

private:
    QString m_id;
    double m_gain;
    int m_latency;
};

int totalFrames(const ProcessingBufferList& chunks)
{
    int total{0};
    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(chunk && chunk->isValid()) {
            total += chunk->frameCount();
        }
    }
    return total;
}

void appendConstantMono(ProcessingBufferList& chunks, const AudioFormat& format, int frames, double sample)
{
    const size_t samples = static_cast<size_t>(std::max(frames, 0));
    auto* chunk          = chunks.addItem(format, 0, samples);
    ASSERT_TRUE(chunk);
    auto data = chunk->data();
    ASSERT_EQ(data.size(), samples);
    std::fill(data.begin(), data.end(), sample);
}

std::unique_ptr<DspNode> makeSkipSilenceNode(int minSilenceDurationMs, int thresholdDb, bool keepInitial,
                                             bool includeMiddle)
{
    DspRegistry registry;
    auto node = registry.create(u"fooyin.dsp.skipsilence"_s);
    EXPECT_TRUE(node);
    if(!node) {
        return {};
    }

    QByteArray preset;
    QDataStream stream(&preset, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32(1);
    stream << qint32(minSilenceDurationMs);
    stream << qint32(thresholdDb);
    stream << keepInitial;
    stream << includeMiddle;
    EXPECT_TRUE(node->loadSettings(preset));
    return node;
}
} // namespace

TEST(DSPChainTest, MutatesNodeOrderWithAddInsertMoveAndRemove)
{
    DspChain chain;
    auto a     = std::make_unique<TestDspNode>(u"a"_s);
    auto b     = std::make_unique<TestDspNode>(u"b"_s);
    auto c     = std::make_unique<TestDspNode>(u"c"_s);
    auto* cPtr = c.get();

    chain.addNode(std::move(a));
    chain.addNode(std::move(c));
    chain.insertNode(1, std::move(b));

    auto snapshot = chain.snapshot();
    ASSERT_EQ(snapshot.size(), 3U);
    EXPECT_EQ(snapshot[0].id, u"a"_s);
    EXPECT_EQ(snapshot[1].id, u"b"_s);
    EXPECT_EQ(snapshot[2].id, u"c"_s);

    chain.moveNode(2, 0);
    snapshot = chain.snapshot();
    ASSERT_EQ(snapshot.size(), 3U);
    EXPECT_EQ(snapshot[0].id, u"c"_s);
    EXPECT_EQ(snapshot[1].id, u"a"_s);
    EXPECT_EQ(snapshot[2].id, u"b"_s);

    chain.removeNode(1);
    snapshot = chain.snapshot();
    ASSERT_EQ(snapshot.size(), 2U);
    EXPECT_EQ(snapshot[0].id, u"c"_s);
    EXPECT_EQ(snapshot[1].id, u"b"_s);

    chain.removeNode(cPtr);
    snapshot = chain.snapshot();
    ASSERT_EQ(snapshot.size(), 1U);
    EXPECT_EQ(snapshot[0].id, u"b"_s);
}

TEST(DSPChainTest, PreparePropagatesFormatAndComputesTotalLatency)
{
    auto first            = std::make_unique<TestDspNode>(u"first"_s, 1.0, 3);
    auto* firstPtr        = first.get();
    first->outputFormatFn = [](const AudioFormat& input) {
        auto output = input;
        output.setSampleRate(input.sampleRate() + 1000);
        return output;
    };

    auto second            = std::make_unique<TestDspNode>(u"second"_s, 1.0, 5);
    auto* secondPtr        = second.get();
    second->outputFormatFn = [](const AudioFormat& input) {
        auto output = input;
        output.setChannelCount(input.channelCount() + 1);
        return output;
    };

    DspChain chain;
    std::vector<DspNodePtr> nodes;
    nodes.push_back(std::move(first));
    nodes.push_back(std::move(second));
    chain.replaceNodes(std::move(nodes));

    const AudioFormat input{SampleFormat::F64, 48000, 2};
    chain.prepare(input);

    const auto output = chain.outputFormat();
    EXPECT_EQ(output.sampleRate(), 49000);
    EXPECT_EQ(output.channelCount(), 3);

    EXPECT_EQ(firstPtr->prepareCount, 1);
    EXPECT_EQ(secondPtr->prepareCount, 1);
    EXPECT_EQ(firstPtr->lastPrepared.sampleRate(), 48000);
    EXPECT_EQ(secondPtr->lastPrepared.sampleRate(), 49000);
    EXPECT_EQ(secondPtr->lastPrepared.channelCount(), 2);

    EXPECT_EQ(chain.totalLatencyFrames(), 8);
    const double expectedSeconds = (3.0 / 48000.0) + (5.0 / 49000.0);
    EXPECT_NEAR(chain.totalLatencySeconds(), expectedSeconds, 1e-12);
}

TEST(DSPChainTest, FlushProcessesChunksProducedDuringFlush)
{
    auto node              = std::make_unique<TestDspNode>(u"flush-node"_s, 2.0);
    auto* nodePtr          = node.get();
    node->emitChunkOnFlush = true;

    DspChain chain;
    chain.addNode(std::move(node));
    chain.prepare(AudioFormat{SampleFormat::F64, 48000, 1});

    ProcessingBufferList chunks;
    chain.flush(chunks, DspNode::FlushMode::EndOfTrack);

    EXPECT_EQ(nodePtr->flushCount, 1);
    EXPECT_EQ(nodePtr->processCount, 0);
    ASSERT_EQ(chunks.count(), 1U);
    const auto* buffer = chunks.item(0);
    ASSERT_TRUE(buffer);
    const auto data = buffer->constData();
    ASSERT_EQ(data.size(), 1U);
    EXPECT_DOUBLE_EQ(data[0], 0.5);
}

TEST(DSPChainTest, FlushRoutesTailThroughDownstreamNodesOnly)
{
    auto first              = std::make_unique<TestDspNode>(u"first"_s, 2.0);
    auto* firstPtr          = first.get();
    first->emitChunkOnFlush = true;
    first->flushSample      = 0.5;

    auto second     = std::make_unique<TestDspNode>(u"second"_s, 3.0);
    auto* secondPtr = second.get();

    DspChain chain;
    chain.addNode(std::move(first));
    chain.addNode(std::move(second));
    chain.prepare(AudioFormat{SampleFormat::F64, 48000, 1});

    ProcessingBufferList chunks;
    chain.flush(chunks, DspNode::FlushMode::EndOfTrack);

    EXPECT_EQ(firstPtr->flushCount, 1);
    EXPECT_EQ(firstPtr->processCount, 0);
    EXPECT_EQ(secondPtr->flushCount, 1);
    EXPECT_EQ(secondPtr->processCount, 1);

    ASSERT_EQ(chunks.count(), 1U);
    const auto* buffer = chunks.item(0);
    ASSERT_TRUE(buffer);
    const auto data = buffer->constData();
    ASSERT_EQ(data.size(), 1U);
    EXPECT_DOUBLE_EQ(data[0], 1.5);
}

TEST(DSPChainTest, ResetCallsResetOnAllNodes)
{
    auto first      = std::make_unique<TestDspNode>(u"first"_s);
    auto* firstPtr  = first.get();
    auto second     = std::make_unique<TestDspNode>(u"second"_s);
    auto* secondPtr = second.get();

    DspChain chain;
    chain.addNode(std::move(first));
    chain.addNode(std::move(second));

    chain.reset();

    EXPECT_EQ(firstPtr->resetCount, 1);
    EXPECT_EQ(secondPtr->resetCount, 1);
}

TEST(DSPChainTest, SkipSilenceIncludeMiddleSettingControlsMiddleSilenceRemoval)
{
    const AudioFormat format{SampleFormat::F64, 1000, 1};

    auto makeChunks = [&format]() {
        ProcessingBufferList chunks;
        appendConstantMono(chunks, format, 100, 1.0);
        appendConstantMono(chunks, format, 300, 0.0);
        appendConstantMono(chunks, format, 100, 1.0);
        return chunks;
    };

    auto dropMiddle = makeSkipSilenceNode(250, -59, true, false);
    ASSERT_TRUE(dropMiddle);
    dropMiddle->prepare(format);

    auto dropChunks = makeChunks();
    dropMiddle->process(dropChunks);
    EXPECT_EQ(totalFrames(dropChunks), 200);

    auto keepMiddle = makeSkipSilenceNode(250, -59, true, true);
    ASSERT_TRUE(keepMiddle);
    keepMiddle->prepare(format);

    auto keepChunks = makeChunks();
    keepMiddle->process(keepChunks);
    EXPECT_EQ(totalFrames(keepChunks), 500);
}

TEST(DSPChainTest, SkipSilencePrepareClearsPendingStateOnFormatChange)
{
    const AudioFormat firstFormat{SampleFormat::F64, 1000, 1};
    const AudioFormat secondFormat{SampleFormat::F64, 2000, 1};

    auto dsp = makeSkipSilenceNode(250, -59, false, false);
    ASSERT_TRUE(dsp);
    dsp->prepare(firstFormat);

    ProcessingBufferList firstChunks;
    appendConstantMono(firstChunks, firstFormat, 300, 0.0);
    dsp->process(firstChunks);
    EXPECT_EQ(totalFrames(firstChunks), 0);

    dsp->prepare(secondFormat);

    ProcessingBufferList secondChunks;
    appendConstantMono(secondChunks, secondFormat, 100, 1.0);
    dsp->process(secondChunks);
    EXPECT_EQ(totalFrames(secondChunks), 100);
}
} // namespace Fooyin::Testing
