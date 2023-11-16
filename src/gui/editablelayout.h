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

#include <gui/layoutprovider.h>
#include <gui/widgetprovider.h>

#include <QWidget>

namespace Fooyin {
class ActionManager;
class ActionContainer;
class SettingsManager;
class FyWidget;

class EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                            LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void initialise();

    ActionContainer* createNewMenu(FyWidget* parent, const QString& title) const;
    void setupReplaceWidgetMenu(ActionContainer* menu, FyWidget* current);
    void setupContextMenu(FyWidget* widget, ActionContainer* menu);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void changeLayout(const Layout& layout);
    void saveLayout();
    bool loadLayout(const Layout& layout);
    bool loadLayout();

    void showOverlay(FyWidget* widget);
    void hideOverlay();

    void showQuickSetup();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
