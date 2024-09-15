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
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/starrating.h>

constexpr auto MultipleValuesPrefix = "<<multiple values>>";

namespace Fooyin::TagEditor {
using TagFieldMap = std::unordered_map<QString, TagEditorItem>;

class TagEditorModelPrivate
{
public:
    explicit TagEditorModelPrivate(TagEditorModel* self, SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
    { }

    bool hasTagConflict(TagEditorItem* item, const QString& title) const;
    bool hasDefaultField(const QString& field) const;
    bool isDefaultField(const QString& name) const;

    void reset();

    void updateFields();
    bool updateTrackMetadata(const TagEditorField& field, const QVariant& value, bool split = false);

    TagEditorModel* m_self;

    SettingsManager* m_settings;
    ScriptRegistry m_scriptRegistry;

    TrackList m_tracks;

    QString m_defaultFieldtext{QStringLiteral("<input field name>")};
    std::vector<TagEditorField> m_fields;
    int m_ratingRow{-1};

    TagEditorItem m_root;
    TagFieldMap m_tags;
};

bool TagEditorModelPrivate::hasTagConflict(TagEditorItem* item, const QString& title) const
{
    for(const auto& [key, tag] : m_tags) {
        if(item != &tag && (title == key || tag.title() == title || tag.changedTitle() == title)) {
            return true;
        }
    }
    return false;
}

bool TagEditorModelPrivate::hasDefaultField(const QString& field) const
{
    const QString fieldToFind = field.toUpper();
    return std::ranges::any_of(std::as_const(m_fields), [fieldToFind](const auto& editorField) {
        return editorField.scriptField.compare(fieldToFind, Qt::CaseInsensitive) == 0;
    });
}

bool TagEditorModelPrivate::isDefaultField(const QString& name) const
{
    return std::ranges::any_of(std::as_const(m_fields), [name](const auto& field) { return field.name == name; });
}

void TagEditorModelPrivate::reset()
{
    m_root = {};
    m_tags.clear();
}

void TagEditorModelPrivate::updateFields()
{
    const auto iterateTags = [this](const auto& tags, const bool isDefault = false) {
        for(const auto& [field, value] : Utils::asRange(tags)) {
            if(value.isEmpty() || hasDefaultField(field)) {
                continue;
            }

            if(!m_tags.contains(field)) {
                TagEditorField editorField;
                editorField.name        = field;
                editorField.scriptField = field;
                editorField.isDefault   = isDefault;
                auto* item              = &m_tags.emplace(field, TagEditorItem{editorField, &m_root}).first->second;
                m_root.appendChild(item);
            }
            auto& node = m_tags.at(field);
            node.addTrackValue(value);
        }
    };

    for(const Track& track : m_tracks) {
        for(auto& [field, node] : m_tags) {
            const auto result = track.metaValue(node.field().scriptField);
            node.addTrackValue(result.split(QLatin1String{Constants::UnitSeparator}, Qt::SkipEmptyParts));
        }

        iterateTags(track.metadata(), true);
        iterateTags(track.extraTags());
    }
}

bool TagEditorModelPrivate::updateTrackMetadata(const TagEditorField& field, const QVariant& value, bool split)
{
    if(m_tracks.empty() || field.scriptField.isEmpty()) {
        return false;
    }

    const bool isList  = split || field.multivalue;
    const bool isFloat = (field.scriptField == QLatin1String{Constants::MetaData::RatingEditor});

    QString tag{field.scriptField};
    if(tag == QLatin1String{Constants::MetaData::RatingEditor}) {
        tag = QLatin1String{Constants::MetaData::Rating};
    }

    QStringList listValue;
    float floatValue{-1};

    if(isList) {
        listValue = value.toString().split(QStringLiteral(";"), Qt::SkipEmptyParts);
        std::ranges::transform(listValue, listValue.begin(), [](const QString& val) { return val.trimmed(); });
    }
    else if(isFloat) {
        bool validFloat{false};
        floatValue = value.toFloat(&validFloat);
        if(!validFloat) {
            floatValue = -1;
        }
    }

    for(int i{0}; Track & track : m_tracks) {
        if(track.hasCue()) {
            continue;
        }

        if(split && std::cmp_less(i, listValue.size())) {
            if(isFloat) {
                m_scriptRegistry.setValue(tag, listValue.at(i).toFloat(), track);
            }
            else {
                m_scriptRegistry.setValue(tag, listValue.at(i), track);
            }
        }
        else if(isList) {
            m_scriptRegistry.setValue(tag, listValue, track);
        }
        else if(isFloat) {
            m_scriptRegistry.setValue(tag, floatValue, track);
        }
        else {
            m_scriptRegistry.setValue(tag, value.toString(), track);
        }

        ++i;
    }

    return true;
}

TagEditorModel::TagEditorModel(SettingsManager* settings, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<TagEditorModelPrivate>(this, settings)}
{ }

TagEditorModel::~TagEditorModel() = default;

TrackList TagEditorModel::tracks() const
{
    return p->m_tracks;
}

void TagEditorModel::reset(const TrackList& tracks, const std::vector<TagEditorField>& fields)
{
    beginResetModel();
    p->reset();
    p->m_tracks = tracks;
    p->m_fields = fields;

    for(const auto& field : fields) {
        auto* item = &p->m_tags.emplace(field.scriptField.toUpper(), TagEditorItem{field, &p->m_root}).first->second;
        p->m_root.appendChild(item);
    }

    p->updateFields();
    p->m_root.sortCustomTags();

    endResetModel();
}

void TagEditorModel::setRatingRow(int row)
{
    p->m_ratingRow = row;
}

void TagEditorModel::autoNumberTracks()
{
    const auto total = static_cast<int>(p->m_tracks.size());
    QStringList trackNums;

    for(int i{1}; i <= total; ++i) {
        trackNums.append(QString::number(i));
    }

    const auto addMissingTag = [this](const QString& tag) {
        if(!p->m_tags.contains(tag)) {
            TagEditorField field;
            field.name        = tag;
            field.scriptField = tag;

            auto* item = &p->m_tags.emplace(tag, TagEditorItem{field, &p->m_root}).first->second;
            p->m_root.appendChild(item);
        }
    };

    static const auto trackTag      = QString::fromLatin1(Constants::MetaData::Track);
    static const auto trackTotalTag = QString::fromLatin1(Constants::MetaData::TrackTotal);

    addMissingTag(trackTag);
    addMissingTag(trackTotalTag);

    auto& track = p->m_tags.at(trackTag);
    if(track.setValue(trackNums.join(u"; "))) {
        track.setMultipleValues(total > 1);
        track.setSplitTrackValues(true);
        emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
    }

    auto& totalTotal = p->m_tags.at(trackTotalTag);
    if(totalTotal.setValue(total)) {
        emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
    }
}

void TagEditorModel::updateValues(const std::map<QString, QString>& fieldValues, bool match)
{
    auto splitValues = [this](const QString& value) {
        QStringList values = value.split(QStringLiteral("; "));
        values.resize(static_cast<qsizetype>(p->m_tracks.size()));
        return values.join(u"; ").remove(QLatin1String{MultipleValuesPrefix}).trimmed();
    };

    auto updateItem = [&](TagEditorItem& item, const QString& value) {
        if(item.value() != value) {
            if(value.contains(QLatin1String{MultipleValuesPrefix})) {
                const QString splitValue = splitValues(value);
                item.setValue(splitValue);
                item.setMultipleValues(p->m_tracks.size() > 1);
                item.setSplitTrackValues(true);
            }
            else {
                item.setValue(value);
            }
        }
    };

    for(const auto& [tag, value] : fieldValues) {
        const auto tagIt
            = std::ranges::find_if(p->m_tags, [&tag](const auto& field) { return field.second.field().name == tag; });
        if(tagIt != p->m_tags.cend()) {
            updateItem(tagIt->second, value);
        }
        else if(match) {
            const int row = rowCount({});
            beginInsertRows({}, row, row);

            auto* item = &p->m_tags.emplace(tag, TagEditorItem{{}, &p->m_root}).first->second;
            item->setTitle(tag);
            item->addTrack(static_cast<int>(p->m_tracks.size()));
            updateItem(*item, value);
            item->setStatus(TagEditorItem::Added);
            p->m_root.appendChild(item);

            endInsertRows();
        }
    }

    emit dataChanged({}, {}, {Qt::DisplayRole});
}

bool TagEditorModel::haveChanges()
{
    return std::ranges::any_of(p->m_tags, [](const auto& tag) { return tag.second.status() != TagEditorItem::None; });
}

bool TagEditorModel::haveOnlyStatChanges()
{
    return std::ranges::all_of(
               p->m_tags,
               [](const auto& tag) { return tag.first == u"Rating" || tag.second.status() == TagEditorItem::None; })
        && p->m_tags.at(QStringLiteral("Rating")).status() == TagEditorItem::Changed;
}

void TagEditorModel::applyChanges()
{
    QStringList fieldsToRemove;

    for(auto& [index, node] : p->m_tags) {
        const TagEditorItem::ItemStatus status = node.status();
        TagEditorField field                   = node.field();
        if(field.scriptField.isEmpty()) {
            field.scriptField = node.titleChanged() ? node.changedTitle() : node.title();
        }
        field.multivalue = Track::isMultiValueTag(field.scriptField);
        field.isDefault  = !Track::isExtraTag(field.scriptField);

        switch(status) {
            case(TagEditorItem::Added): {
                if(p->updateTrackMetadata(field, node.changedValue(), node.splitTrackValues())) {
                    node.applyChanges(field);
                    emit dataChanged({}, {}, {Qt::FontRole});
                }
                break;
            }
            case(TagEditorItem::Removed): {
                if(p->updateTrackMetadata(field, {})) {
                    beginRemoveRows({}, node.row(), node.row());
                    p->m_root.removeChild(node.row());
                    endRemoveRows();
                    fieldsToRemove.push_back(node.title());
                }
                break;
            }
            case(TagEditorItem::Changed): {
                if(node.titleChanged()) {
                    const auto fieldIt = std::ranges::find_if(std::as_const(p->m_tags), [node](const auto& tag) {
                        return tag.second.title() == node.title();
                    });
                    if(fieldIt != p->m_tags.end()) {
                        auto tagItem      = p->m_tags.extract(fieldIt);
                        field.scriptField = tagItem.key();
                        if(p->updateTrackMetadata(field, {})) {
                            const QString key = node.changedTitle();
                            tagItem.key()     = key;
                            p->m_tags.insert(std::move(tagItem));
                        }
                    }
                }

                if(p->updateTrackMetadata(node.field(), node.valueChanged() ? node.changedValue() : node.value(),
                                          node.splitTrackValues())) {
                    node.applyChanges(field);
                    node.setSplitTrackValues(false);
                    emit dataChanged({}, {}, {Qt::FontRole});
                }
                break;
            }
            case(TagEditorItem::None):
                break;
        }
    }

    for(const auto& field : fieldsToRemove) {
        p->m_tags.erase(field);
    }
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

    if(role == Qt::DisplayRole || role == Qt::EditRole || role == TagEditorItem::Title) {
        if(index.column() == 0) {
            const QString title = item->titleChanged() ? item->changedTitle() : item->title();
            if(role == Qt::EditRole) {
                return title;
            }
            if(!item->isSetField() && role != TagEditorItem::Title) {
                const QString name = QStringLiteral("<") + title + QStringLiteral(">");
                return name;
            }

            return title;
        }

        if(index.row() == p->m_ratingRow) {
            if(item->trackCount() > 1) {
                return QString::fromLatin1(MultipleValuesPrefix);
            }
            return QVariant::fromValue(
                StarRating{item->valueChanged() ? item->changedValue().toFloat() : item->value().toFloat(), 5,
                           p->m_settings->value<Settings::Gui::StarRatingSize>()});
        }

        if(role == Qt::EditRole) {
            return item->valueChanged() ? item->changedValue() : item->value();
        }

        return item->valueChanged() ? item->changedDisplayValue() : item->displayValue();
    }

    return {};
}

bool TagEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(p->m_tracks.empty()) {
        return false;
    }

    if(role != Qt::EditRole) {
        return false;
    }

    auto* item = static_cast<TagEditorItem*>(index.internalPointer());

    switch(index.column()) {
        case(0): {
            const QString newTitle = value.toString().toUpper();

            if((item->status() == TagEditorItem::Added && newTitle == p->m_defaultFieldtext)
               || p->hasTagConflict(item, newTitle)) {
                if(item->status() == TagEditorItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }

            if(!item->setTitle(newTitle)) {
                if(item->status() == TagEditorItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }

            break;
        }
        case(1): {
            QString setValue = value.toString().trimmed();

            if(index.row() == p->m_ratingRow) {
                const auto rating = value.value<StarRating>();
                setValue          = rating.rating() == 0 ? QString{} : QString::number(rating.rating());
            }

            if(!item->setValue(setValue)) {
                return false;
            }
            break;
        }
        default:
            break;
    }

    emit dataChanged(index, index);

    return true;
}

QModelIndex TagEditorModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    TagEditorItem* item = p->m_root.child(row);

    return createIndex(row, column, item);
}

int TagEditorModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->m_root.childCount();
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

        auto* item = static_cast<TagEditorItem*>(index.internalPointer());
        if(!item) {
            continue;
        }

        if(item->isSetField()) {
            setData(index.siblingAtColumn(1), {}, Qt::EditRole);
            continue;
        }

        if(item->status() == TagEditorItem::Added) {
            beginRemoveRows({}, i, i);
            p->m_root.removeChild(i);
            endRemoveRows();
            p->m_tags.erase(p->m_defaultFieldtext + QString::number(row));
        }
        else {
            item->setStatus(TagEditorItem::Removed);
            emit dataChanged({}, {}, {Qt::FontRole});
        }
    }

    return true;
}

void TagEditorModel::addPendingRow()
{
    TagEditorField field;
    field.name = p->m_defaultFieldtext;
    TagEditorItem newItem{field, &p->m_root};
    newItem.setStatus(TagEditorItem::Added);

    const int row = p->m_root.childCount();
    auto* item    = &p->m_tags.emplace(p->m_defaultFieldtext + QString::number(row), newItem).first->second;

    beginInsertRows({}, row, row);
    p->m_root.appendChild(item);
    endInsertRows();
}

void TagEditorModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    p->m_root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin::TagEditor

#include "moc_tageditormodel.cpp"
