/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "testutils.h"

#include <QDir>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TempResource::TempResource(const QString& filename, QObject* parent)
    : QTemporaryFile{parent}
    , m_file{filename}
{
    setFileTemplate(QDir::tempPath() + QStringLiteral("/fooyin_test_XXXXXXXXXXXXXXX"));

    if(open()) {
        QFile resource{filename};
        if(resource.open(QIODevice::ReadOnly)) {
            write(resource.readAll());
        }
    }

    QTemporaryFile::reset();
}
void TempResource::checkValid() const
{
    QByteArray origFileData;
    QByteArray tmpFileData;
    {
        QFile origFile{m_file};
        const bool isOpen = origFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(origFile.isOpen());

        if(isOpen) {
            origFileData = origFile.readAll();
            origFile.close();
        }
    }

    {
        QFile tmpFile{fileName()};
        const bool isOpen = tmpFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(tmpFile.isOpen());

        if(isOpen) {
            tmpFileData = tmpFile.readAll();
            tmpFile.close();
        }
    }

    EXPECT_TRUE(!origFileData.isEmpty());
    EXPECT_TRUE(!tmpFileData.isEmpty());
    EXPECT_EQ(origFileData, tmpFileData);
}
} // namespace Fooyin::Testing
