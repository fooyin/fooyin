#pragma once

#include <QFont>
#include <QString>

namespace Fooyin::Lyrics {
struct LyricsFonts
{
    QFont base;
    QFont line;
    QFont wordLine;
    QFont word;
};

inline LyricsFonts resolveLyricsFonts(const QFont& defaultFont, const QString& baseFont, const QString& lineFont,
                                     const QString& wordLineFont, const QString& wordFont)
{
    const auto resolve = [](const QString& value, const QFont& fallback) {
        QFont font;
        return !value.isEmpty() && font.fromString(value) ? font : fallback;
    };

    LyricsFonts fonts;
    fonts.base = resolve(baseFont, defaultFont);
    QFont highlighted{fonts.base};
    if(highlighted.pointSizeF() > 0) {
        highlighted.setPointSizeF(highlighted.pointSizeF() * 1.2);
    }
    else if(highlighted.pixelSize() > 0) {
        highlighted.setPixelSize(qMax(highlighted.pixelSize() + 1, qRound(highlighted.pixelSize() * 1.2)));
    }
    fonts.line     = resolve(lineFont, highlighted);
    fonts.wordLine = resolve(wordLineFont, fonts.line);
    fonts.word     = resolve(wordFont, fonts.wordLine);
    return fonts;
}
} // namespace Fooyin::Lyrics
