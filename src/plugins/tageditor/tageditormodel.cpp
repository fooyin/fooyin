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

struct EditorPair
{
    QString displayName;
    std::function<QString(const Track&)> metadata;
};

class TagEditorModelPrivate
{
public:
    explicit TagEditorModelPrivate(TagEditorModel* self, SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
    { }

    bool hasCustomTagConflict(TagEditorItem* item, const QString& title) const;
    bool isDefaultField(const QString& name) const;
    QString findSetField(const QString& name);

    void reset();

    void updateFields();
    void updateEditorValues();

    void updateTrackMetadata(const QString& name, const QVariant& value, bool split = false);

    void addCustomTrackMetadata(const QString& name, const QString& value, bool split = false);
    void replaceCustomTrackMetadata(const QString& name, const QString& value, bool split = false);
    void removeCustomTrackMetadata(const QString& name);

    TagEditorModel* m_self;

    SettingsManager* m_settings;
    ScriptRegistry m_scriptRegistry;

    TrackList m_tracks;

    QString m_defaultFieldtext{QStringLiteral("<input field name>")};
    // TODO: Make m_fields shown configurable
    const std::vector<EditorPair> m_fields{
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

    const std::unordered_map<QString, QString> m_setFields{
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

    TagEditorItem m_root;
    TagFieldMap m_tags;
    TagFieldMap m_customTags;
};

bool TagEditorModelPrivate::hasCustomTagConflict(TagEditorItem* item, const QString& title) const
{
    for(const auto& [_, tag] : m_customTags) {
        if(item != &tag) {
            if(tag.title() == title || tag.changedTitle() == title) {
                return true;
            }
        }
    }
    return false;
}

bool TagEditorModelPrivate::isDefaultField(const QString& name) const
{
    return std::ranges::any_of(std::as_const(m_fields),
                               [name](const EditorPair& pair) { return pair.displayName == name; });
}

QString TagEditorModelPrivate::findSetField(const QString& name)
{
    if(!m_setFields.contains(name)) {
        return {};
    }
    return m_setFields.at(name);
}

void TagEditorModelPrivate::reset()
{
    m_root = {};
    m_tags.clear();
    m_customTags.clear();
}

void TagEditorModelPrivate::updateFields()
{
    for(const Track& track : m_tracks) {
        const auto trackTags = track.extraTags();

        for(const auto& [field, values] : Utils::asRange(trackTags)) {
            if(values.empty()) {
                continue;
            }

            if(!m_customTags.contains(field)) {
                auto* item = &m_customTags.emplace(field, TagEditorItem{field, &m_root, false}).first->second;
                m_root.appendChild(item);
            }
        }
    }

    updateEditorValues();
}

void TagEditorModelPrivate::updateEditorValues()
{
    if(m_tracks.empty()) {
        return;
    }

    for(const auto& track : m_tracks) {
        for(const auto& [field, var] : m_fields) {
            const auto result = var(track);
            if(result.contains(u"\037")) {
                m_tags[field].addTrackValue(result.split(QStringLiteral("\037")));
            }
            else {
                m_tags[field].addTrackValue(result);
            }
        }

        for(auto& [field, node] : m_customTags) {
            const auto result = m_scriptRegistry.value(field, track);
            node.addTrackValue(result.value.split(u'\037'));
        }
    }
}

void TagEditorModelPrivate::updateTrackMetadata(const QString& name, const QVariant& value, bool split)
{
    if(m_tracks.empty()) {
        return;
    }

    const QString metadata = findSetField(name);
    const bool isList      = split
                     || (metadata == QLatin1String{Constants::MetaData::AlbumArtist}
                         || metadata == QLatin1String{Constants::MetaData::Artist}
                         || metadata == QLatin1String{Constants::MetaData::Genre});
    const bool isFloat = (metadata == QLatin1String{Constants::MetaData::Rating});

    QStringList listValue;
    float floatValue{-1};

    if(isList) {
        listValue = value.toString().split(u';', Qt::SkipEmptyParts);
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
                m_scriptRegistry.setValue(metadata, listValue.at(i).toFloat(), track);
            }
            else {
                m_scriptRegistry.setValue(metadata, listValue.at(i), track);
            }
        }
        else if(isList) {
            m_scriptRegistry.setValue(metadata, listValue, track);
        }
        else if(isFloat) {
            m_scriptRegistry.setValue(metadata, floatValue, track);
        }
        else {
            m_scriptRegistry.setValue(metadata, value.toString(), track);
        }

        ++i;
    }
}

void TagEditorModelPrivate::addCustomTrackMetadata(const QString& name, const QString& value, bool split)
{
    if(m_tracks.empty()) {
        return;
    }

    QStringList listValue = value.split(u';', Qt::SkipEmptyParts);
    std::ranges::transform(listValue, listValue.begin(), [](const QString& val) { return val.trimmed(); });

    for(int i{0}; Track & track : m_tracks) {
        QString newValue{value};
        if(split && std::cmp_less(i, listValue.size())) {
            newValue = listValue.at(i);
        }
        track.addExtraTag(name, newValue);
        ++i;
    }
}

void TagEditorModelPrivate::replaceCustomTrackMetadata(const QString& name, const QString& value, bool split)
{
    if(m_tracks.empty()) {
        return;
    }

    QStringList listValue = value.split(u';', Qt::SkipEmptyParts);
    std::ranges::transform(listValue, listValue.begin(), [](const QString& val) { return val.trimmed(); });

    for(int i{0}; Track & track : m_tracks) {
        QString newValue{value};
        if(split && std::cmp_less(i, listValue.size())) {
            newValue = listValue.at(i);
        }
        track.replaceExtraTag(name, newValue);
        ++i;
    }
}

void TagEditorModelPrivate::removeCustomTrackMetadata(const QString& name)
{
    if(m_tracks.empty()) {
        return;
    }

    for(Track& track : m_tracks) {
        track.removeExtraTag(name);
    }
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

void TagEditorModel::reset(const TrackList& tracks)
{
    beginResetModel();
    p->reset();
    p->m_tracks = tracks;

    for(const auto& [field, _] : p->m_fields) {
        auto* item = &p->m_tags.emplace(field, TagEditorItem{field, &p->m_root, true}).first->second;
        p->m_root.appendChild(item);
    }

    p->updateFields();
    p->m_root.sortCustomTags();

    endResetModel();
}

void TagEditorModel::autoNumberTracks()
{
    const auto total = static_cast<int>(p->m_tracks.size());
    QStringList trackNums;
    for(int i{1}; i <= total; ++i) {
        trackNums.append(QString::number(i));
    }

    auto& trackTag = p->m_tags.at(QLatin1String("Track Number"));
    auto& totalTag = p->m_tags.at(QLatin1String("Total Tracks"));

    if(trackTag.setValue(trackNums.join(u"; "))) {
        trackTag.setMultipleValues(total > 1);
        trackTag.setSplitTrackValues(true);
    }

    totalTag.setValue(total);
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
        if(p->m_tags.contains(tag)) {
            updateItem(p->m_tags.at(tag), value);
        }
        else if(p->m_customTags.contains(tag)) {
            updateItem(p->m_customTags.at(tag), value);
        }
        else if(match) {
            const int row = rowCount({});
            beginInsertRows({}, row, row);

            auto* item = &p->m_customTags.emplace(tag, TagEditorItem{{}, &p->m_root, false}).first->second;
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
    return std::ranges::any_of(p->m_tags, [](const auto& tag) { return tag.second.status() != TagEditorItem::None; })
        || std::ranges::any_of(p->m_customTags,
                               [](const auto& tag) { return tag.second.status() != TagEditorItem::None; });
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
    const auto updateChangedNodes = [this](TagFieldMap& tags, bool custom) {
        QStringList fieldsToRemove;

        for(auto& [index, node] : tags) {
            const TagEditorItem::ItemStatus status = node.status();

            switch(status) {
                case(TagEditorItem::Added): {
                    if(custom) {
                        p->addCustomTrackMetadata(node.changedTitle(), node.changedValue(), node.splitTrackValues());
                    }
                    else {
                        p->updateTrackMetadata(node.changedTitle(), node.changedValue(), node.splitTrackValues());
                    }

                    node.applyChanges();
                    emit dataChanged({}, {}, {Qt::FontRole});
                    break;
                }
                case(TagEditorItem::Removed): {
                    p->removeCustomTrackMetadata(node.title());

                    beginRemoveRows({}, node.row(), node.row());
                    p->m_root.removeChild(node.row());
                    endRemoveRows();
                    fieldsToRemove.push_back(node.title());

                    break;
                }
                case(TagEditorItem::Changed): {
                    if(custom) {
                        const auto fieldIt
                            = std::ranges::find_if(std::as_const(p->m_customTags), [node](const auto& tag) {
                                  return tag.second.title() == node.title();
                              });
                        if(fieldIt != p->m_customTags.end()) {
                            auto tagItem = p->m_customTags.extract(fieldIt);

                            p->removeCustomTrackMetadata(tagItem.key());

                            tagItem.key() = node.changedTitle();
                            p->m_customTags.insert(std::move(tagItem));

                            p->replaceCustomTrackMetadata(node.changedTitle(), node.changedValue(),
                                                          node.splitTrackValues());
                            node.applyChanges();
                        }
                    }
                    else {
                        p->updateTrackMetadata(node.title(), node.changedValue(), node.splitTrackValues());
                        node.applyChanges();
                        node.setSplitTrackValues(false);
                    }

                    emit dataChanged({}, {}, {Qt::FontRole});
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

    updateChangedNodes(p->m_tags, false);
    updateChangedNodes(p->m_customTags, true);
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
            if(!item->isDefault() && role != TagEditorItem::Title) {
                const QString name = QStringLiteral("<") + title + QStringLiteral(">");
                return name;
            }

            return title;
        }

        if(index.row() == 13) {
            if(!item->valueChanged() && item->multipleValues()) {
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
            const QString newTitle = value.toString();
            if((item->status() == TagEditorItem::Added && newTitle == p->m_defaultFieldtext)
               || p->hasCustomTagConflict(item, newTitle)) {
                if(item->status() == TagEditorItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }

            const QString name = value.toString().toUpper();
            if(!item->setTitle(name)) {
                if(item->status() == TagEditorItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }

            break;
        }
        case(1): {
            QString setValue = value.toString();

            if(index.row() == 13) {
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

        if(index.data(TagEditorItem::IsDefault).toBool()) {
            setData(index.siblingAtColumn(1), {}, Qt::EditRole);
            continue;
        }

        auto* item = static_cast<TagEditorItem*>(index.internalPointer());
        if(item) {
            if(item->status() == TagEditorItem::Added) {
                beginRemoveRows({}, i, i);
                p->m_root.removeChild(i);
                endRemoveRows();
                p->m_customTags.erase(p->m_defaultFieldtext + QString::number(row));
            }
            else {
                item->setStatus(TagEditorItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }

    return true;
}

void TagEditorModel::addPendingRow()
{
    TagEditorItem newItem{p->m_defaultFieldtext, &p->m_root, false};
    newItem.setStatus(TagEditorItem::Added);

    const int row = p->m_root.childCount();
    auto* item    = &p->m_customTags.emplace(p->m_defaultFieldtext + QString::number(row), newItem).first->second;

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
