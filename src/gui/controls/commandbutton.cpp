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

#include "commandbutton.h"

#include "commandbuttonconfigdialog.h"

#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/scripting/scriptcommandhandler.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>

using namespace Qt::StringLiterals;

constexpr auto CommandIdKey   = "CommandButton/CommandId";
constexpr auto TextKey        = "CommandButton/Text";
constexpr auto IconPathKey    = "CommandButton/IconPath";
constexpr auto ButtonStyleKey = "CommandButton/ToolButtonStyle";

namespace {
QString expandPath(const QString& path)
{
    QString trimmed = path.trimmed();
    if(trimmed.isEmpty()) {
        return {};
    }

    if(trimmed == "~"_L1) {
        return QDir::homePath();
    }

    if(trimmed.startsWith("~/"_L1)) {
        return QDir::home().filePath(trimmed.sliced(2));
    }

    return trimmed;
}

QString commandDescription(const QString& commandId)
{
    const auto resolved = Fooyin::ScriptCommandHandler::resolveCommand(commandId);
    if(!resolved) {
        return {};
    }

    switch(resolved->type) {
        case Fooyin::ScriptCommandAliasType::PlayingProperties:
            return Fooyin::CommandButton::tr("Open properties for the currently playing track");
        case Fooyin::ScriptCommandAliasType::PlayingFolder:
            return Fooyin::CommandButton::tr("Open the folder of the currently playing track");
        case Fooyin::ScriptCommandAliasType::Action:
            return {};
    }

    return {};
}
} // namespace

namespace Fooyin {
CommandButton::CommandButton(ActionManager* actionManager, PlayerController* playerController,
                             ScriptCommandHandler* commandHandler, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_commandHandler{commandHandler}
    , m_settings{settings}
    , m_layout{new QHBoxLayout(this)}
    , m_button{new ToolButton(this)}
    , m_boundCommand{nullptr}
{
    setObjectName(CommandButton::name());

    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);
    m_layout->addWidget(m_button);

    m_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_button->setContextMenuPolicy(Qt::CustomContextMenu);
    m_button->setFocusPolicy(Qt::NoFocus);

    QObject::connect(m_button, &QAbstractButton::clicked, this, [this]() {
        if(!m_boundCommand && m_commandHandler && m_commandHandler->canExecute(m_config.commandId)) {
            m_commandHandler->execute(m_config.commandId);
        }
    });
    QObject::connect(m_button, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showContextMenu(m_button->mapToGlobal(pos)); });

    QObject::connect(m_actionManager, &ActionManager::commandsChanged, this, [this]() {
        rebindAction();
        updateButton();
    });
    QObject::connect(m_actionManager, &ActionManager::contextChanged, this, [this]() { updateButton(); });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this]() { updateButton(); });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, [this]() { updateButton(); });

    settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { updateButton(); });
    settings->subscribe<Settings::Gui::Theme>(this, [this]() { updateButton(); });
    settings->subscribe<Settings::Gui::Style>(this, [this]() { updateButton(); });
    settings->subscribe<Settings::Gui::ToolButtonStyle>(this, [this]() { updateButtonStyle(); });

    applyConfig(defaultConfig());
}

QString CommandButton::name() const
{
    return tr("Command Button");
}

QString CommandButton::layoutName() const
{
    return u"CommandButton"_s;
}

CommandButton::ConfigData CommandButton::factoryConfig()
{
    return {};
}

CommandButton::ConfigData CommandButton::defaultConfig() const
{
    auto config{factoryConfig()};

    config.commandId       = m_settings->fileValue(CommandIdKey, config.commandId).toString().trimmed();
    config.text            = m_settings->fileValue(TextKey, config.text).toString();
    config.iconPath        = m_settings->fileValue(IconPathKey, config.iconPath).toString().trimmed();
    config.toolButtonStyle = m_settings->fileValue(ButtonStyleKey, config.toolButtonStyle).toInt();

    return config;
}

const CommandButton::ConfigData& CommandButton::currentConfig() const
{
    return m_config;
}

QIcon CommandButton::previewIcon(const ConfigData& config) const
{
    QIcon configuredIcon = customIcon(config.iconPath);
    if(!configuredIcon.isNull()) {
        return configuredIcon;
    }

    const bool customIconRequested = !config.iconPath.trimmed().isEmpty();

    if(customIconRequested) {
        return fallbackIcon(config.commandId);
    }

    return fallbackIcon(config.commandId);
}

void CommandButton::applyConfig(const ConfigData& config)
{
    m_config.commandId       = config.commandId.trimmed();
    m_config.text            = config.text;
    m_config.iconPath        = config.iconPath.trimmed();
    m_config.toolButtonStyle = config.toolButtonStyle;

    rebindAction();
    updateButtonStyle();
    updateButton();
}

void CommandButton::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(CommandIdKey, config.commandId.trimmed());
    m_settings->fileSet(TextKey, config.text);
    m_settings->fileSet(IconPathKey, config.iconPath.trimmed());
    m_settings->fileSet(ButtonStyleKey, config.toolButtonStyle);
}

void CommandButton::clearSavedDefaults() const
{
    m_settings->fileRemove(CommandIdKey);
    m_settings->fileRemove(TextKey);
    m_settings->fileRemove(IconPathKey);
    m_settings->fileRemove(ButtonStyleKey);
}

void CommandButton::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void CommandButton::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

QSize CommandButton::sizeHint() const
{
    return m_layout->sizeHint();
}

QSize CommandButton::minimumSizeHint() const
{
    return m_layout->minimumSize();
}

void CommandButton::changeEvent(QEvent* event)
{
    FyWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            updateButton();
            break;
        default:
            break;
    }
}

void CommandButton::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void CommandButton::openConfigDialog()
{
    showConfigDialog(new CommandButtonConfigDialog(this, m_actionManager, this));
}

CommandButton::ConfigData CommandButton::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("CommandId"_L1)) {
        config.commandId = layout.value("CommandId"_L1).toString().trimmed();
    }
    if(layout.contains("Text"_L1)) {
        config.text = layout.value("Text"_L1).toString();
    }
    if(layout.contains("IconPath"_L1)) {
        config.iconPath = layout.value("IconPath"_L1).toString().trimmed();
    }
    if(layout.contains("ToolButtonStyle"_L1)) {
        config.toolButtonStyle = layout.value("ToolButtonStyle"_L1).toInt();
    }

    return config;
}

void CommandButton::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["CommandId"_L1]       = config.commandId.trimmed();
    layout["Text"_L1]            = config.text;
    layout["IconPath"_L1]        = config.iconPath.trimmed();
    layout["ToolButtonStyle"_L1] = config.toolButtonStyle;
}

void CommandButton::rebindAction()
{
    if(m_actionChangedConnection) {
        QObject::disconnect(m_actionChangedConnection);
        m_actionChangedConnection = {};
    }

    m_boundCommand = nullptr;
    m_button->setDefaultAction(nullptr);
    m_button->setContextMenuPolicy(Qt::CustomContextMenu);
    m_button->setFocusPolicy(Qt::NoFocus);

    const auto resolved = ScriptCommandHandler::resolveCommand(m_config.commandId);
    if(!resolved || resolved->type != ScriptCommandAliasType::Action || !m_actionManager) {
        return;
    }

    m_boundCommand = m_actionManager->command(Id{resolved->id});
    if(m_boundCommand && m_boundCommand->action()) {
        m_button->setDefaultAction(m_boundCommand->action());
        m_button->setContextMenuPolicy(Qt::CustomContextMenu);
        m_button->setFocusPolicy(Qt::NoFocus);
        m_actionChangedConnection
            = QObject::connect(m_boundCommand->action(), &QAction::changed, this, [this]() { updateButton(); });
    }
}

void CommandButton::updateButton()
{
    const QIcon configuredIcon     = customIcon(m_config.iconPath);
    const bool customIconRequested = !m_config.iconPath.isEmpty();
    const bool usingCustomFallback = customIconRequested && configuredIcon.isNull();
    const bool canExecute          = m_commandHandler && m_commandHandler->canExecute(m_config.commandId);

    m_button->setIcon(configuredIcon.isNull() ? fallbackIcon(m_config.commandId) : configuredIcon);
    m_button->setEnabled(true);

    const QString description = currentDescription(m_config.commandId, m_config.text);
    m_button->setText(description);
    m_button->setToolTip(currentToolTip(usingCustomFallback));
    m_button->setStatusTip(m_button->toolTip());
    m_button->setWhatsThis(m_button->toolTip());
    m_button->updateGeometry();
    m_button->update();

    if(!m_config.commandId.isEmpty()) {
        m_button->setCursor(canExecute ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    else {
        m_button->setCursor(Qt::ArrowCursor);
    }
}

void CommandButton::updateButtonStyle() const
{
    const auto options
        = static_cast<Settings::Gui::ToolButtonOptions>(m_settings->value<Settings::Gui::ToolButtonStyle>());

    m_button->setToolButtonStyle(static_cast<Qt::ToolButtonStyle>(m_config.toolButtonStyle));
    m_button->setStretchEnabled(options & Settings::Gui::Stretch);
    m_button->setAutoRaise(!(options & Settings::Gui::Raise));
}

void CommandButton::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addConfigureAction(menu, false);
    menu->popup(globalPos);
}

QIcon CommandButton::customIcon(const QString& iconPath) const
{
    const QString resolvedPath = resolvedIconPath(iconPath);
    if(resolvedPath.isEmpty()) {
        return {};
    }

    const QFileInfo iconInfo{resolvedPath};
    if(!iconInfo.isFile()) {
        return {};
    }

    const QIcon icon{iconInfo.absoluteFilePath()};
    return icon.isNull() ? QIcon{} : icon;
}

QIcon CommandButton::fallbackIcon(const QString& commandId) const
{
    const auto fallback = []() {
        return Utils::iconFromTheme(Constants::Icons::Command);
    };

    const auto resolved = ScriptCommandHandler::resolveCommand(commandId);
    if(!resolved) {
        return fallback();
    }

    if(!m_actionManager || resolved->type != ScriptCommandAliasType::Action) {
        return fallback();
    }

    if(auto* command = m_actionManager->command(Id{resolved->id})) {
        if(command->action() && !command->action()->icon().isNull()) {
            return command->action()->icon();
        }
    }

    return fallback();
}

QString CommandButton::currentDescription(const QString& commandId, const QString& text) const
{
    if(!text.trimmed().isEmpty()) {
        return text;
    }

    if(const QString description = actionDescription(commandId); !description.isEmpty()) {
        return description;
    }

    if(const QString description = commandDescription(commandId); !description.isEmpty()) {
        return description;
    }

    if(commandId.isEmpty()) {
        return tr("Right-click to configure this button.");
    }

    return commandId;
}

QString CommandButton::currentToolTip(bool usingFallbackForCustomIcon) const
{
    QString toolTip;

    if(m_config.commandId.isEmpty()) {
        toolTip = tr("Right-click to configure this button.");
    }
    else if(m_boundCommand) {
        toolTip = m_boundCommand->stringWithShortcut(m_boundCommand->description());
    }
    else {
        toolTip = currentDescription(m_config.commandId, m_config.text);
        if(toolTip != m_config.commandId) {
            toolTip += u"\n"_s + m_config.commandId;
        }
    }

    const bool canExecute = m_commandHandler && m_commandHandler->canExecute(m_config.commandId);

    if(!m_config.commandId.isEmpty() && !canExecute) {
        toolTip += u"\n"_s + tr("Command is currently unavailable.");
    }

    if(usingFallbackForCustomIcon) {
        toolTip += u"\n"_s + tr("Custom icon missing; using fallback icon.");
    }

    return toolTip;
}

QString CommandButton::resolvedIconPath(const QString& iconPath) const
{
    return expandPath(iconPath);
}

QString CommandButton::actionDescription(const QString& commandId) const
{
    const auto resolved = ScriptCommandHandler::resolveCommand(commandId);
    if(!resolved || resolved->type != ScriptCommandAliasType::Action || !m_actionManager) {
        return {};
    }

    if(auto* command = m_actionManager->command(Id{resolved->id})) {
        return command->description();
    }

    return {};
}
} // namespace Fooyin
