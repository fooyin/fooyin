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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QSaveFile>

#include <optional>

using namespace Qt::StringLiterals;

constexpr auto FormatName  = "fooyin-theme"_L1;
constexpr auto MaxFileSize = 1024 * 1024;

namespace Fooyin {
namespace {
QString groupName(QPalette::ColorGroup group)
{
    switch(group) {
        case QPalette::Active:
            return u"active"_s;
        case QPalette::Disabled:
            return u"disabled"_s;
        case QPalette::Inactive:
            return u"inactive"_s;
        case QPalette::All:
            return u"all"_s;
        default:
            return {};
    }
}

std::optional<QPalette::ColorGroup> groupFromName(QLatin1StringView name)
{
    if(name == "active"_L1) {
        return QPalette::Active;
    }
    if(name == "disabled"_L1) {
        return QPalette::Disabled;
    }
    if(name == "inactive"_L1) {
        return QPalette::Inactive;
    }
    if(name == "all"_L1) {
        return QPalette::All;
    }
    return {};
}

QString colourKey(const PaletteKey& key)
{
    const auto roleMeta  = QMetaEnum::fromType<QPalette::ColorRole>();
    const char* roleName = roleMeta.valueToKey(key.role);
    if(!roleName) {
        return {};
    }
    return groupName(key.group) + u"."_s + QString::fromLatin1(roleName);
}

std::optional<PaletteKey> paletteKey(const QString& name)
{
    const qsizetype separator = name.indexOf(u'.');
    if(separator <= 0 || separator == name.size() - 1 || name.indexOf(u'.', separator + 1) >= 0) {
        return {};
    }

    const auto group = groupFromName(QLatin1StringView{name.first(separator).toLatin1()});
    if(!group) {
        return {};
    }

    const auto roleMeta = QMetaEnum::fromType<QPalette::ColorRole>();

    bool validRole{false};
    const int role = roleMeta.keyToValue(name.sliced(separator + 1).toLatin1().constData(), &validRole);
    if(!validRole || role < 0 || role >= QPalette::NColorRoles) {
        return {};
    }

    return PaletteKey{static_cast<QPalette::ColorRole>(role), *group};
}

QString fontStyleName(QFont::Style style)
{
    switch(style) {
        case QFont::StyleItalic:
            return u"italic"_s;
        case QFont::StyleOblique:
            return u"oblique"_s;
        case QFont::StyleNormal:
        default:
            return u"normal"_s;
    }
}

std::optional<QFont::Style> fontStyle(const QString& name)
{
    if(name == u"normal"_s) {
        return QFont::StyleNormal;
    }
    if(name == u"italic"_s) {
        return QFont::StyleItalic;
    }
    if(name == u"oblique"_s) {
        return QFont::StyleOblique;
    }
    return {};
}

QString capitalisationName(QFont::Capitalization capitalisation)
{
    switch(capitalisation) {
        case QFont::SmallCaps:
            return u"smallCaps"_s;
        case QFont::AllUppercase:
            return u"allUppercase"_s;
        case QFont::AllLowercase:
            return u"allLowercase"_s;
        case QFont::Capitalize:
            return u"capitalize"_s;
        case QFont::MixedCase:
        default:
            return u"mixedCase"_s;
    }
}

std::optional<QFont::Capitalization> capitalisation(const QString& name)
{
    if(name == u"mixedCase"_s) {
        return QFont::MixedCase;
    }
    if(name == u"smallCaps"_s) {
        return QFont::SmallCaps;
    }
    if(name == u"allUppercase"_s) {
        return QFont::AllUppercase;
    }
    if(name == u"allLowercase"_s) {
        return QFont::AllLowercase;
    }
    if(name == u"capitalize"_s) {
        return QFont::Capitalize;
    }
    return {};
}

// We could just use QFont::toString(), but that isn't very cross-platform compatible, so we serialise as much as we can
QJsonObject fontToJson(const QFont& font)
{
    QJsonArray families;
    for(const QString& family : font.families()) {
        families.append(family);
    }

    QJsonObject json{{u"families"_s, families},
                     {u"weight"_s, font.weight()},
                     {u"style"_s, fontStyleName(font.style())},
                     {u"underline"_s, font.underline()},
                     {u"strikeOut"_s, font.strikeOut()},
                     {u"fixedPitch"_s, font.fixedPitch()},
                     {u"kerning"_s, font.kerning()},
                     {u"stretch"_s, font.stretch()},
                     {u"capitalisation"_s, capitalisationName(font.capitalization())},
                     {u"wordSpacing"_s, font.wordSpacing()}};

    if(font.pointSizeF() > 0) {
        json[u"pointSize"_s] = font.pointSizeF();
    }
    else if(font.pixelSize() > 0) {
        json[u"pixelSize"_s] = font.pixelSize();
    }

    if(!font.styleName().isEmpty()) {
        json[u"styleName"_s] = font.styleName();
    }

    json[u"letterSpacing"_s]
        = QJsonObject{{u"type"_s, font.letterSpacingType() == QFont::AbsoluteSpacing ? u"absolute"_s : u"percentage"_s},
                      {u"value"_s, font.letterSpacing()}};

    return json;
}

std::expected<bool, QString> readBool(const QJsonObject& json, QLatin1StringView key)
{
    if(!json.contains(key)) {
        return std::unexpected(u"Font property '%1' is missing."_s.arg(key));
    }
    if(!json.value(key).isBool()) {
        return std::unexpected(u"Font property '%1' must be a boolean."_s.arg(key));
    }
    return json.value(key).toBool();
}

std::expected<QFont, QString> fontFromJson(const QJsonValue& value)
{
    if(!value.isObject()) {
        return std::unexpected(u"Each font must be an object."_s);
    }

    const QJsonObject json = value.toObject();
    if(!json.value(u"families"_s).isArray()) {
        return std::unexpected(u"Font property 'families' must be an array."_s);
    }

    QStringList families;
    for(const QJsonValue& family : json.value(u"families"_s).toArray()) {
        if(!family.isString() || family.toString().isEmpty()) {
            return std::unexpected(u"Font families must be non-empty strings."_s);
        }
        families.emplace_back(family.toString());
    }
    if(families.isEmpty()) {
        return std::unexpected(u"A font must contain at least one family."_s);
    }

    QFont font;
    font.setFamilies(families);

    const bool hasPointSize = json.contains(u"pointSize"_s);
    const bool hasPixelSize = json.contains(u"pixelSize"_s);
    if(hasPointSize == hasPixelSize) {
        return std::unexpected(u"A font must contain exactly one of 'pointSize' or 'pixelSize'."_s);
    }

    if(hasPointSize) {
        const double size = json.value(u"pointSize"_s).toDouble(-1);
        if(size <= 0) {
            return std::unexpected(u"Font point size must be greater than zero."_s);
        }
        font.setPointSizeF(size);
    }
    else {
        const int size = json.value(u"pixelSize"_s).toInt(-1);
        if(size <= 0) {
            return std::unexpected(u"Font pixel size must be greater than zero."_s);
        }
        font.setPixelSize(size);
    }

    const int weight = json.value(u"weight"_s).toInt(-1);
    if(weight < 1 || weight > 1000) {
        return std::unexpected(u"Font weight must be between 1 and 1000."_s);
    }
    font.setWeight(static_cast<QFont::Weight>(weight));

    const auto style = fontStyle(json.value(u"style"_s).toString());
    if(!style) {
        return std::unexpected(u"Font style is invalid."_s);
    }
    font.setStyle(*style);

    const auto caps = capitalisation(json.value(u"capitalisation"_s).toString());
    if(!caps) {
        return std::unexpected(u"Font capitalisation is invalid."_s);
    }
    font.setCapitalization(*caps);

    const auto underline = readBool(json, "underline"_L1);
    if(!underline) {
        return std::unexpected(underline.error());
    }
    font.setUnderline(*underline);

    const auto strikeOut = readBool(json, "strikeOut"_L1);
    if(!strikeOut) {
        return std::unexpected(strikeOut.error());
    }
    font.setStrikeOut(*strikeOut);

    const auto fixedPitch = readBool(json, "fixedPitch"_L1);
    if(!fixedPitch) {
        return std::unexpected(fixedPitch.error());
    }
    font.setFixedPitch(*fixedPitch);

    const auto kerning = readBool(json, "kerning"_L1);
    if(!kerning) {
        return std::unexpected(kerning.error());
    }
    font.setKerning(*kerning);

    const int stretch = json.value(u"stretch"_s).toInt(-1);
    if(stretch < 1 || stretch > 4000) {
        return std::unexpected(u"Font stretch must be between 1 and 4000."_s);
    }
    font.setStretch(stretch);

    if(!json.value(u"wordSpacing"_s).isDouble()) {
        return std::unexpected(u"Font word spacing must be a number."_s);
    }
    font.setWordSpacing(json.value(u"wordSpacing"_s).toDouble());

    if(json.contains(u"styleName"_s)) {
        if(!json.value(u"styleName"_s).isString()) {
            return std::unexpected(u"Font style name must be a string."_s);
        }
        font.setStyleName(json.value(u"styleName"_s).toString());
    }

    const QJsonValue spacingValue = json.value(u"letterSpacing"_s);
    if(!spacingValue.isObject()) {
        return std::unexpected(u"Font letter spacing must be an object."_s);
    }

    const QJsonObject spacing = spacingValue.toObject();
    if(!spacing.value(u"value"_s).isDouble()) {
        return std::unexpected(u"Font letter spacing value must be a number."_s);
    }

    const QString spacingType = spacing.value(u"type"_s).toString();
    if(spacingType == u"absolute"_s) {
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing.value(u"value"_s).toDouble());
    }
    else if(spacingType == u"percentage"_s) {
        font.setLetterSpacing(QFont::PercentageSpacing, spacing.value(u"value"_s).toDouble());
    }
    else {
        return std::unexpected(u"Font letter spacing type is invalid."_s);
    }

    return font;
}
} // namespace

QByteArray FyThemeFile::toJson(const FyTheme& theme)
{
    QJsonObject colours;
    for(auto it = theme.colours.cbegin(); it != theme.colours.cend(); ++it) {
        const QString key = colourKey(it.key());
        if(!key.isEmpty() && it.value().isValid()) {
            colours[key] = it.value().name(QColor::HexArgb);
        }
    }

    QJsonObject fonts;
    for(auto it = theme.fonts.cbegin(); it != theme.fonts.cend(); ++it) {
        fonts[it.key().isEmpty() ? u"default"_s : it.key()] = fontToJson(it.value());
    }

    const QJsonObject root{{u"format"_s, FormatName},
                           {u"version"_s, CurrentVersion},
                           {u"name"_s, theme.name},
                           {u"colours"_s, colours},
                           {u"fonts"_s, fonts}};
    return QJsonDocument{root}.toJson();
}

FyThemeFile::ReadResult FyThemeFile::fromJson(const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if(parseError.error != QJsonParseError::NoError) {
        return std::unexpected(u"Invalid JSON: %1"_s.arg(parseError.errorString()));
    }

    if(!document.isObject()) {
        return std::unexpected(u"Theme file root must be an object."_s);
    }

    const QJsonObject root = document.object();
    if(root.value(u"format"_s).toString() != FormatName) {
        return std::unexpected(u"This is not a fooyin theme file."_s);
    }

    if(!root.value(u"version"_s).isDouble()) {
        return std::unexpected(u"Theme file version is missing or invalid."_s);
    }
    const double versionValue = root.value(u"version"_s).toDouble(-1);
    const int version         = static_cast<int>(versionValue);
    if(versionValue != version || version != CurrentVersion) {
        return std::unexpected(u"Theme file version %1 is not supported."_s.arg(versionValue));
    }

    const QString name = root.value(u"name"_s).toString().trimmed();
    if(name.isEmpty()) {
        return std::unexpected(u"Theme name is missing or empty."_s);
    }

    if(!root.value(u"colours"_s).isObject() || !root.value(u"fonts"_s).isObject()) {
        return std::unexpected(u"Theme colours and fonts must be objects."_s);
    }

    FyTheme theme;
    theme.name = name;

    static const QRegularExpression ColourPattern{u"^#[0-9A-Fa-f]{8}$"_s};
    const QJsonObject colours = root.value(u"colours"_s).toObject();
    for(auto it = colours.begin(); it != colours.end(); ++it) {
        const auto key = paletteKey(it.key());
        if(!key) {
            return std::unexpected(u"Unknown palette key '%1'."_s.arg(it.key()));
        }

        if(!it.value().isString() || !ColourPattern.match(it.value().toString()).hasMatch()) {
            return std::unexpected(u"Colour '%1' must use #AARRGGBB format."_s.arg(it.key()));
        }

        const QColor colour{it.value().toString()};
        if(!colour.isValid()) {
            return std::unexpected(u"Colour '%1' is invalid."_s.arg(it.key()));
        }
        theme.colours[*key] = colour;
    }

    const QJsonObject fonts = root.value(u"fonts"_s).toObject();
    for(auto it = fonts.begin(); it != fonts.end(); ++it) {
        const auto font = fontFromJson(it.value());
        if(!font) {
            return std::unexpected(u"Invalid font '%1': %2"_s.arg(it.key(), font.error()));
        }
        theme.fonts[it.key() == u"default"_s ? QString{} : it.key()] = *font;
    }

    if(!theme.isValid()) {
        return std::unexpected(u"Theme contains no colours or fonts."_s);
    }

    return theme;
}

FyThemeFile::ReadResult FyThemeFile::read(const QString& path)
{
    QFile file{path};
    if(!file.open(QIODevice::ReadOnly)) {
        return std::unexpected(file.errorString());
    }

    if(file.size() > MaxFileSize) {
        return std::unexpected(u"Theme file is too large."_s);
    }

    return fromJson(file.readAll());
}

FyThemeFile::WriteResult FyThemeFile::write(const FyTheme& theme, const QString& path)
{
    if(!theme.isValid() || theme.name.trimmed().isEmpty()) {
        return std::unexpected(u"Theme is empty or has no name."_s);
    }

    QSaveFile file{path};
    if(!file.open(QIODevice::WriteOnly)) {
        return std::unexpected(file.errorString());
    }

    const QByteArray data = toJson(theme);
    if(file.write(data) != data.size()) {
        const QString error = file.errorString();
        file.cancelWriting();
        return std::unexpected(error);
    }

    if(!file.commit()) {
        return std::unexpected(file.errorString());
    }

    return {};
}
} // namespace Fooyin
