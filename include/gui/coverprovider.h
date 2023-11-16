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

#pragma once

#include "fycore_export.h"

#include <core/track.h>

#include <QObject>

#include <memory>

class QPixmap;
class QString;
class QSize;

namespace Fooyin {
class FYCORE_EXPORT CoverProvider : public QObject
{
    Q_OBJECT

public:
    explicit CoverProvider(QObject* parent = nullptr);
    ~CoverProvider() override;

    [[nodiscard]] QPixmap trackCover(const Track& track, bool saveToDisk = false) const;
    [[nodiscard]] QPixmap trackCover(const Track& track, const QSize& size, bool saveToDisk = false) const;

    void clearCache();

signals:
    void coverAdded(const Fooyin::Track& track);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
