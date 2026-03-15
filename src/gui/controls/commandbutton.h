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

#pragma once

#include <gui/fywidget.h>

#include <QIcon>

class QContextMenuEvent;
class QEvent;
class QHBoxLayout;
class QJsonObject;
class QPoint;

namespace Fooyin {
class ActionManager;
class Command;
class PlayerController;
class ScriptCommandHandler;
class SettingsManager;
class ToolButton;

class CommandButton : public FyWidget
{
    Q_OBJECT

public:
    struct ConfigData
    {
        QString commandId;
        QString text;
        QString iconPath;
        int toolButtonStyle{Qt::ToolButtonIconOnly};
    };

    CommandButton(ActionManager* actionManager, PlayerController* playerController,
                  ScriptCommandHandler* commandHandler, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] static ConfigData factoryConfig();
    [[nodiscard]] const ConfigData& currentConfig() const;
    [[nodiscard]] QIcon previewIcon(const ConfigData& config) const;
    void applyConfig(const ConfigData& config);
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void openConfigDialog() override;

private:
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    void rebindAction();
    void updateButton();
    void updateButtonStyle() const;
    void showContextMenu(const QPoint& globalPos);

    [[nodiscard]] QIcon customIcon(const QString& iconPath) const;
    [[nodiscard]] QIcon fallbackIcon(const QString& commandId) const;
    [[nodiscard]] QString currentDescription(const QString& commandId, const QString& text) const;
    [[nodiscard]] QString currentToolTip(bool usingFallbackForCustomIcon) const;
    [[nodiscard]] QString resolvedIconPath(const QString& iconPath) const;
    [[nodiscard]] QString actionDescription(const QString& commandId) const;

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    ScriptCommandHandler* m_commandHandler;
    SettingsManager* m_settings;

    ConfigData m_config;
    QHBoxLayout* m_layout;
    ToolButton* m_button;
    Command* m_boundCommand;
    QMetaObject::Connection m_actionChangedConnection;
};
} // namespace Fooyin
