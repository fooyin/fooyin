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

#include "gui/autoplaylist/autoplaylistquery.h"
#include "gui/autoplaylist/autoplaylistrulerowwidget.h"
#include "gui/autoplaylist/autoplaylistruleswidget.h"

#include <core/scripting/trackqueryfilter.h>
#include <core/track.h>
#include <utils/scopeguard.h>

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QPushButton>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(AutoPlaylistTest, TextBoundaryRulesGenerateKeywordPhrases)
{
    Track track{u"/music/first.flac"_s};
    track.setTitle(u"The Moon (Live)"_s);
    const TrackList tracks{track};
    TrackQueryFilter filter;

    for(const auto& query : {uR"(title STARTS WITH "The ")"_s, u"title ENDS WITH \"(Live)\""_s,
                             uR"(title NOT STARTS WITH "Other")"_s, u"title NOT ENDS WITH \"(Demo)\""_s}) {
        const auto rules = AutoPlaylistQueryBuilder::rules(query, {});
        ASSERT_TRUE(rules.has_value());

        AutoPlaylistRulesWidget widget;
        widget.setRules(*rules);
        const auto [generated, sort] = AutoPlaylistQueryBuilder::query(widget.rules());

        EXPECT_EQ(u"(%1)"_s.arg(query), generated);
        EXPECT_TRUE(widget.error().isEmpty());
        EXPECT_TRUE(AutoPlaylistQueryBuilder::queryError(generated, sort).isEmpty());
        EXPECT_EQ(tracks, filter.filter(generated, tracks));
        EXPECT_TRUE(
            AutoPlaylistQueryBuilder::queryError(AutoPlaylistQueryBuilder::formatQuery(generated), {}).isEmpty());
    }
}

TEST(AutoPlaylistTest, LiteralListsRoundTripThroughRules)
{
    for(const auto& query :
        {uR"(genre IN ("Rock", "Jazz"))"_s, uR"(genre NOT IN ("Rock", "Jazz"))"_s,
         uR"(NOT (genre IN ("Rock", "Jazz")))"_s, u"playcount IN (0, 2, 01)"_s, u"rating IN (0, 3.5, 5)"_s,
         u"duration_s IN (30, 120)"_s, u"bitrate IN (128, 320)"_s, u"samplerate IN (44100, 96000)"_s,
         u"bitdepth IN (16, 24)"_s, u"$info(channels) IN (1, 2, 6)"_s, uR"(extension IN ("flac", "opus"))"_s,
         uR"($meta("CUSTOM") IN ("Rock, Jazz", "say \"hi\"", "C:\\Music"))"_s}) {
        SCOPED_TRACE(query.toStdString());
        const auto rules = AutoPlaylistQueryBuilder::rules(query, {});
        ASSERT_TRUE(rules.has_value());
        ASSERT_EQ(1, rules->group.rules.size());
        const auto& rule = std::get<AutoPlaylistRule>(rules->group.rules.front());
        EXPECT_EQ(Expr::In, rule.comparison);
        EXPECT_FALSE(rule.values.empty());

        AutoPlaylistRulesWidget widget;
        widget.setRules(*rules);
        EXPECT_TRUE(widget.error().isEmpty());

        const auto [generated, sort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_TRUE(AutoPlaylistQueryBuilder::queryError(generated, sort).isEmpty());

        const auto restored = AutoPlaylistQueryBuilder::rules(generated, sort);
        ASSERT_TRUE(restored.has_value());

        const auto& restoredRule = std::get<AutoPlaylistRule>(restored->group.rules.front());
        EXPECT_EQ(rule.values, restoredRule.values);
        EXPECT_EQ(rule.negated, restoredRule.negated);
        EXPECT_EQ(rule.source, restoredRule.source);
    }
}

TEST(AutoPlaylistTest, HasAnyListsRoundTripThroughRules)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"Moonlight (Live)"_s);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Studio Demo"_s);
    Track third{u"/music/third.flac"_s};
    third.setTitle(u"Studio edit"_s);
    const TrackList tracks{first, second, third};
    TrackQueryFilter filter;

    for(const auto& query : {uR"(title HAS ANY ("live", "demo"))"_s, uR"(title NOT HAS ANY ("live", "demo"))"_s,
                             uR"($meta("CUSTOM") HAS ANY ("Rock, Jazz", "say \"hi\"", "C:\\Music"))"_s}) {
        const auto rules = AutoPlaylistQueryBuilder::rules(query, {});
        ASSERT_TRUE(rules.has_value());

        const auto& rule = std::get<AutoPlaylistRule>(rules->group.rules.front());
        EXPECT_EQ(Expr::ContainsAny, rule.comparison);

        AutoPlaylistRulesWidget widget;
        widget.setRules(*rules);
        EXPECT_TRUE(widget.error().isEmpty());

        const auto [generated, sort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_EQ(u"(%1)"_s.arg(query), generated);
        EXPECT_TRUE(AutoPlaylistQueryBuilder::queryError(generated, sort).isEmpty());

        const QString formatted = AutoPlaylistQueryBuilder::formatQuery(generated);
        EXPECT_EQ(filter.filter(query, tracks), filter.filter(formatted, tracks));
        if(rule.field == u"title"_s) {
            EXPECT_EQ((rule.negated ? TrackList{third} : TrackList{first, second}), filter.filter(generated, tracks));
        }
    }
}

TEST(AutoPlaylistTest, NestedListsPreserveFilteringAndSorting)
{
    Track first{u"/music/first.flac"_s};
    first.setGenres({u"Rock"_s});
    first.setPlayCount(2);
    Track second{u"/music/second.flac"_s};
    second.setGenres({u"Jazz"_s});
    second.setPlayCount(3);
    Track third{u"/music/third.flac"_s};
    third.setGenres({u"Blues"_s});
    third.setPlayCount(4);
    const TrackList tracks{first, second, third};

    const QString query = uR"((genre IN ("Rock", "Jazz") OR playcount IN (4)) AND genre NOT IN ("Blues") LIMIT 1)"_s;
    const QString sort  = u"SORT DESCENDING BY playcount"_s;
    const auto rules    = AutoPlaylistQueryBuilder::rules(query, sort);
    ASSERT_TRUE(rules.has_value());

    AutoPlaylistRulesWidget widget;
    widget.setRules(*rules);
    const auto [generated, generatedSort] = AutoPlaylistQueryBuilder::query(widget.rules());
    EXPECT_EQ(u"SORT- %playcount%"_s, generatedSort);

    TrackQueryFilter filter;
    EXPECT_EQ(TrackList{second}, filter.filter(query + u' ' + sort, tracks));
    EXPECT_EQ(TrackList{second}, filter.filter(generated + u' ' + generatedSort, tracks));
}

TEST(AutoPlaylistTest, FileAndAudioRulesFilterTracks)
{
    Track first{u"/music/first.flac"_s};
    first.setBitrate(320);
    first.setSampleRate(44100);
    first.setBitDepth(24);
    first.setChannels(2);
    first.setCreatedTime(QDateTime{QDate{2024, 1, 1}, QTime{0, 0}}.toMSecsSinceEpoch());
    first.setModifiedTime(QDateTime{QDate{2025, 1, 1}, QTime{0, 0}}.toMSecsSinceEpoch());
    Track second{u"/music/second.opus"_s};
    second.setBitrate(128);
    second.setSampleRate(96000);
    second.setBitDepth(16);
    second.setChannels(6);
    second.setCreatedTime(QDateTime{QDate{2023, 1, 1}, QTime{0, 0}}.toMSecsSinceEpoch());
    second.setModifiedTime(QDateTime{QDate{2026, 1, 1}, QTime{0, 0}}.toMSecsSinceEpoch());
    const TrackList tracks{first, second};
    TrackQueryFilter filter;

    for(const auto& query : {u"bitrate > 128"_s, u"samplerate IS 44100"_s, u"bitdepth IS 24"_s,
                             u"$info(channels) IS 2"_s, uR"(extension IS "flac")"_s,
                             uR"(createdtime SINCE "2024-01-01")"_s, uR"(lastmodified BEFORE "2026-01-01")"_s}) {
        SCOPED_TRACE(query.toStdString());
        const auto rules = AutoPlaylistQueryBuilder::rules(query, {});
        ASSERT_TRUE(rules.has_value());

        AutoPlaylistRulesWidget widget;
        widget.setRules(*rules);
        EXPECT_TRUE(widget.error().isEmpty());

        const auto [generated, sort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_TRUE(AutoPlaylistQueryBuilder::queryError(generated, sort).isEmpty());
        EXPECT_EQ(TrackList{first}, filter.filter(query, tracks));
        EXPECT_EQ(TrackList{first}, filter.filter(generated, tracks));
    }
}

TEST(AutoPlaylistTest, CustomSortScriptsSurviveRuleEdits)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"First match"_s);
    first.setPlayCount(2);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Second match"_s);
    second.setPlayCount(3);
    Track third{u"/music/third.flac"_s};
    third.setTitle(u"Other track"_s);
    third.setPlayCount(4);
    const TrackList tracks{first, second, third};

    const QString query = uR"(title HAS "match" LIMIT 1)"_s;
    TrackQueryFilter filter;
    for(const auto& sort : {u"SORT- $add(%playcount%,1)"_s, u"SORT- %playcount% - %filepath%"_s,
                            u"SORT- %replaygain_track_gain% | %playcount%"_s}) {
        SCOPED_TRACE(sort.toStdString());
        const auto rules = AutoPlaylistQueryBuilder::rules(query, sort);
        ASSERT_TRUE(rules.has_value());
        ASSERT_TRUE(rules->sortScript.has_value());

        AutoPlaylistRulesWidget widget;
        widget.setRules(*rules);
        const auto [generated, generatedSort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_TRUE(widget.error().isEmpty());
        EXPECT_EQ(u"SORT- "_s + *rules->sortScript, generatedSort);
        EXPECT_EQ(rules->sortScript, widget.rules().sortScript);

        EXPECT_EQ(TrackList{second}, filter.filter(query + u' ' + sort, tracks));
        EXPECT_EQ(TrackList{second}, filter.filter(generated + u' ' + generatedSort, tracks));

        auto edited  = widget.rules();
        edited.limit = 2;
        widget.setRules(edited);
        const auto [editedQuery, editedSort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_EQ(generatedSort, editedSort);
        EXPECT_EQ((TrackList{second, first}), filter.filter(editedQuery + u' ' + editedSort, tracks));

        std::get<AutoPlaylistRule>(edited.group.rules.front()).value = u"First"_s;
        widget.setRules(edited);
        const auto [filteredQuery, filteredSort] = AutoPlaylistQueryBuilder::query(widget.rules());
        EXPECT_EQ(generatedSort, filteredSort);
        EXPECT_EQ(TrackList{first}, filter.filter(filteredQuery + u' ' + filteredSort, tracks));

        const auto restored = AutoPlaylistQueryBuilder::rules(filteredQuery, filteredSort);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(rules->sortScript, restored->sortScript);
        EXPECT_TRUE(restored->descending);
    }
}

TEST(AutoPlaylistTest, UnsupportedListsRemainScriptQueries)
{
    for(const auto& query :
        {uR"(genre IN (%artist%, "Jazz"))"_s, uR"(genre IN ($lower("ROCK")))"_s, uR"(genre IN (""))"_s,
         u"genre IN (\"Rock\nJazz\")"_s, u"playcount IN (-1, 2)"_s, u"playcount IN (1.5)"_s,
         u"playcount IN (2147483648)"_s, u"rating IN (3.25)"_s, u"rating IN (6)"_s, uR"(date IN ("2024", "2025"))"_s,
         u"loved IN (0, 1)"_s, uR"(title HAS ANY ($lower("LIVE")))"_s, uR"(title HAS ANY (%artist%))"_s,
         uR"(title HAS ANY (""))"_s, uR"(playcount HAS ANY ("1", "2"))"_s}) {
        EXPECT_FALSE(AutoPlaylistQueryBuilder::rules(query, {}).has_value());
    }
}

TEST(AutoPlaylistTest, ChangingComparisonsPreservesLists)
{
    AutoPlaylistRule rule;
    rule.field      = u"genre"_s;
    rule.comparison = Expr::In;
    rule.values     = {u"Rock"_s, u"Jazz"_s};
    AutoPlaylistRuleRowWidget row{rule, nullptr};
    row.show();

    QComboBox* comparison{nullptr};
    for(auto* combo : row.findChildren<QComboBox*>()) {
        if(combo->findData(Expr::In) >= 0) {
            comparison = combo;
            break;
        }
    }
    ASSERT_NE(nullptr, comparison);

    const auto* edit = row.findChild<QPushButton*>();
    EXPECT_TRUE(edit->isVisible());
    comparison->setCurrentIndex(comparison->findData(Expr::Equals));
    EXPECT_FALSE(edit->isVisible());
    comparison->setCurrentIndex(comparison->findData(Expr::ContainsAny));
    EXPECT_TRUE(edit->isVisible());
    EXPECT_EQ(Expr::ContainsAny, row.rule().comparison);
    EXPECT_EQ(rule.values, row.rule().values);
    comparison->setCurrentIndex(comparison->findText(AutoPlaylistRulesWidget::comparisonText(Expr::In, true, false)));
    EXPECT_TRUE(edit->isVisible());
    EXPECT_EQ(Expr::In, row.rule().comparison);
    EXPECT_TRUE(row.rule().negated);
    EXPECT_EQ(rule.values, row.rule().values);
}

TEST(AutoPlaylistTest, RuleValidationChecksLists)
{
    AutoPlaylistRules rules;
    AutoPlaylistRule rule;
    rule.field      = u"playcount"_s;
    rule.comparison = Expr::In;
    AutoPlaylistRulesWidget widget;

    for(const auto& values : {std::vector<QString>{}, std::vector{u"-1"_s}, std::vector{u"1"_s, u"invalid"_s}}) {
        rule.values       = values;
        rules.group.rules = {rule};
        widget.setRules(rules);
        EXPECT_FALSE(widget.error().isEmpty());
    }
    rule.values       = {u"0"_s, u"2"_s};
    rule.negated      = true;
    rules.group.rules = {rule};
    widget.setRules(rules);
    EXPECT_TRUE(widget.error().isEmpty());
}
} // namespace Fooyin::Testing

int main(int argc, char** argv)
{
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows");
#else
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    }

    const QApplication app{argc, argv};
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
