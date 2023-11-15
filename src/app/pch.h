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

#ifdef __cplusplus

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QList>
#include <QMetaObject>
#include <QMetaType>
#include <QModelIndex>
#include <QModelIndexList>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmapCache>
#include <QPluginLoader>
#include <QPointer>
#include <QSettings>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QString>
#include <QStringView>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QWidget>
#include <qglobal.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <type_traits>
#include <utility>

#endif
