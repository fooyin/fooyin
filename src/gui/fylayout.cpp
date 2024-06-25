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

#include <utils/utils.h>

#include <QJsonDocument>
#include <QMainWindow>

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

    if(layoutObject.empty() || !layoutObject.contains(QStringLiteral("Name"))) {
        return {};
    }

    const QString name = layoutObject.value(QStringLiteral("Name")).toString();

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
    jsonSize[u"Width"]    = size.width();
    jsonSize[u"Height"]   = size.height();
    m_json[u"WindowSize"] = jsonSize;
}

void FyLayout::loadWindowSize() const
{
    if(!m_json.contains(u"WindowSize")) {
        return;
    }

    auto* window = Utils::getMainWindow();
    if(!window) {
        return;
    }

    const auto sizeVal = m_json.value(u"WindowSize");
    if(!sizeVal.isObject()) {
        qInfo() << "Invalid WindowSize value";
        return;
    }

    const auto sizeObj = sizeVal.toObject();

    QSize size;
    if(sizeObj.contains(u"Width")) {
        size.setWidth(sizeObj.value(u"Width").toInt());
    }
    if(sizeObj.contains(u"Height")) {
        size.setHeight(sizeObj.value(u"Height").toInt());
    }

    if(size.isValid()) {
        window->resize(size);
    }
}
} // namespace Fooyin
