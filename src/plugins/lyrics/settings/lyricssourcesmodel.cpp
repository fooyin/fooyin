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

#include "lyricssourcesmodel.h"

#include "sources/lyricsource.h"

#include <utils/helpers.h>

#include <QIODevice>
#include <QMimeData>

constexpr auto LyricItems = "application/x-fooyin-lyricsources";

namespace Fooyin::Lyrics {
LyricsSourcesModel::LyricsSourcesModel(QObject* parent)
    : QAbstractListModel{parent}
{ }

void LyricsSourcesModel::setup(const std::vector<LyricSource*>& sources)
{
    beginResetModel();

    m_sources.clear();

    for(const auto* source : sources) {
        m_sources.emplace_back(source->name(), source->index(), source->enabled());
    }

    endResetModel();
}

std::vector<LyricSourceItem> LyricsSourcesModel::sources() const
{
    return m_sources;
}

Qt::ItemFlags LyricsSourcesModel::flags(const QModelIndex& index) const
{
    auto flags = QAbstractListModel::flags(index);
    if(index.isValid()) {
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable;
    }
    else {
        flags |= Qt::ItemIsDropEnabled;
    }
    return flags;
}

int LyricsSourcesModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_sources.size());
}

QVariant LyricsSourcesModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    if(index.row() < 0 || std::cmp_greater_equal(index.row(), m_sources.size())) {
        return {};
    }

    const auto& source = m_sources.at(index.row());

    switch(role) {
        case(Qt::DisplayRole):
            return source.name;
        case(Qt::CheckStateRole):
            return source.enabled ? Qt::Checked : Qt::Unchecked;
        default:
            return {};
    }
}

bool LyricsSourcesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    if(index.row() < 0 || std::cmp_greater_equal(index.row(), m_sources.size())) {
        return false;
    }

    auto& source = m_sources.at(index.row());

    if(role == Qt::CheckStateRole) {
        const bool isChecked = (value.value<Qt::CheckState>() == Qt::Checked);
        if(std::exchange(source.enabled, isChecked) != isChecked) {
            emit dataChanged(index, index, {role});
            return true;
        }
    }

    return false;
}

QStringList LyricsSourcesModel::mimeTypes() const
{
    return {QString::fromLatin1(LyricItems)};
}

QMimeData* LyricsSourcesModel::mimeData(const QModelIndexList& indexes) const
{
    QStringList selected;
    for(const QModelIndex& index : indexes) {
        selected.emplace_back(index.data(Qt::DisplayRole).toString());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream << selected;

    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(LyricItems), data);
    return mimeData;
}

bool LyricsSourcesModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                         const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(LyricItems))) {
        return true;
    }

    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

Qt::DropActions LyricsSourcesModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions LyricsSourcesModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

bool LyricsSourcesModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                      const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if(row < 0) {
        row = static_cast<int>(m_sources.size());
    }

    if(data->hasFormat(QString::fromLatin1(LyricItems))) {
        QByteArray decoderData = data->data(QString::fromLatin1(LyricItems));
        QStringList selected;
        QDataStream stream(&decoderData, QIODevice::ReadOnly);
        stream >> selected;

        beginResetModel();

        for(const QString& sourceName : std::as_const(selected)) {
            auto sourceIt = std::ranges::find_if(
                m_sources, [&sourceName](const auto& source) { return source.name == sourceName; });

            if(sourceIt != m_sources.cend()) {
                const int currentIndex = static_cast<int>(std::distance(m_sources.begin(), sourceIt));
                const int newIndex     = row > currentIndex ? row - 1 : row;
                Utils::move(m_sources, currentIndex, newIndex);
                ++row;
            }
        }

        for(int i{0}; auto& source : m_sources) {
            source.index = i++;
        }

        endResetModel();
    }

    return true;
}
} // namespace Fooyin::Lyrics
