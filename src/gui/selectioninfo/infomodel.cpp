/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "infomodel.h"

#include <core/track.h>

#include <QFileInfo>
#include <QFont>
#include <QThread>

constexpr auto HeaderFontDelta = 2;

namespace Fooyin {
InfoModel::InfoModel(LibraryManager* libraryManager, QObject* parent)
    : TreeModel{parent}
    , m_populator{libraryManager}
{
    m_populator.moveToThread(&m_populatorThread);

    m_headerFont.setPointSize(m_headerFont.pointSize() + HeaderFontDelta);
    m_headerFont.setBold(true);

    QObject::connect(&m_populator, &InfoPopulator::populated, this, &InfoModel::populate);
}

Qt::ItemFlags InfoModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TreeModel::flags(index);

    if(index.data(InfoItem::Type).toInt() == InfoItem::Entry) {
        flags |= Qt::ItemNeverHasChildren;
    }

    return flags;
}

InfoModel::~InfoModel()
{
    m_populatorThread.quit();
    m_populatorThread.wait();
}

QVariant InfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return QStringLiteral("Name");
        case(1):
            return QStringLiteral("Value");
        default:
            break;
    }

    return {};
}

int InfoModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

bool InfoModel::hasChildren(const QModelIndex& parent) const
{
    return !parent.isValid() || itemForIndex(parent)->childCount() > 0;
}

QVariant InfoModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item              = itemForIndex(index);
    const InfoItem::ItemType type = item->type();

    if(role == InfoItem::Type) {
        return QVariant::fromValue<InfoItem::ItemType>(type);
    }

    if(role == Qt::FontRole) {
        if(type == InfoItem::Header) {
            return m_headerFont;
        }
        return {};
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case(0):
            return item->name();
        case(1):
            return item->value();
        default:
            break;
    }

    return {};
}

InfoItem::Options InfoModel::options() const
{
    return m_options;
}

void InfoModel::setOption(InfoItem::Option option, bool enabled)
{
    if(enabled) {
        m_options |= option;
    }
    else {
        m_options &= ~option;
    }
}

void InfoModel::setOptions(InfoItem::Options options)
{
    m_options = options;
}

void InfoModel::resetModel(const TrackList& tracks)
{
    if(m_populatorThread.isRunning()) {
        m_populator.stopThread();
    }
    else {
        m_populatorThread.start();
    }

    QMetaObject::invokeMethod(&m_populator, [this, tracks] { m_populator.run(m_options, tracks); });
}

void InfoModel::populate(const InfoData& data)
{
    beginResetModel();
    resetRoot();
    m_nodes.clear();

    m_nodes = data.nodes;

    for(const auto& [parentKey, children] : data.parents) {
        InfoItem* parent{nullptr};

        if(parentKey == u"Root") {
            parent = rootItem();
        }
        else if(m_nodes.contains(parentKey)) {
            parent = &m_nodes.at(parentKey);
        }

        if(parent) {
            for(const auto& child : children) {
                if(m_nodes.contains(child)) {
                    parent->appendChild(&m_nodes.at(child));
                }
            }
        }
    }

    rootItem()->sortChildren();

    endResetModel();
}
} // namespace Fooyin

#include "moc_infomodel.cpp"
