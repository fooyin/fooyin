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

#pragma once

#include "fygui_export.h"

#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class EditableLayout;

class FYGUI_EXPORT SearchController : public QObject
{
    Q_OBJECT

public:
    explicit SearchController(EditableLayout* editableLayout, QObject* parent = nullptr);
    ~SearchController() override;

    void setupWidgetConnections(const Id& id);
    IdSet connectedWidgets(const Id& id);
    void setConnectedWidgets(const Id& id, const IdSet& widgets);
    void removeConnectedWidgets(const Id& id);

    void changeSearch(const Id& id, const QString& search);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
