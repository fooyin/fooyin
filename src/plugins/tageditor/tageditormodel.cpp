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

#include "tageditormodel.h"

#include "tageditoritem.h"

#include <core/constants.h>
#include <core/scripting/scriptregistry.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

constexpr auto DefaultFieldText = "<input field name>";

namespace Fooyin::TagEditor {
using TagFieldMap = std::unordered_map<QString, TagEditorItem>;

struct EditorPair
{
    QString displayName;
    QString metadata;
};

struct TagEditorModel::Private
{
    TagEditorModel* self;

    SettingsManager* settings;
    ScriptRegistry scriptRegistry;

    TrackList tracks;

    std::vector<EditorPair> fields{
        {"Artist Name", Constants::MetaData::Artist},  {"Track Title", Constants::MetaData::Title},
        {"Album Title", Constants::MetaData::Album},   {"Date", Constants::MetaData::Date},
        {"Genre", Constants::MetaData::Genre},         {"Composer", Constants::MetaData::Composer},
        {"Performer", Constants::MetaData::Performer}, {"Album Artist", Constants::MetaData::AlbumArtist},
        {"Track Number", Constants::MetaData::Track},  {"Total Tracks", Constants::MetaData::TrackTotal},
        {"Disc Number", Constants::MetaData::Disc},    {"Total Discs", Constants::MetaData::DiscTotal},
        {"Comment", Constants::MetaData::Comment}};
    TagFieldMap tags;
    TagFieldMap customTags;

    explicit Private(TagEditorModel* self, SettingsManager* settings)
        : self{self}
        , settings{settings}
    { }

    bool isDefaultField(const QString& name) const
    {
        return std::ranges::any_of(std::as_const(fields),
                                   [name](const EditorPair& pair) { return pair.displayName == name; });
    }

    QString findField(const QString& name)
    {
        const auto field = std::ranges::find_if(std::as_const(fields),
                                                [name](const EditorPair& pair) { return pair.displayName == name; });
        if(field == fields.cend()) {
            return {};
        }
        return field->metadata;
    }

    void reset()
    {
        tags.clear();
        customTags.clear();
    }

    void updateFields()
    {
        for(const Track& track : tracks) {
            const auto trackTags = track.extraTags();

            for(const auto& [field, values] : trackTags) {
                if(values.empty()) {
                    continue;
                }

                if(!customTags.contains(field)) {
                    auto* item
                        = &customTags.emplace(field, TagEditorItem{field, self->rootItem(), false}).first->second;
                    self->rootItem()->appendChild(item);
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

        for(int count{0}; const auto& track : tracks) {
            if(count > 40) {
                break;
            }

            for(const auto& [field, var] : fields) {
                scriptRegistry.changeCurrentTrack(track);
                const auto result = scriptRegistry.varValue(var);
                if(result.cond) {
                    if(result.value.contains(Constants::Separator)) {
                        tags[field].addTrackValue(result.value.split(Constants::Separator));
                    }
                    else {
                        tags[field].addTrackValue(result.value);
                    }
                }
                else {
                    tags[field].addTrackValue(QStringLiteral(""));
                }
            }
            ++count;
        }
    }

    void updateTrackMetadata(const QString& name, const QVariant& value)
    {
        if(tracks.empty()) {
            return;
        }

        const QString metadata = findField(name);

        for(Track& track : tracks) {
            if(metadata == Constants::MetaData::Artist || metadata == Constants::MetaData::Genre) {
                scriptRegistry.setVar(metadata, value.toString().split(QStringLiteral("; ")), track);
            }
            else if(metadata == Constants::MetaData::Track || metadata == Constants::MetaData::TrackTotal
                    || metadata == Constants::MetaData::Disc || metadata == Constants::MetaData::DiscTotal) {
                scriptRegistry.setVar(metadata, value.toInt(), track);
            }
            else {
                scriptRegistry.setVar(metadata, value.toString(), track);
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
    : TableModel{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

void TagEditorModel::reset(const TrackList& tracks)
{
    beginResetModel();
    resetRoot();
    p->reset();
    p->tracks = tracks;

    for(const auto& [field, _] : p->fields) {
        auto* item = &p->tags.emplace(field, TagEditorItem{field, rootItem(), true}).first->second;
        rootItem()->appendChild(item);
    }

    p->updateFields();

    endResetModel();
}

void TagEditorModel::addNewRow()
{
    TagEditorItem newItem{DefaultFieldText, rootItem(), false};
    newItem.setStatus(TagEditorItem::Added);
    auto* item = &p->customTags.emplace(DefaultFieldText, newItem).first->second;

    const int row = rootItem()->childCount();
    beginInsertRows({}, row, row);
    rootItem()->appendChild(item);
    endInsertRows();

    emit newPendingRow();
}

void TagEditorModel::removeRow(int row)
{
    const QModelIndex& index = this->index(row, 0, {});

    if(!index.isValid()) {
        return;
    }

    if(index.data(TagEditorItem::IsDefault).toBool()) {
        return;
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());
    if(item) {
        if(item->status() == TagEditorItem::Added) {
            beginRemoveRows({}, row, row);
            rootItem()->removeChild(row);
            endRemoveRows();
            p->customTags.erase(item->name());
        }
        else {
            item->setStatus(TagEditorItem::Removed);
            emit dataChanged(index, index, {Qt::FontRole});
        }
    }
}

void TagEditorModel::processQueue()
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
                    rootItem()->removeChild(node.row());
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
}

QVariant TagEditorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Name";
        case(1):
            return "Value";
        default:
            return {};
    }
}

TagEditorModel::~TagEditorModel() = default;

int TagEditorModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

QVariant TagEditorModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole && role != TagEditorItem::IsDefault) {
        return {};
    }

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

    switch(index.column()) {
        case(0):
            if(role == Qt::EditRole) {
                return item->name();
            }
            if(!item->isDefault()) {
                const QString name = QStringLiteral("<") + item->name() + QStringLiteral(">");
                return name;
            }
            return item->name();
        case(1):
            return item->value();
        default:
            return {};
    }
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

            emit pendingRowAdded();
            break;
        }
        case(1):
            if(value == item->value()) {
                return false;
            }
            item->setValue(value.toStringList());
            break;
        default:
            break;
    }

    if(item->status() != TagEditorItem::Added) {
        item->setStatus(TagEditorItem::Changed);
    }

    emit dataChanged(index, index);

    return true;
}

Qt::ItemFlags TagEditorModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);

    if(index.column() != 0 || !index.data(TagEditorItem::IsDefault).toBool()) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QString TagEditorModel::defaultFieldText() const
{
    return DefaultFieldText;
}

void TagEditorModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    rootItem()->removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin::TagEditor

#include "moc_tageditormodel.cpp"
