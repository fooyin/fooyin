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

#include "widgetprovider.h"

#include "gui/widgets/splitterwidget.h"
#include "widgetfactory.h"

#include <core/plugins/pluginmanager.h>
#include <utils/enumhelper.h>

#include <QMenu>

namespace Gui::Widgets {
WidgetProvider::WidgetProvider(Widgets::WidgetFactory* widgetFactory, Core::ActionManager* actionManager,
                               Core::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_widgetFactory{widgetFactory}
    , m_actionManager{actionManager}
    , m_settings{settings}
{ }

WidgetProvider::~WidgetProvider() = default;

FyWidget* WidgetProvider::createWidget(const QString& widget)
{
    FyWidget* createdWidget = m_widgetFactory->make(widget);
    return createdWidget;
}

SplitterWidget* WidgetProvider::createSplitter(Qt::Orientation type, QWidget* parent)
{
    auto* splitter = new SplitterWidget(m_actionManager, this, m_settings, parent);
    splitter->setOrientation(type);
    return splitter;
}
} // namespace Gui::Widgets
