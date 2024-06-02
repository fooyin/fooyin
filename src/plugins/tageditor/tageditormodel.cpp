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

#include "tageditormodel.h"

#include "tageditoritem.h"

#include <core/constants.h>
#include <core/scripting/scriptregistry.h>
#include <gui/trackselectioncontroller.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/starrating.h>

namespace Fooyin::TagEditor {
using TagFieldMap = std::unordered_map<QString, TagEditorItem>;

struct EditorPair
{
    QString displayName;
    std::function<QString(const Track&)> metadata;
};

struct TagEditorModel::Private
{
    TagEditorModel* self;

    SettingsManager* settings;
    ScriptRegistry scriptRegistry;

    TrackList tracks;

    // TODO: Make fields shown configurable
    const std::vector<EditorPair> fields{
        {QStringLiteral("Artist Name"), &Track::artist},
        {QStringLiteral("Track Title"), &Track::title},
        {QStringLiteral("Album Title"), &Track::album},
        {QStringLiteral("Date"), &Track::date},
        {QStringLiteral("Genre"), &Track::genre},
        {QStringLiteral("Composer"), &Track::composer},
        {QStringLiteral("Performer"), &Track::performer},
        {QStringLiteral("Album Artist"), &Track::albumArtist},
        {QStringLiteral("Track Number"),
         [](const Track& track) {
             return track.metaValue(QString::fromLatin1(Constants::MetaData::Track));
         }},
        {QStringLiteral("Total Tracks"),
         [](const Track& track) {
             return track.metaValue(QString::fromLatin1(Constants::MetaData::TrackTotal));
         }},
        {QStringLiteral("Disc Number"),
         [](const Track& track) {
             return track.metaValue(QString::fromLatin1(Constants::MetaData::Disc));
         }},
        {QStringLiteral("Total Discs"),
         [](const Track& track) {
             return track.metaValue(QString::fromLatin1(Constants::MetaData::DiscTotal));
         }},
        {QStringLiteral("Comment"), &Track::comment},
        {QStringLiteral("Rating"), [](const Track& track) {
             return track.metaValue(QString::fromLatin1(Constants::MetaData::Rating));
         }}};

    const std::unordered_map<QString, QString> setFields{
        {QStringLiteral("Artist Name"), QString::fromLatin1(Constants::MetaData::Artist)},
        {QStringLiteral("Track Title"), QString::fromLatin1(Constants::MetaData::Title)},
        {QStringLiteral("Album Title"), QString::fromLatin1(Constants::MetaData::Album)},
        {QStringLiteral("Date"), QString::fromLatin1(Constants::MetaData::Date)},
        {QStringLiteral("Genre"), QString::fromLatin1(Constants::MetaData::Genre)},
        {QStringLiteral("Composer"), QString::fromLatin1(Constants::MetaData::Composer)},
        {QStringLiteral("Performer"), QString::fromLatin1(Constants::MetaData::Performer)},
        {QStringLiteral("Album Artist"), QString::fromLatin1(Constants::MetaData::AlbumArtist)},
        {QStringLiteral("Track Number"), QString::fromLatin1(Constants::MetaData::Track)},
        {QStringLiteral("Total Tracks"), QString::fromLatin1(Constants::MetaData::TrackTotal)},
        {QStringLiteral("Disc Number"), QString::fromLatin1(Constants::MetaData::Disc)},
        {QStringLiteral("Total Discs"), QString::fromLatin1(Constants::MetaData::DiscTotal)},
        {QStringLiteral("Comment"), QString::fromLatin1(Constants::MetaData::Comment)},
        {QStringLiteral("Rating"), QString::fromLatin1(Constants::MetaData::Rating)}};

    TagEditorItem root;
    TagFieldMap tags;
    TagFieldMap customTags;

    explicit Private(TagEditorModel* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
    { }

    bool isDefaultField(const QString& name) const
    {
        return std::ranges::any_of(std::as_const(fields),
                                   [name](const EditorPair& pair) { return pair.displayName == name; });
    }

    QString findSetField(const QString& name)
    {
        if(!setFields.contains(name)) {
            return {};
        }
        return setFields.at(name);
    }

    void reset()
    {
        root = {};
        tags.clear();
        customTags.clear();
    }

    void updateFields()
    {
        for(const Track& track : tracks) {
            const auto trackTags = track.extraTags();

            for(const auto& [field, values] : Utils::asRange(trackTags)) {
                if(values.empty()) {
                    continue;
                }

                if(!customTags.contains(field)) {
                    auto* item = &customTags.emplace(field, TagEditorItem{field, &root, false}).first->second;
                    root.appendChild(item);
                }

                auto* fieldItem = &customTags.at(field);
                fieldItem->addTrackValue(values);
            }
        }

        updateEditorValues();
    }

    void updateEditorValues()
    {
        if(tracks.empty()) {
            return;
        }

        for(const auto& track : tracks) {
            for(const auto& [field, var] : fields) {
                const auto result = var(track);
                if(result.contains(u"\037")) {
                    tags[field].addTrackValue(result.split(QStringLiteral("\037")));
                }
                else {
                    tags[field].addTrackValue(result);
                }
            }

            for(auto& [field, node] : customTags) {
                const auto result = scriptRegistry.value(field, track);
                if(!result.cond) {
                    node.addTrack();
                }
            }
        }
    }

    void updateTrackMetadata(const QString& name, const QVariant& value)
    {
        if(tracks.empty()) {
            return;
        }

        const QString metadata = findSetField(name);
        const bool isList      = (metadata == QLatin1String{Constants::MetaData::AlbumArtist}
                             || metadata == QLatin1String{Constants::MetaData::Artist}
                             || metadata == QLatin1String{Constants::MetaData::Genre});
        const bool isNumeric   = (metadata == QLatin1String{Constants::MetaData::Track}
                                || metadata == QLatin1String{Constants::MetaData::TrackTotal}
                                || metadata == QLatin1String{Constants::MetaData::Disc}
                                || metadata == QLatin1String{Constants::MetaData::DiscTotal}
                                || metadata == QLatin1String{Constants::MetaData::Rating});

        QStringList listValue;
        int intValue{-1};
        bool intValueIsValid{false};

        if(isList) {
            listValue = value.toString().split(QStringLiteral("; "));
        }
        else if(isNumeric) {
            intValue = value.toInt(&intValueIsValid);
            if(!intValueIsValid) {
                intValue = -1;
            }
        }

        for(Track& track : tracks) {
            if(isList) {
                scriptRegistry.setValue(metadata, listValue, track);
            }
            else if(isNumeric) {
                scriptRegistry.setValue(metadata, intValue, track);
            }
            else {
                scriptRegistry.setValue(metadata, value.toString(), track);
            }
        }
    }

    void addCustomTrackMetadata(const QString& name, const QString& value)
    {
        if(tracks.empty()) {
            return;
        }

        for(Track& track : tracks) {
            track.addExtraTag(name, value);
        }
    }

    void replaceCustomTrackMetadata(const QString& name, const QString& value)
    {
        if(tracks.empty()) {
            return;
        }

        for(Track& track : tracks) {
            track.replaceExtraTag(name, value);
        }
    }

    void removeCustomTrackMetadata(const QString& name)
    {
        if(tracks.empty()) {
            return;
        }

        for(Track& track : tracks) {
            track.removeExtraTag(name);
        }
    }
};

TagEditorModel::TagEditorModel(SettingsManager* settings, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

TagEditorModel::~TagEditorModel() = default;

void TagEditorModel::reset(const TrackList& tracks)
{
    beginResetModel();
    p->reset();
    p->tracks = tracks;

    for(const auto& [field, _] : p->fields) {
        auto* item = &p->tags.emplace(field, TagEditorItem{field, &p->root, true}).first->second;
        p->root.appendChild(item);
    }

    p->updateFields();
    p->root.sortCustomTags();

    endResetModel();
}

bool TagEditorModel::processQueue()
{
    bool nodeChanged{false};

    const auto updateChangedNodes = [this, &nodeChanged](TagFieldMap& tags, bool custom) {
        QStringList fieldsToRemove;

        for(auto& [index, node] : tags) {
            const TagEditorItem::ItemStatus status = node.status();

            switch(status) {
                case(TagEditorItem::Added): {
                    if(custom) {
                        p->addCustomTrackMetadata(node.name(), node.value());
                    }
                    else {
                        p->updateTrackMetadata(node.name(), node.value());
                    }

                    node.setStatus(TagEditorItem::None);
                    emit dataChanged({}, {}, {Qt::FontRole});

                    nodeChanged = true;
                    break;
                }
                case(TagEditorItem::Removed): {
                    p->removeCustomTrackMetadata(node.name());

                    beginRemoveRows({}, node.row(), node.row());
                    p->root.removeChild(node.row());
                    endRemoveRows();
                    fieldsToRemove.push_back(node.name());

                    nodeChanged = true;
                    break;
                }
                case(TagEditorItem::Changed): {
                    if(custom) {
                        const auto fieldIt
                            = std::ranges::find_if(std::as_const(p->customTags), [node](const auto& pair) {
                                  return pair.second.name() == node.name();
                              });
                        if(fieldIt != p->customTags.end()) {
                            auto tagItem = p->customTags.extract(fieldIt);

                            p->removeCustomTrackMetadata(tagItem.key());

                            tagItem.key() = node.name();
                            p->customTags.insert(std::move(tagItem));

                            p->replaceCustomTrackMetadata(node.name(), node.value());
                        }
                    }
                    else {
                        p->updateTrackMetadata(node.name(), node.value());
                    }

                    node.setStatus(TagEditorItem::None);
                    emit dataChanged({}, {}, {Qt::FontRole});

                    nodeChanged = true;
                    break;
                }
                case(TagEditorItem::None):
                    break;
            }
        }

        for(const auto& field : fieldsToRemove) {
            tags.erase(field);
        }
    };

    updateChangedNodes(p->tags, false);
    updateChangedNodes(p->customTags, true);

    if(nodeChanged) {
        emit trackMetadataChanged(p->tracks);
    }

    return nodeChanged;
}

QVariant TagEditorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignCenter);
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

Qt::ItemFlags TagEditorModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);

    if((index.column() != 0 || !index.data(TagEditorItem::IsDefault).toBool())) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant TagEditorModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == TagEditorItem::IsDefault) {
        return item->isDefault();
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        if(index.column() == 0) {
            if(role == Qt::EditRole) {
                return item->name();
            }
            if(!item->isDefault()) {
                const QString name = QStringLiteral("<") + item->name() + QStringLiteral(">");
                return name;
            }

            return item->name();
        }

        if(index.row() == 13) {
            return QVariant::fromValue(StarRating{item->value().toInt(), 5});
        }

        return item->value();
    }

    return {};
}

bool TagEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(p->tracks.empty()) {
        return false;
    }

    if(role != Qt::EditRole) {
        return false;
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());

    switch(index.column()) {
        case(0): {
            if(value == item->name() || p->customTags.contains(value.toString())) {
                if(item->status() == TagEditorItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }

            const QString name = value.toString().toUpper();
            item->setTitle(name);
            break;
        }
        case(1): {
            QString setValue = value.toString();

            if(index.row() == 13) {
                const auto rating = value.value<StarRating>();
                setValue          = QString::number(rating.starCount());
            }

            if(setValue.simplified() == item->value().simplified()) {
                return false;
            }

            item->setValue({setValue});
            break;
        }
        default:
            break;
    }

    if(item->status() != TagEditorItem::Added) {
        item->setStatus(TagEditorItem::Changed);
    }

    emit dataChanged(index, index);

    return true;
}

QModelIndex TagEditorModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    TagEditorItem* item = p->root.child(row);

    return createIndex(row, column, item);
}

int TagEditorModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->root.childCount();
}

int TagEditorModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

bool TagEditorModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        if(index.data(TagEditorItem::IsDefault).toBool()) {
            setData(index.siblingAtColumn(1), {}, Qt::EditRole);
            continue;
        }

        auto* item = static_cast<TagEditorItem*>(index.internalPointer());
        if(item) {
            if(item->status() == TagEditorItem::Added) {
                beginRemoveRows({}, i, i);
                p->root.removeChild(i);
                endRemoveRows();
                p->customTags.erase(defaultFieldText() + QString::number(row));
            }
            else {
                item->setStatus(TagEditorItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }

    return true;
}

QString TagEditorModel::defaultFieldText()
{
    return QStringLiteral("<input field name>");
}

void TagEditorModel::addPendingRow()
{
    TagEditorItem newItem{defaultFieldText(), &p->root, false};
    newItem.setStatus(TagEditorItem::Added);

    const int row = p->root.childCount();
    auto* item    = &p->customTags.emplace(defaultFieldText() + QString::number(row), newItem).first->second;

    beginInsertRows({}, row, row);
    p->root.appendChild(item);
    endInsertRows();
}

void TagEditorModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    p->root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin::TagEditor

#include "moc_tageditormodel.cpp"
