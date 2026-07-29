/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <utils/jsonutils.h>

#include <QJsonArray>
#include <QJsonObject>

#include <set>
#include <unordered_map>

namespace Fooyin::Utils {
QJsonValue mergeJsonThreeWay(const QJsonValue& base, const QJsonValue& first, const QJsonValue& second,
                             const JsonArrayItemIdentity& arrayItemIdentity)
{
    if(second == base) {
        return first;
    }
    if(first == base || first == second) {
        return second;
    }

    if(base.isObject() && first.isObject() && second.isObject()) {
        const auto baseObject   = base.toObject();
        const auto firstObject  = first.toObject();
        const auto secondObject = second.toObject();

        std::set<QString> keys;
        for(auto it = baseObject.constBegin(); it != baseObject.constEnd(); ++it) {
            keys.insert(it.key());
        }
        for(auto it = firstObject.constBegin(); it != firstObject.constEnd(); ++it) {
            keys.insert(it.key());
        }
        for(auto it = secondObject.constBegin(); it != secondObject.constEnd(); ++it) {
            keys.insert(it.key());
        }

        QJsonObject merged;
        for(const auto& key : keys) {
            const auto value = mergeJsonThreeWay(baseObject.value(key), firstObject.value(key), secondObject.value(key),
                                                 arrayItemIdentity);
            if(!value.isUndefined()) {
                merged.insert(key, value);
            }
        }
        return merged;
    }

    if(base.isArray() && first.isArray() && second.isArray()) {
        const auto baseArray   = base.toArray();
        const auto firstArray  = first.toArray();
        const auto secondArray = second.toArray();

        if(arrayItemIdentity) {
            std::unordered_map<QString, QJsonValue> baseItems;
            std::unordered_map<QString, QJsonValue> firstItems;
            std::unordered_map<QString, QJsonValue> secondItems;
            bool identified{true};

            for(const auto& value : baseArray) {
                const QString identity = arrayItemIdentity(value);
                identified &= !identity.isEmpty() && baseItems.emplace(identity, value).second;
            }
            for(const auto& value : firstArray) {
                const QString identity = arrayItemIdentity(value);
                identified &= !identity.isEmpty() && firstItems.emplace(identity, value).second;
            }
            for(const auto& value : secondArray) {
                const QString identity = arrayItemIdentity(value);
                identified &= !identity.isEmpty() && secondItems.emplace(identity, value).second;
            }

            if(!identified) {
                return second;
            }

            QJsonArray merged;
            for(const auto& value : secondArray) {
                const QString identity = arrayItemIdentity(value);
                const auto baseItem    = baseItems.find(identity);
                const auto firstItem   = firstItems.find(identity);

                if(baseItem == baseItems.end()) {
                    merged.append(value);
                }
                else if(firstItem != firstItems.end()) {
                    merged.append(mergeJsonThreeWay(baseItem->second, firstItem->second, value, arrayItemIdentity));
                }
                else if(value != baseItem->second) {
                    merged.append(value);
                }
            }

            for(const auto& value : firstArray) {
                const QString identity = arrayItemIdentity(value);
                if(!baseItems.contains(identity) && !secondItems.contains(identity)) {
                    merged.append(value);
                }
            }

            return merged;
        }

        if(baseArray.size() == firstArray.size() && firstArray.size() == secondArray.size()) {
            QJsonArray merged;
            for(qsizetype i{0}; i < secondArray.size(); ++i) {
                merged.append(
                    mergeJsonThreeWay(baseArray.at(i), firstArray.at(i), secondArray.at(i), arrayItemIdentity));
            }
            return merged;
        }
    }

    return second;
}
} // namespace Fooyin::Utils
