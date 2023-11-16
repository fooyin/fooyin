/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/crypto.h>

#include <QDateTime>
#include <QRandomGenerator>
#include <QUuid>

namespace Fooyin::Utils {
QString generateRandomHash()
{
    const QString uniqueId  = QUuid::createUuid().toString();
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(uniqueId.toUtf8());
    hash.addData(timestamp.toUtf8());

    QString headerKey = hash.result().toHex();
    return headerKey;
}
} // namespace Fooyin::Utils
