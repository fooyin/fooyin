/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/stringpool.h>

#include <array>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Fooyin {
class StringPoolPrivate
{
public:
    struct Bucket
    {
        mutable std::mutex mutex;
        std::unordered_map<QString, StringPool::StringId> ids;
        std::vector<QString> strings{{}};
        std::vector<StringPool::StringId> listEntries;
    };

    static constexpr auto DomainCount = static_cast<size_t>(StringPool::Domain::ExtraTagKey) + 1;

    [[nodiscard]] Bucket& bucket(StringPool::Domain domain)
    {
        return buckets.at(static_cast<size_t>(domain));
    }

    [[nodiscard]] const Bucket& bucket(StringPool::Domain domain) const
    {
        return buckets.at(static_cast<size_t>(domain));
    }

private:
    std::array<Bucket, DomainCount> buckets;
};

StringPool::StringPool()
    : p{std::make_unique<StringPoolPrivate>()}
{ }

StringPool::~StringPool() = default;

QString StringPool::intern(Domain domain, const QString& value)
{
    return resolve(domain, internId(domain, value));
}

StringPool::StringId StringPool::internId(Domain domain, const QString& value)
{
    if(value.isEmpty()) {
        return EmptyStringId;
    }

    auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    if(const auto it = bucket.ids.find(value); it != bucket.ids.cend()) {
        return it->second;
    }

    const auto id = static_cast<StringId>(bucket.strings.size());
    bucket.strings.push_back(value);
    bucket.ids.emplace(bucket.strings.back(), id);

    return id;
}

StringPool::StringListRef StringPool::internList(Domain domain, const QStringList& values)
{
    if(values.isEmpty()) {
        return {};
    }

    auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    const auto offset = static_cast<uint32_t>(bucket.listEntries.size());
    bucket.listEntries.reserve(bucket.listEntries.size() + static_cast<size_t>(values.size()));

    for(const auto& value : values) {
        if(value.isEmpty()) {
            bucket.listEntries.push_back(EmptyStringId);
            continue;
        }

        if(const auto it = bucket.ids.find(value); it != bucket.ids.cend()) {
            bucket.listEntries.push_back(it->second);
            continue;
        }

        const auto id = static_cast<StringId>(bucket.strings.size());
        bucket.strings.push_back(value);
        bucket.ids.emplace(bucket.strings.back(), id);
        bucket.listEntries.push_back(id);
    }

    return {.offset = offset, .size = static_cast<uint32_t>(values.size())};
}

QString StringPool::resolve(Domain domain, StringId id) const
{
    if(id == EmptyStringId) {
        return {};
    }

    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    return id < bucket.strings.size() ? bucket.strings.at(id) : QString{};
}

QStringList StringPool::resolveList(Domain domain, StringListRef ref) const
{
    if(ref.isEmpty()) {
        return {};
    }

    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    QStringList values;
    values.reserve(ref.size);

    const auto end = static_cast<size_t>(ref.offset) + static_cast<size_t>(ref.size);
    if(end > bucket.listEntries.size()) {
        return {};
    }

    for(size_t index{ref.offset}; index < end; ++index) {
        const auto id = bucket.listEntries.at(index);
        values.push_back(id < bucket.strings.size() ? bucket.strings.at(id) : QString{});
    }

    return values;
}

QString StringPool::joined(Domain domain, StringListRef ref, const QString& separator) const
{
    if(ref.isEmpty()) {
        return {};
    }

    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    const auto end = static_cast<size_t>(ref.offset) + static_cast<size_t>(ref.size);
    if(end > bucket.listEntries.size()) {
        return {};
    }

    QStringList values;
    values.reserve(ref.size);

    for(size_t index{ref.offset}; index < end; ++index) {
        const auto id = bucket.listEntries.at(index);
        values.push_back(id < bucket.strings.size() ? bucket.strings.at(id) : QString{});
    }

    return values.join(separator);
}

QString StringPool::valueAt(Domain domain, StringListRef ref, qsizetype index) const
{
    if(ref.isEmpty() || index < 0 || std::cmp_greater_equal(index, ref.size)) {
        return {};
    }

    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    const auto listIndex = static_cast<size_t>(ref.offset) + static_cast<size_t>(index);
    if(listIndex >= bucket.listEntries.size()) {
        return {};
    }

    const auto id = bucket.listEntries.at(listIndex);
    return id < bucket.strings.size() ? bucket.strings.at(id) : QString{};
}

bool StringPool::contains(Domain domain, StringListRef ref, const QString& value) const
{
    if(ref.isEmpty() || value.isEmpty()) {
        return false;
    }

    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    const auto it = bucket.ids.find(value);
    if(it == bucket.ids.cend()) {
        return false;
    }

    const auto end = static_cast<size_t>(ref.offset) + static_cast<size_t>(ref.size);
    if(end > bucket.listEntries.size()) {
        return false;
    }

    for(size_t listIndex{ref.offset}; listIndex < end; ++listIndex) {
        if(bucket.listEntries.at(listIndex) == it->second) {
            return true;
        }
    }

    return false;
}

QStringList StringPool::values(Domain domain) const
{
    const auto& bucket = p->bucket(domain);

    const std::scoped_lock lock{bucket.mutex};

    QStringList values;
    values.reserve(static_cast<qsizetype>(bucket.strings.size()));

    for(size_t index{1}; index < bucket.strings.size(); ++index) {
        values.push_back(bucket.strings.at(index));
    }

    values.sort(Qt::CaseInsensitive);

    return values;
}
} // namespace Fooyin
