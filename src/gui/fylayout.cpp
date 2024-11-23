/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fylayout.h>

#include <gui/theme/fytheme.h>
#include <utils/utils.h>

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMainWindow>

Q_LOGGING_CATEGORY(LAYOUT, "fy.layout")

using namespace Qt::StringLiterals;

namespace {
struct ReadResult
{
    QString name;
    QJsonObject json;
};

ReadResult readLayout(const QByteArray& json)
{
    const auto doc = QJsonDocument::fromJson(json);

    if(doc.isEmpty() || !doc.isObject()) {
        return {};
    }

    const auto layoutObject = doc.object();

    if(layoutObject.empty() || !layoutObject.contains(u"Name"_s)) {
        return {};
    }

    const QString name = layoutObject.value(u"Name"_s).toString();

    if(name.isEmpty()) {
        return {};
    }

    return {name, layoutObject};
}
} // namespace

namespace Fooyin {
FyLayout::FyLayout(QString name, QJsonObject json)
    : m_name{std::move(name)}
    , m_json{std::move(json)}
{ }

FyLayout::FyLayout(const QByteArray& file)
{
    const auto result = readLayout(file);

    m_name = result.name;
    m_json = result.json;
}

bool FyLayout::isValid() const
{
    return !m_name.isEmpty() && !m_json.isEmpty();
}

QString FyLayout::name() const
{
    return m_name;
}

QJsonObject FyLayout::json() const
{
    return m_json;
}

void FyLayout::saveWindowSize()
{
    auto* window = Utils::getMainWindow();
    if(!window) {
        return;
    }

    const auto size = window->size();
    QJsonObject jsonSize;
    jsonSize["Width"_L1]    = size.width();
    jsonSize["Height"_L1]   = size.height();
    m_json["WindowSize"_L1] = jsonSize;
}

void FyLayout::loadWindowSize() const
{
    if(!m_json.contains("WindowSize"_L1)) {
        return;
    }

    auto* window = Utils::getMainWindow();
    if(!window) {
        return;
    }

    const auto sizeVal = m_json.value("WindowSize"_L1);
    if(!sizeVal.isObject()) {
        qCWarning(LAYOUT) << "Invalid WindowSize value";
        return;
    }

    const auto sizeObj = sizeVal.toObject();

    QSize size;
    if(sizeObj.contains("Width"_L1)) {
        size.setWidth(sizeObj.value("Width"_L1).toInt());
    }
    if(sizeObj.contains("Height"_L1)) {
        size.setHeight(sizeObj.value("Height"_L1).toInt());
    }

    if(size.isValid()) {
        window->resize(size);
    }
}

void FyLayout::saveTheme(const FyTheme& theme, ThemeOptions options)
{
    if(!theme.isValid()) {
        return;
    }

    FyTheme saveTheme{theme};

    if(!(options & SaveColours)) {
        saveTheme.colours.clear();
    }
    if(!(options & SaveFonts)) {
        saveTheme.fonts.clear();
    }

    QByteArray currTheme;
    QDataStream stream{&currTheme, QDataStream::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << theme;
    m_json["Theme"_L1] = QString::fromUtf8(currTheme.toBase64());
}

FyTheme FyLayout::loadTheme() const
{
    if(!m_json.contains("Theme"_L1)) {
        return {};
    }

    FyTheme theme;

    auto data = QByteArray::fromBase64(m_json.value("Theme"_L1).toString().toUtf8());
    QDataStream stream{&data, QDataStream::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> theme;

    return theme;
}
} // namespace Fooyin
