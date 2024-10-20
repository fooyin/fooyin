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

#include <gui/fywidget.h>
#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class EditableLayout;
class SearchControllerPrivate;

class SearchController : public QObject
{
    Q_OBJECT

public:
    explicit SearchController(EditableLayout* editableLayout, QObject* parent = nullptr);
    ~SearchController() override;

    void loadWidgets();

    void setupWidgetConnections(const Id& id);
    void registerSetFunction(const Id& id, const std::function<void(const QString&)>& func);

    WidgetList connectedWidgets(const Id& id);
    IdSet connectedWidgetIds(const Id& id);

    void setConnectedWidgets(const Id& id, const IdSet& widgets);
    void removeConnectedWidgets(const Id& id);

    void changeSearch(const Id& id, const QString& search);

signals:
    void connectionChanged(const Fooyin::Id& id);

private:
    std::unique_ptr<SearchControllerPrivate> p;
};
} // namespace Fooyin
