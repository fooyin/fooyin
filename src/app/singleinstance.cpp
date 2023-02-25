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

#include "singleinstance.h"

#include <QCryptographicHash>

namespace Fy {
QString generateKeyHash(const QString& key, const QString& salt)
{
    QByteArray data;

    data.append(key.toUtf8());
    data.append(salt.toUtf8());

    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
}

SingleInstance::SingleInstance(const QString& key)
    : key{key}
    , memoryKey{generateKeyHash(key, "memoryKey")}
    , lockKey{generateKeyHash(key, "lockKey")}
    , memory{memoryKey}
    , lock{lockKey, 1}
{
    lock.acquire();
    {
        QSharedMemory fix(memoryKey);
        fix.attach();
    }
    lock.release();
}

SingleInstance::~SingleInstance()
{
    release();
}

bool SingleInstance::isRunning()
{
    if(memory.isAttached()) {
        return false;
    }

    lock.acquire();

    const bool isRunning = memory.attach();
    if(isRunning) {
        memory.detach();
    }

    lock.release();

    return isRunning;
}

bool SingleInstance::tryRunning()
{
    if(isRunning()) {
        return false;
    }

    lock.acquire();
    const bool result = memory.create(sizeof(uint64_t));
    lock.release();

    if(!result) {
        release();
        return false;
    }

    return true;
}

void SingleInstance::release()
{
    lock.acquire();

    if(memory.isAttached()) {
        memory.detach();
    }

    lock.release();
}
} // namespace Fy
