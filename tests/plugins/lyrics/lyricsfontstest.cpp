#include "lyricsfonts.h"

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(LyricsFontsTest, HighlightedFontsInheritConfiguredLineFont)
{
    QFont base;
    base.setPointSizeF(12);
    QFont line{base};
    line.setPointSizeF(18);
    const auto fonts = Lyrics::resolveLyricsFonts(base, {}, line.toString(), {}, {});
    EXPECT_NEAR(fonts.base.pointSizeF(), 12, 0.001);
    EXPECT_NEAR(fonts.line.pointSizeF(), 18, 0.001);
    EXPECT_EQ(fonts.wordLine, fonts.line);
    EXPECT_EQ(fonts.word, fonts.line);
}

TEST(LyricsFontsTest, DefaultsScaleBaseFontAndExplicitOverridesWin)
{
    QFont base;
    base.setPointSizeF(15);
    QFont wordLine{base};
    wordLine.setPointSizeF(10);
    QFont word{base};
    word.setPointSizeF(9);
    const auto defaults = Lyrics::resolveLyricsFonts(base, {}, {}, {}, {});
    EXPECT_NEAR(defaults.line.pointSizeF(), 18, 0.001);
    EXPECT_EQ(defaults.word, defaults.line);
    const auto inherited = Lyrics::resolveLyricsFonts(base, {}, {}, wordLine.toString(), {});
    EXPECT_EQ(inherited.word, wordLine);
    const auto overridden = Lyrics::resolveLyricsFonts(base, {}, {}, wordLine.toString(), word.toString());
    EXPECT_NEAR(overridden.wordLine.pointSizeF(), 10, 0.001);
    EXPECT_NEAR(overridden.word.pointSizeF(), 9, 0.001);
}

TEST(LyricsFontsTest, ScalesPixelFontsAndFallsBackOnInvalidSettings)
{
    QFont base;
    base.setPixelSize(15);
    const auto fonts = Lyrics::resolveLyricsFonts(base, u"invalid"_s, u"invalid"_s, {}, {});
    EXPECT_EQ(fonts.base, base);
    EXPECT_EQ(fonts.line.pixelSize(), 18);
    EXPECT_EQ(fonts.wordLine, fonts.line);
    EXPECT_EQ(fonts.word, fonts.line);
}
} // namespace Fooyin::Testing
