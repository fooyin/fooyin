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

#include "layoutprovider.h"

#include <QWidget>

class QHBoxLayout;

namespace Utils {
class OverlayFilter;
class ActionManager;
class ActionContainer;
} // namespace Utils

namespace Core {
class SettingsManager;
} // namespace Core

namespace Gui::Widgets {
class FyWidget;
class WidgetFactory;
class WidgetProvider;
class SplitterWidget;

class EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(Core::SettingsManager* settings, Utils::ActionManager* actionManager,
                            WidgetFactory* widgetFactory, WidgetProvider* widgetProvider,
                            LayoutProvider* layoutProvider, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void initialise();

    Utils::ActionContainer* createNewMenu(FyWidget* parent, const QString& title) const;
    void setupWidgetMenu(Utils::ActionContainer* menu, FyWidget* parent, bool replace = false);
    void setupContextMenu(FyWidget* widget, Utils::ActionContainer* menu);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void changeLayout(const Layout& layout);
    void saveLayout();
    bool loadLayout(const QByteArray& layout);
    bool loadLayout();

    void showOverlay(FyWidget* widget);
    void hideOverlay();

private:
    Utils::ActionManager* m_actionManager;
    Core::SettingsManager* m_settings;
    Widgets::WidgetFactory* m_widgetFactory;
    Widgets::WidgetProvider* m_widgetProvider;
    LayoutProvider* m_layoutProvider;

    Utils::ActionContainer* m_menu;
    QHBoxLayout* m_box;
    Utils::OverlayFilter* m_overlay;
    SplitterWidget* m_splitter;
    int m_menuLevels;
    bool m_layoutEditing;
};
} // namespace Gui::Widgets
