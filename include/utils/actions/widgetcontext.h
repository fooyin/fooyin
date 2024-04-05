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

#pragma once

#include "fyutils_export.h"

#include <utils/id.h>

#include <QPointer>
#include <QWidget>

namespace Fooyin {
namespace Constants::Context {
constexpr auto Global = "Context.Global";
}

class FYUTILS_EXPORT Context
{
public:
    Context() = default;
    explicit Context(const Id& id);
    explicit Context(const IdList& ids);

    bool operator==(const Context& context) const;

    [[nodiscard]] int size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool contains(const Id& id) const;

    [[nodiscard]] IdList::const_iterator begin() const;
    [[nodiscard]] IdList::const_iterator end() const;

    void append(const Id& id);
    void append(const Context& context);
    void prepend(const Id& id);

    void erase(const Id& id);

private:
    IdList m_ids;
};

class FYUTILS_EXPORT WidgetContext : public QObject
{
    Q_OBJECT

public:
    explicit WidgetContext(QWidget* widget, QObject* parent = nullptr);
    WidgetContext(QWidget* widget, Context context, QObject* parent = nullptr);

    [[nodiscard]] Context context() const;
    [[nodiscard]] QWidget* widget() const;

    [[nodiscard]] bool isEnabled() const;

    void setEnabled(bool enabled);

signals:
    void isEnabledChanged();

private:
    QPointer<QWidget> m_widget;
    Context m_context;
    bool m_isEnabled;
};
using WidgetContextList = std::vector<WidgetContext*>;
} // namespace Fooyin
