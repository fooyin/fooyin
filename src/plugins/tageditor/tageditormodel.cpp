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

namespace Fy::TagEditor {
struct EditorPair
{
    QString displayName;
    QString metadata;
};

struct TagEditorModel::Private
{
    Gui::TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;
    Core::Scripting::Registry scriptRegistry;

    std::vector<EditorPair> fields{
        {"Artist Name", Core::Constants::MetaData::Artist},  {"Track Title", Core::Constants::MetaData::Title},
        {"Album Title", Core::Constants::MetaData::Album},   {"Date", Core::Constants::MetaData::Date},
        {"Genre", Core::Constants::MetaData::Genre},         {"Composer", Core::Constants::MetaData::Composer},
        {"Performer", Core::Constants::MetaData::Performer}, {"Album Artist", Core::Constants::MetaData::AlbumArtist},
        {"Track Number", Core::Constants::MetaData::Track},  {"Total Tracks", Core::Constants::MetaData::TrackTotal},
        {"Disc Number", Core::Constants::MetaData::Disc},    {"Total Discs", Core::Constants::MetaData::DiscTotal},
        {"Comment", Core::Constants::MetaData::Comment}};
    std::unordered_map<QString, TagEditorItem> tags;

    explicit Private(Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings)
        : trackSelection{trackSelection}
        , settings{settings}
    { }

    QString findField(const QString& name)
    {
        const auto field = std::ranges::find_if(std::as_const(fields),
                                                [name](const EditorPair& pair) { return pair.displayName == name; });
        if(field == fields.cend()) {
            return {};
        }
        return field->metadata;
    }

    void removeField(const QString& name)
    {
        if(tags.contains(name)) {
            tags.erase(name);
        }
    }

    void updateTrackMetadata(const QString& name, const QVariant& value, Core::TrackList& tracks)
    {
        if(tracks.empty()) {
            return;
        }

        const QString metadata = findField(name);

        for(Core::Track& track : tracks) {
            if(metadata == Core::Constants::MetaData::Artist || metadata == Core::Constants::MetaData::Genre) {
                scriptRegistry.setVar(metadata, value.toString().split(QStringLiteral("; ")), track);
            }
            else if(metadata == Core::Constants::MetaData::Track || metadata == Core::Constants::MetaData::TrackTotal
                    || metadata == Core::Constants::MetaData::Disc
                    || metadata == Core::Constants::MetaData::DiscTotal) {
                scriptRegistry.setVar(metadata, value.toInt(), track);
            }
            else {
                scriptRegistry.setVar(metadata, value.toString(), track);
            }
        }
        // TODO: Set custom tags
    }
};

TagEditorModel::TagEditorModel(Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                               QObject* parent)
    : TableModel{parent}
    , p{std::make_unique<Private>(trackSelection, settings)}
{
    populate();
    updateEditorValues();

    QObject::connect(p->trackSelection, &Gui::TrackSelectionController::selectionChanged, this,
                     &TagEditorModel::updateEditorValues);
}

void TagEditorModel::populate()
{
    for(const auto& [field, _] : p->fields) {
        auto* item = &p->tags.emplace(field, TagEditorItem{field}).first->second;
        rootItem()->appendChild(item);
    }
}

void TagEditorModel::addNewRow()
{
    //    TagEditorItem newItem{"<input field name>", rootItem()};
    //    newItem.setStatus(TagEditorItem::Added);
    //    auto* item = &p->tags.emplace("<input field name>", newItem).first->second;

    //    const int row = rootItem()->childCount();
    //    beginInsertRows({}, row, row);
    //    rootItem()->appendChild(item);
    //    endInsertRows();
}

void TagEditorModel::processQueue()
{
    Core::TrackList tracks = p->trackSelection->selectedTracks();
    std::vector<QString> fieldsToRemove;

    auto tagsToCheck
        = p->tags | std::views::filter([](const auto& tag) { return tag.second.status() != TagEditorItem::None; });

    for(auto& [index, node] : tagsToCheck) {
        const TagEditorItem::ItemStatus status = node.status();

        switch(status) {
            case(TagEditorItem::Added): {
                p->updateTrackMetadata(node.name(), node.value(), tracks);
                node.setStatus(TagEditorItem::None);
                emit dataChanged({}, {}, {Qt::FontRole});
                break;
            }
            case(TagEditorItem::Removed): {
                beginRemoveRows({}, node.row(), node.row());
                rootItem()->removeChild(node.row());
                endRemoveRows();
                fieldsToRemove.push_back(node.name());
                break;
            }
            case(TagEditorItem::Changed): {
                p->updateTrackMetadata(node.name(), node.value(), tracks);
                node.setStatus(TagEditorItem::None);
                emit dataChanged({}, {}, {Qt::FontRole});
                break;
            }
            case(TagEditorItem::None):
                break;
        }
    }
    for(const auto& field : fieldsToRemove) {
        p->removeField(field);
    }

    if(!tagsToCheck.empty()) {
        emit trackMetadataChanged(tracks);
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
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->name();
        case(1):
            return item->value();
        default:
            return {};
    }
}

bool TagEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!p->trackSelection->hasTracks()) {
        return false;
    }

    if(role != Qt::EditRole) {
        return false;
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());

    if(value == item->value()) {
        return false;
    }

    item->setValue(value.toStringList());
    item->setStatus(TagEditorItem::Changed);

    emit dataChanged(index, index);

    return true;
}

Qt::ItemFlags TagEditorModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);

    if(index.column() != 0) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

void TagEditorModel::updateEditorValues()
{
    auto tracks = p->trackSelection->selectedTracks();

    if(tracks.empty()) {
        return;
    }

    rootItem()->reset();

    int count{0};
    for(const auto& track : tracks) {
        if(count > 40) {
            break;
        }
        for(const auto& [field, var] : p->fields) {
            p->scriptRegistry.changeCurrentTrack(track);
            const auto result = p->scriptRegistry.varValue(var);
            if(result.cond) {
                if(result.value.contains(Core::Constants::Separator)) {
                    p->tags[field].addTrackValue(result.value.split(Core::Constants::Separator));
                }
                else {
                    p->tags[field].addTrackValue(result.value);
                }
            }
            else {
                p->tags[field].addTrackValue(QStringLiteral(""));
            }
        }
        ++count;
    }
    emit dataChanged({}, {}, {Qt::DisplayRole});
    // TODO: Return custom tags
}
} // namespace Fy::TagEditor
