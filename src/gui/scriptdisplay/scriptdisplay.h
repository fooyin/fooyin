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

#include <core/track.h>
#include <gui/fywidget.h>

#include <QString>
#include <memory>

class QContextMenuEvent;
class QEvent;
class QHBoxLayout;
class QJsonObject;
class QResizeEvent;
class QTextBrowser;

namespace Fooyin {
class ActionManager;
class PlayerController;
class PlaylistHandler;
class PropertiesDialog;
class ScriptCommandHandler;
class ScriptParser;
class SettingsManager;

class ScriptDisplay : public FyWidget
{
    Q_OBJECT

public:
    struct ConfigData
    {
        QString script;
        QString font;
        QString bgColour;
        QString fgColour;
        QString linkColour;
        int horizontalAlignment{Qt::AlignLeft};
        int verticalAlignment{Qt::AlignCenter};
    };

    ScriptDisplay(PlayerController* playerController, PlaylistHandler* playlistHandler,
                  ScriptCommandHandler* commandHandler, SettingsManager* settings, QWidget* parent = nullptr);
    ~ScriptDisplay() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] static ConfigData factoryConfig();
    [[nodiscard]] const ConfigData& currentConfig() const;
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
    void resizeEvent(QResizeEvent* event) override;

private:
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    void applyAppearance();
    void updateText();
    void updateViewportAlignment();
    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] QString evaluateScript() const;
    void activateLink(const QString& link) const;

    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    ScriptCommandHandler* m_commandHandler;
    SettingsManager* m_settings;

    std::unique_ptr<ScriptParser> m_scriptParser;
    QHBoxLayout* m_layout;
    QTextBrowser* m_text;
    QString m_lastHtml;
    Track m_lastTrack;
    ConfigData m_config;
};
} // namespace Fooyin
