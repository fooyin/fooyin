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

#include "sortactionhandler.h"

#include "utils/utils.h"

#include <core/library/sortingregistry.h>
#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/id.h>

#include <QAction>
#include <QMenu>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
Id sortPresetActionId(int presetId)
{
    return Id{u"Edit.Sort.Preset.%1"_s.arg(presetId)};
}
} // namespace

SortActionHandler::SortActionHandler(ActionManager* actionManager, SortingRegistry* sortingRegistry,
                                     Context actionContext, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_sortingRegistry{sortingRegistry}
    , m_actionContext{std::move(actionContext)}
    , m_randomiseCmd{nullptr}
    , m_reverseCmd{nullptr}
    , m_scope{SortScope::All}
{
    m_actionContext.erase(Constants::Context::TrackSelection);
}

SortActionHandler::~SortActionHandler()
{
    unregisterPresetActions();
}

Command* SortActionHandler::registerRandomiseAction(QAction* action, const QString& statusTip)
{
    action->setStatusTip(statusTip);
    m_randomiseCmd = m_actionManager->registerAction(action, Constants::Actions::SortRandomise, m_actionContext);
    m_randomiseCmd->setCategories({tr("Edit"), tr("Sort")});
    m_randomiseCmd->setAttribute(ProxyAction::UpdateText);
    QObject::connect(action, &QAction::triggered, this, [this]() { Q_EMIT randomiseRequested(m_scope); });
    return m_randomiseCmd;
}

Command* SortActionHandler::registerReverseAction(QAction* action, const QString& statusTip)
{
    action->setStatusTip(statusTip);
    m_reverseCmd = m_actionManager->registerAction(action, Constants::Actions::SortReverse, m_actionContext);
    m_reverseCmd->setCategories({tr("Edit"), tr("Sort")});
    m_reverseCmd->setAttribute(ProxyAction::UpdateText);
    QObject::connect(action, &QAction::triggered, this, [this]() { Q_EMIT reverseRequested(m_scope); });
    return m_reverseCmd;
}

void SortActionHandler::refreshPresetActions(const QString& statusTip)
{
    unregisterPresetActions();

    const auto presets = m_sortingRegistry->items();
    for(const auto& preset : presets) {
        //: %1 refers to the name of a sorting preset e.g. "Sort by Album"
        const QString title = tr("Sort by %1").arg(preset.name);
        auto* presetAction  = new QAction(title, this);
        presetAction->setStatusTip(statusTip);

        const Id actionId = sortPresetActionId(preset.id);
        auto* presetCmd   = m_actionManager->registerAction(presetAction, actionId, m_actionContext);
        presetCmd->setCategories({tr("Edit"), tr("Sort")});
        presetCmd->setAttribute(ProxyAction::UpdateText);

        QObject::connect(presetAction, &QAction::triggered, this, [this, presetId = preset.id]() {
            if(const auto sortPreset = m_sortingRegistry->itemById(presetId)) {
                Q_EMIT sortPresetRequested(sortPreset->script, m_scope);
            }
        });

        m_sortPresetActions.push_back({.presetId = preset.id, .action = presetAction, .command = presetCmd});
    }
}

void SortActionHandler::addSortMenu(QMenu* parent, bool disabled, SortScope scope)
{
    auto* sortMenu = new QMenu(tr("Sort"), parent);

    if(disabled) {
        sortMenu->setEnabled(false);
        parent->addMenu(sortMenu);
        return;
    }

    QObject::connect(sortMenu, &QMenu::aboutToShow, this, [this, scope]() { m_scope = scope; });
    QObject::connect(sortMenu, &QMenu::aboutToHide, this, [this]() { m_scope = SortScope::All; }, Qt::QueuedConnection);

    Utils::forwardMenuStatusTips(sortMenu);

    if(m_randomiseCmd) {
        sortMenu->addAction(m_randomiseCmd->action());
    }
    if(m_reverseCmd) {
        sortMenu->addAction(m_reverseCmd->action());
    }

    if(!m_sortPresetActions.empty()) {
        sortMenu->addSeparator();
    }

    for(const auto& presetAction : m_sortPresetActions) {
        if(presetAction.command) {
            sortMenu->addAction(presetAction.command->action());
        }
    }

    auto* moreSettings = new QAction(tr("More…"), sortMenu);
    moreSettings->setStatusTip(tr("Open the sorting presets settings"));
    QObject::connect(moreSettings, &QAction::triggered, this, &SortActionHandler::settingsRequested);

    sortMenu->addSeparator();
    sortMenu->addAction(moreSettings);

    parent->addMenu(sortMenu);
}

void SortActionHandler::setEnabled(bool enabled) const
{
    for(const auto& presetAction : m_sortPresetActions) {
        if(presetAction.action) {
            presetAction.action->setEnabled(enabled);
        }
    }
}

void SortActionHandler::unregisterPresetActions()
{
    for(const auto& presetAction : m_sortPresetActions) {
        QAction* action = presetAction.action;
        if(!action) {
            continue;
        }

        m_actionManager->unregisterAction(action, sortPresetActionId(presetAction.presetId), m_actionContext);
        action->deleteLater();
    }
    m_sortPresetActions.clear();
}
} // namespace Fooyin

#include "moc_sortactionhandler.cpp"
