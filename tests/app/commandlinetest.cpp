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

#include "commandline.h"

#include <gtest/gtest.h>

#include <QByteArray>

#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(CommandLineTest, ParsesSeekTimes)
{
    EXPECT_EQ(CommandLine::parseSeekTime(u"5000"), 5000);
    EXPECT_EQ(CommandLine::parseSeekTime(u"500ms"), 500);
    EXPECT_EQ(CommandLine::parseSeekTime(u"10s"), 10'000);
    EXPECT_EQ(CommandLine::parseSeekTime(u"2m"), 120'000);
    EXPECT_EQ(CommandLine::parseSeekTime(u"1h"), 3'600'000);
    EXPECT_EQ(CommandLine::parseSeekTime(u"1:30"), 90'000);
    EXPECT_EQ(CommandLine::parseSeekTime(u"1:02:03"), 3'723'000);
}

TEST(CommandLineTest, RejectsInvalidSeekTimes)
{
    EXPECT_FALSE(CommandLine::parseSeekTime(u""));
    EXPECT_FALSE(CommandLine::parseSeekTime(u"-1"));
    EXPECT_FALSE(CommandLine::parseSeekTime(u"10seconds"));
    EXPECT_FALSE(CommandLine::parseSeekTime(u"1:60"));
    EXPECT_FALSE(CommandLine::parseSeekTime(u"1:2:60"));
    EXPECT_FALSE(CommandLine::parseSeekTime(u" 10s"));
    EXPECT_FALSE(CommandLine::parseSeekTime(u"18446744073709551615h"));
}

#ifndef Q_OS_WIN
namespace {
class Arguments
{
public:
    Arguments(std::initializer_list<const char*> arguments)
    {
        m_values.reserve(arguments.size());
        for(const char* argument : arguments) {
            m_values.emplace_back(argument);
        }

        m_arguments.reserve(m_values.size());
        for(QByteArray& value : m_values) {
            m_arguments.push_back(value.data());
        }
    }

    [[nodiscard]] int size() const
    {
        return static_cast<int>(m_arguments.size());
    }

    [[nodiscard]] char** data()
    {
        return m_arguments.data();
    }

private:
    std::vector<QByteArray> m_values;
    std::vector<char*> m_arguments;
};
} // namespace

TEST(CommandLineTest, ParsesRemoteUrl)
{
    Arguments arguments{"fooyin", "https://example.com/live/stream"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::Run);
    ASSERT_EQ(command.files().size(), 1);
    EXPECT_EQ(command.files().front(), QUrl{u"https://example.com/live/stream"_s});
}

TEST(CommandLineTest, ReportsUnsupportedInput)
{
    Arguments arguments{"fooyin", "file-that-does-not-exist.flac"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::ExitFailure);
    EXPECT_FALSE(command.message().isEmpty());
    EXPECT_TRUE(command.files().empty());
}

TEST(CommandLineTest, RejectsConflictingPlayerOptions)
{
    Arguments arguments{"fooyin", "--play", "--stop"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::ExitFailure);
    EXPECT_EQ(command.message(), u"Only one player option can be used at a time."_s);
}

TEST(CommandLineTest, ParsesSeekOptionWithUnits)
{
    Arguments arguments{"fooyin", "--seek-forward", "1:30"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::Run);
    EXPECT_EQ(command.playerAction(), CommandLine::PlayerAction::SeekFwd);
    EXPECT_EQ(command.seekDelta(), 90'000);
}

TEST(CommandLineTest, ParsesVolumeAndPlaybackModes)
{
    Arguments arguments{"fooyin", "--volume", "42.5", "--repeat", "album", "--shuffle", "tracks"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::Run);
    EXPECT_EQ(command.volumeAction(), CommandLine::VolumeAction::Set);
    EXPECT_DOUBLE_EQ(command.volume(), 0.425);
    EXPECT_EQ(command.repeatMode(), CommandLine::RepeatMode::Album);
    EXPECT_EQ(command.shuffleMode(), CommandLine::ShuffleMode::Tracks);
}

TEST(CommandLineTest, ParsesMuteShortcut)
{
    Arguments arguments{"fooyin", "-m"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::Run);
    EXPECT_EQ(command.volumeAction(), CommandLine::VolumeAction::ToggleMute);
}

TEST(CommandLineTest, RejectsInvalidVolume)
{
    Arguments arguments{"fooyin", "--volume", "101"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::ExitFailure);
    EXPECT_FALSE(command.message().isEmpty());
}

TEST(CommandLineTest, HelpRequestsSuccessfulExit)
{
    Arguments arguments{"fooyin", "--help"};
    CommandLine command{arguments.size(), arguments.data()};

    EXPECT_EQ(command.parse(), CommandLine::ParseResult::ExitSuccess);
    EXPECT_TRUE(command.message().contains(u"--volume <percent>"));
    EXPECT_TRUE(command.message().contains(u"--repeat <mode>"));
}

TEST(CommandLineTest, SerialisesValidatedOptions)
{
    Arguments arguments{"fooyin", "--seek-backward", "10s", "https://example.com/stream"};
    CommandLine source{arguments.size(), arguments.data()};
    ASSERT_EQ(source.parse(), CommandLine::ParseResult::Run);

    CommandLine restored;
    ASSERT_TRUE(restored.loadOptions(source.saveOptions()));
    EXPECT_EQ(restored.playerAction(), CommandLine::PlayerAction::SeekBack);
    EXPECT_EQ(restored.seekDelta(), 10'000);
    EXPECT_EQ(restored.files(), source.files());
}
#endif

TEST(CommandLineTest, RejectsInvalidSerialisedOptions)
{
    CommandLine command;

    EXPECT_FALSE(command.loadOptions(QByteArray{"invalid"}));
}
} // namespace Fooyin::Testing
