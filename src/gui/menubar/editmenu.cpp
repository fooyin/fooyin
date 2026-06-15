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

#include "editmenu.h"

#include <core/library/sortingregistry.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin {
EditMenu::EditMenu(ActionManager* actionManager, SortingRegistry* sortingRegistry, SettingsManager* settings,
                   QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_sortingRegistry{sortingRegistry}
    , m_settings{settings}
    , m_randomiseAction{new QAction(tr("Randomise"), this)}
    , m_reverseAction{new QAction(tr("Reverse"), this)}
{
    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    const QStringList editCategory = {tr("Edit")};
    const QStringList sortCategory = {tr("Edit"), tr("Sort")};

    auto* sortMenu = m_actionManager->createMenu(Constants::Menus::EditSort);
    sortMenu->menu()->setTitle(tr("Sort"));
    editMenu->addMenu(sortMenu, Actions::Groups::Three);

    m_randomiseAction->setEnabled(false);
    auto* randomiseCmd = m_actionManager->registerAction(m_randomiseAction, Constants::Actions::SortRandomise);
    randomiseCmd->setCategories(sortCategory);
    randomiseCmd->setAttribute(ProxyAction::UpdateText);
    sortMenu->addAction(randomiseCmd, Actions::Groups::One);

    m_reverseAction->setEnabled(false);
    auto* reverseCmd = m_actionManager->registerAction(m_reverseAction, Constants::Actions::SortReverse);
    reverseCmd->setCategories(sortCategory);
    reverseCmd->setAttribute(ProxyAction::UpdateText);
    sortMenu->addAction(reverseCmd, Actions::Groups::One);

    QObject::connect(m_sortingRegistry, &RegistryBase::itemAdded, this, &EditMenu::refreshSortActions);
    QObject::connect(m_sortingRegistry, &RegistryBase::itemChanged, this, &EditMenu::refreshSortActions);
    QObject::connect(m_sortingRegistry, &RegistryBase::itemRemoved, this, &EditMenu::refreshSortActions);
    refreshSortActions();

    auto* search = new QAction(tr("S&earch"), this);
    search->setStatusTip(tr("Search the current playlist"));
    auto* searchCommand = actionManager->registerAction(search, Constants::Actions::SearchPlaylist);
    searchCommand->setCategories(editCategory);
    searchCommand->setDefaultShortcut({{QKeySequence::Find}, {QKeySequence{Qt::Key_F3}}});
    editMenu->addAction(searchCommand, Actions::Groups::Three);
    QObject::connect(search, &QAction::triggered, this, &EditMenu::requestSearch);

    auto* openSettings = new QAction(tr("&Settings"), this);
    Gui::setThemeIcon(openSettings, Constants::Icons::Settings);
    openSettings->setStatusTip(tr("Open the settings dialog"));
    auto* settingsCommand = actionManager->registerAction(openSettings, Constants::Actions::Settings);
    settingsCommand->setCategories(editCategory);
    settingsCommand->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_P});
    editMenu->addSeparator(Actions::Groups::Three);
    editMenu->addAction(settingsCommand, Actions::Groups::Three);
    QObject::connect(openSettings, &QAction::triggered, m_settings->settingsDialog(), &SettingsDialogController::open);
}

void EditMenu::refreshSortActions()
{
    const auto sortActionId = [](int presetId) {
        return Id{u"Edit.Sort.Preset.%1"_s.arg(presetId)};
    };
    const auto sortActionText = [](const QString& presetName) {
        return tr("Sort by %1").arg(presetName);
    };

    auto* sortMenu                 = m_actionManager->actionContainer(Constants::Menus::EditSort);
    const QStringList sortCategory = {tr("Edit"), tr("Sort")};
    const auto presets             = m_sortingRegistry->items();

    std::erase_if(m_sortPresetActions, [this, &presets, sortActionId](const auto& presetAction) {
        const bool removed = std::ranges::none_of(
            presets, [presetId = presetAction.presetId](const auto& preset) { return preset.id == presetId; });
        if(removed && presetAction.action) {
            m_actionManager->unregisterAction(presetAction.action, sortActionId(presetAction.presetId));
            presetAction.action->deleteLater();
        }
        return removed;
    });

    for(const auto& preset : presets) {
        const auto actionIt = std::ranges::find_if(
            m_sortPresetActions, [preset](const auto& action) { return action.presetId == preset.id; });
        if(actionIt != m_sortPresetActions.cend()) {
            if(actionIt->action) {
                actionIt->action->setText(sortActionText(preset.name));
            }
            continue;
        }

        auto* presetAction = new QAction(sortActionText(preset.name), sortMenu);
        presetAction->setEnabled(false);

        const Id actionId = sortActionId(preset.id);
        auto* presetCmd   = m_actionManager->registerAction(presetAction, actionId);
        presetCmd->setCategories(sortCategory);
        presetCmd->setAttribute(ProxyAction::UpdateText);
        sortMenu->addAction(presetCmd, Actions::Groups::Two);

        m_sortPresetActions.emplace_back(preset.id, presetAction);
    }
}
} // namespace Fooyin

#include "moc_editmenu.cpp"
