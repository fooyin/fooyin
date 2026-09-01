/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <utils/actions/widgetcontext.h>
#include <utils/id.h>

#include <QObject>

#include <memory>
#include <vector>

class QAction;
class QMainWindow;

namespace Fooyin {
class ActionManagerPrivate;
class Command;
class SettingsManager;
class ActionContainer;

using CommandList = std::vector<Command*>;

enum class GlobalShortcutManagement : uint8_t
{
    Unavailable = 0,
    ApplicationManaged,
    SystemManaged,
};

class FYUTILS_EXPORT ActionManager : public QObject
{
    Q_OBJECT

public:
    explicit ActionManager(SettingsManager* settingsManager, QObject* parent = nullptr);
    ~ActionManager() override;

    void setMainWindow(QMainWindow* mainWindow);

    void saveSettings();

    [[nodiscard]] WidgetContext* currentContextObject() const;
    [[nodiscard]] QWidget* currentContextWidget() const;
    [[nodiscard]] WidgetContext* contextObject(QWidget* widget) const;
    void addContextObject(WidgetContext* context);
    void overrideContext(WidgetContext* context, bool override);
    void removeContextObject(WidgetContext* context);

    ActionContainer* createMenu(const Id& id);
    ActionContainer* createMenuBar(const Id& id);
    Command* registerAction(QAction* action, const Id& id,
                            const Context& context = Context{Constants::Context::Global});
    bool unregisterAction(QAction* action, const Id& id, const Context& context = Context{Constants::Context::Global});

    [[nodiscard]] Command* command(const Id& id) const;
    [[nodiscard]] CommandList commands() const;
    [[nodiscard]] ActionContainer* actionContainer(const Id& id) const;

    [[nodiscard]] GlobalShortcutManagement globalShortcutManagement() const;
    void setGlobalShortcutManagement(GlobalShortcutManagement management);
    [[nodiscard]] bool globalShortcutConfigurationAvailable() const;
    void setGlobalShortcutConfigurationAvailable(bool available);
    void configureGlobalShortcuts();

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void commandsChanged();
    void contextChanged(const Fooyin::Context& context);
    void globalShortcutManagementChanged(Fooyin::GlobalShortcutManagement management);
    void globalShortcutConfigurationAvailabilityChanged(bool available);
    void globalShortcutConfigurationRequested();

private:
    std::unique_ptr<ActionManagerPrivate> p;
};
} // namespace Fooyin
