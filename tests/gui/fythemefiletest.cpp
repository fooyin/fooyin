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

#include <gui/theme/fythemefile.h>

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
FyTheme makeTheme()
{
    QFont font;
    font.setFamilies({u"Example Sans"_s, u"sans-serif"_s});
    font.setPointSizeF(10.5);
    font.setWeight(QFont::DemiBold);
    font.setStyle(QFont::StyleItalic);
    font.setUnderline(true);
    font.setStrikeOut(true);
    font.setFixedPitch(false);
    font.setKerning(false);
    font.setStretch(110);
    font.setCapitalization(QFont::SmallCaps);
    font.setWordSpacing(1.25);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.75);

    FyTheme theme;
    theme.id        = 42;
    theme.index     = 7;
    theme.name      = u"Portable Theme"_s;
    theme.isDefault = true;
    theme.colours   = {{PaletteKey{QPalette::Window}, QColor{10, 20, 30, 40}},
                       {PaletteKey{QPalette::WindowText, QPalette::Disabled}, QColor{200, 190, 180}}};
    theme.fonts     = {{QString{}, font}, {u"Fooyin::PlaylistView"_s, font}};
    return theme;
}
} // namespace

TEST(FyThemeFileTest, JsonRoundTripPreservesThemeData)
{
    const FyTheme source = makeTheme();
    const auto result    = FyThemeFile::fromJson(FyThemeFile::toJson(source));

    ASSERT_TRUE(result.has_value()) << result.error().toStdString();
    EXPECT_EQ(source.name, result->name);
    EXPECT_EQ(0, result->id);
    EXPECT_EQ(0, result->index);
    EXPECT_FALSE(result->isDefault);
    EXPECT_EQ(source.colours, result->colours);
    ASSERT_EQ(source.fonts.size(), result->fonts.size());

    const QFont sourceFont = source.fonts.value({});
    const QFont resultFont = result->fonts.value({});
    EXPECT_EQ(sourceFont.families(), resultFont.families());
    EXPECT_DOUBLE_EQ(sourceFont.pointSizeF(), resultFont.pointSizeF());
    EXPECT_EQ(sourceFont.weight(), resultFont.weight());
    EXPECT_EQ(sourceFont.style(), resultFont.style());
    EXPECT_EQ(sourceFont.underline(), resultFont.underline());
    EXPECT_EQ(sourceFont.strikeOut(), resultFont.strikeOut());
    EXPECT_EQ(sourceFont.kerning(), resultFont.kerning());
    EXPECT_EQ(sourceFont.stretch(), resultFont.stretch());
    EXPECT_EQ(sourceFont.capitalization(), resultFont.capitalization());
    EXPECT_DOUBLE_EQ(sourceFont.wordSpacing(), resultFont.wordSpacing());
    EXPECT_EQ(sourceFont.letterSpacingType(), resultFont.letterSpacingType());
    EXPECT_DOUBLE_EQ(sourceFont.letterSpacing(), resultFont.letterSpacing());
}

TEST(FyThemeFileTest, ReadAndWriteThemeFile)
{
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const QString path     = directory.filePath(u"theme.fyt"_s);
    const auto writeResult = FyThemeFile::write(makeTheme(), path);
    ASSERT_TRUE(writeResult.has_value()) << writeResult.error().toStdString();

    const auto result = FyThemeFile::read(path);
    ASSERT_TRUE(result.has_value()) << result.error().toStdString();
    EXPECT_EQ(u"Portable Theme"_s, result->name);
}

TEST(FyThemeFileTest, RejectsUnsupportedVersion)
{
    QJsonObject json   = QJsonDocument::fromJson(FyThemeFile::toJson(makeTheme())).object();
    json[u"version"_s] = FyThemeFile::CurrentVersion + 1;

    const auto result = FyThemeFile::fromJson(QJsonDocument{json}.toJson());
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().isEmpty());
}

TEST(FyThemeFileTest, RejectsInvalidPaletteEntry)
{
    QJsonObject json                 = QJsonDocument::fromJson(FyThemeFile::toJson(makeTheme())).object();
    QJsonObject colours              = json.value(u"colours"_s).toObject();
    colours[u"all.NotAColourRole"_s] = u"#ff112233"_s;
    json[u"colours"_s]               = colours;

    const auto result = FyThemeFile::fromJson(QJsonDocument{json}.toJson());
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().isEmpty());
}
} // namespace Fooyin::Testing
