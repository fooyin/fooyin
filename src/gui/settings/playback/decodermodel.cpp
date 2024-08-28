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

#include "decodermodel.h"

#include <utils/helpers.h>

#include <QIODevice>
#include <QMimeData>

constexpr auto LoaderItems = "application/x-fooyin-loaderitems";

namespace Fooyin {
DecoderModel::DecoderModel(QObject* parent)
    : QAbstractListModel{parent}
{ }

void DecoderModel::setup(const LoaderVariant& loaders)
{
    beginResetModel();

    m_loaders = loaders;
    std::visit(
        [](auto& currLoaders) {
            // Archive loader is a special case which shouldn't be disabled
            std::erase_if(currLoaders, [](const auto& loader) { return loader.name == u"Archive"; });
        },
        m_loaders);

    endResetModel();
}

DecoderModel::LoaderVariant DecoderModel::loaders() const
{
    return m_loaders;
}

Qt::ItemFlags DecoderModel::flags(const QModelIndex& index) const
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

int DecoderModel::rowCount(const QModelIndex& /*parent*/) const
{
    return std::visit([](const auto& loaders) { return static_cast<int>(loaders.size()); }, m_loaders);
}

QVariant DecoderModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto getDataForRole = [&](const auto& loaders) -> QVariant {
        if(index.row() < 0 || index.row() >= static_cast<int>(loaders.size())) {
            return {};
        }

        const auto& loader = loaders.at(index.row());

        switch(role) {
            case(Qt::DisplayRole):
                return loader.name;
            case(Qt::ToolTipRole):
                return QStringLiteral("%1: %2").arg(tr("Supported extensions")).arg(loader.extensions.join(u", "));
            case(Qt::CheckStateRole):
                return loader.enabled ? Qt::Checked : Qt::Unchecked;
            default:
                return {};
        }
    };

    return std::visit(getDataForRole, m_loaders);
}

bool DecoderModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    auto setDataForRole = [&](auto& loaders) -> bool {
        if(index.row() < 0 || index.row() >= static_cast<int>(loaders.size())) {
            return false;
        }

        auto& loader = loaders.at(index.row());

        if(role == Qt::CheckStateRole) {
            loader.enabled = (value.value<Qt::CheckState>() == Qt::Checked);
            emit dataChanged(index, index, {role});
            return true;
        }

        return false;
    };

    return std::visit(setDataForRole, m_loaders);
}

QStringList DecoderModel::mimeTypes() const
{
    return {QString::fromLatin1(LoaderItems)};
}

QMimeData* DecoderModel::mimeData(const QModelIndexList& indexes) const
{
    QStringList selected;
    for(const QModelIndex& index : indexes) {
        selected.emplace_back(index.data(Qt::DisplayRole).toString());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream << selected;

    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(LoaderItems), data);
    return mimeData;
}

bool DecoderModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                   const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(LoaderItems))) {
        return true;
    }

    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

bool DecoderModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if(row < 0) {
        row = std::visit([](const auto& loaders) { return static_cast<int>(loaders.size()); }, m_loaders);
    }

    if(data->hasFormat(QString::fromLatin1(LoaderItems))) {
        QByteArray decoderData = data->data(QString::fromLatin1(LoaderItems));
        QStringList selected;
        QDataStream stream(&decoderData, QIODevice::ReadOnly);
        stream >> selected;

        beginResetModel();

        auto handleDrop = [&selected, &row](auto& loaders) {
            for(const QString& loaderName : selected) {
                auto loaderIt = std::find_if(loaders.begin(), loaders.end(),
                                             [&](const auto& loader) { return loader.name == loaderName; });

                if(loaderIt != loaders.end()) {
                    const int currentIndex = static_cast<int>(std::distance(loaders.begin(), loaderIt));
                    const int newIndex     = row > currentIndex ? row - 1 : row;
                    Utils::move(loaders, currentIndex, newIndex);
                    ++row;
                }
            }

            for(int i{0}; auto& loader : loaders) {
                loader.index = i++;
            }
        };

        std::visit(handleDrop, m_loaders);

        endResetModel();
    }

    return true;
}

Qt::DropActions DecoderModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions DecoderModel::supportedDragActions() const
{
    return Qt::MoveAction;
}
} // namespace Fooyin
