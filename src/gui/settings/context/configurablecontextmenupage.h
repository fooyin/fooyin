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

#pragma once

#include "configurablecontextmenumodel.h"

#include <gui/settings/context/staticcontextmenupage.h>
#include <utils/settings/settingspage.h>

#include <functional>

class QTreeView;

namespace Fooyin {
struct ConfigurableContextMenuDefinition
{
    using StringListReader = std::function<QStringList()>;
    using StringListWriter = std::function<void(const QStringList&)>;

    std::function<std::vector<ConfigurableContextMenuNode>()> nodeFactory;
    StringListReader readDisabledIds;
    StringListWriter writeDisabledIds;
    QStringList defaultDisabledIds;
    StringListReader readTopLevelOrder;
    StringListWriter writeTopLevelOrder;
    QStringList defaultTopLevelOrder;
    bool allowReordering{false};
};

class ConfigurableContextMenuWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    ConfigurableContextMenuWidget(QString description, ConfigurableContextMenuDefinition definition,
                                  QWidget* parent = nullptr);

    void load() override;
    void apply() override;
    void reset() override;

private:
    QString m_description;
    ConfigurableContextMenuDefinition m_definition;
    ConfigurableContextMenuModel* m_model;
    QTreeView* m_tree;
};
} // namespace Fooyin
