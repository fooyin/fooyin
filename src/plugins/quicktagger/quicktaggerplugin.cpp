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

#include "quicktaggerplugin.h"

#include "quicktagger.h"
#include "quicktaggerconstants.h"
#include "quicktaggerpage.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/scripting/scripttrackwriter.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Fooyin::QuickTagger {
namespace {
QString actionIdPart(QString text)
{
    text = text.trimmed();
    text.replace(QRegularExpression{u"[^A-Za-z0-9]+"_s}, u"_"_s);
    text = text.remove(QRegularExpression{u"^_+|_+$"_s});
    return text.isEmpty() ? u"Empty"_s : text;
}

Id actionId(const QuickTag& tag, const QString& value)
{
    return Id{u"Tagging.QuickTagger.%1.%2"_s.arg(actionIdPart(tag.id), actionIdPart(value))};
}

Id removeActionId(const QuickTag& tag)
{
    return Id{u"Tagging.QuickTagger.%1.Remove"_s.arg(actionIdPart(tag.id))};
}

bool canWriteTracks(const TrackList& tracks, const std::shared_ptr<AudioLoader>& audioLoader)
{
    return !tracks.empty() && std::ranges::all_of(tracks, [&audioLoader](const Track& track) {
        return !track.hasCue() && !track.isInArchive() && audioLoader->canWriteMetadata(track);
    });
}

QStringList actionCategories(const QuickTag& tag, const QuickTaggerPlugin* plugin)
{
    return {plugin->tr("Tagging"), plugin->tr("Quick Tagger"), quickTagDisplayName(tag)};
}
} // namespace

void QuickTaggerPlugin::initialise(const CorePluginContext& context)
{
    m_settings    = context.settingsManager;
    m_library     = context.library;
    m_audioLoader = context.audioLoader;

    m_settings->createSetting<Settings::QuickTagger::Fields>(defaultQuickTagsJson(), u"QuickTagger/Fields"_s);
    m_settings->createSetting<Settings::QuickTagger::ConfirmationThreshold>(1, u"QuickTagger/ConfirmationThreshold"_s);

    const QByteArray rawTags = m_settings->value<Settings::QuickTagger::Fields>().trimmed();
    const auto tags          = rawTags.isEmpty() ? defaultQuickTags() : quickTags(*m_settings);
    if(!tags.empty()) {
        setQuickTags(*m_settings, tags);
    }
}

void QuickTaggerPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_trackSelection = context.trackSelection;

    m_page = new QuickTaggerPage(m_settings, this);

    m_trackSelection->registerTrackContextDynamicSubmenu(
        this, TrackContextMenuArea::Track, Fooyin::Constants::Menus::Context::Tagging, Constants::Menus::QuickTagger,
        tr("Quick Tagger"),
        [this](QMenu* menu, const TrackSelection& selection) { renderQuickTaggerMenu(menu, selection.tracks); });

    registerQuickTaggerActions();
    m_settings->subscribe<Settings::QuickTagger::Fields>(this, &QuickTaggerPlugin::registerQuickTaggerActions);
}

void QuickTaggerPlugin::registerQuickTaggerActions()
{
    const auto tags = quickTags(*m_settings);
    IdSet activeIds;

    for(const QuickTag& tag : tags) {
        for(const auto& value : tag.values) {
            const Id id = actionId(tag, value.id);
            activeIds.emplace(id);

            if(QAction* action = registeredAction(id)) {
                syncRegisteredAction(id, tr("Set %1 to %2").arg(quickTagDisplayName(tag), value.value), true);
                if(Command* command = m_actionManager->command(id)) {
                    command->setCategories(actionCategories(tag, this));
                }
            }
            else {
                action = new QAction(tr("Set %1 to %2").arg(quickTagDisplayName(tag), value.value), this);
                QObject::connect(action, &QAction::triggered, this, [this, id] { runQuickTagAction(id); });
                Command* command = m_actionManager->registerAction(action, id);
                command->setDescription(action->text());
                command->setCategories(actionCategories(tag, this));
                m_quickTaggerActions[id] = action;
            }
        }

        const Id id = removeActionId(tag);
        activeIds.emplace(id);

        if(QAction* action = registeredAction(id)) {
            syncRegisteredAction(id, tr("Remove %1").arg(quickTagDisplayName(tag)), true);
            if(Command* command = m_actionManager->command(id)) {
                command->setCategories(actionCategories(tag, this));
            }
        }
        else {
            action = new QAction(tr("Remove %1").arg(quickTagDisplayName(tag)), this);
            QObject::connect(action, &QAction::triggered, this, [this, id] { runRemoveQuickTagAction(id); });
            Command* command = m_actionManager->registerAction(action, id);
            command->setDescription(action->text());
            command->setCategories(actionCategories(tag, this));
            m_quickTaggerActions[id] = action;
        }
    }

    for(const auto& [id, action] : m_quickTaggerActions) {
        if(!activeIds.contains(id)) {
            syncRegisteredAction(id, action ? action->text() : QString{}, false);
        }
    }

    Q_EMIT m_actionManager->commandsChanged();
}

void QuickTaggerPlugin::renderQuickTaggerMenu(QMenu* menu, const TrackList& tracks)
{
    if(!canWriteTracks(tracks, m_audioLoader)) {
        return;
    }

    registerQuickTaggerActions();

    const auto tags = quickTags(*m_settings);

    for(const QuickTag& tag : tags) {
        auto* fieldMenu = new QMenu(quickTagDisplayName(tag), menu);

        for(const auto& value : tag.values) {
            auto* action = new QAction(value.value, fieldMenu);
            QObject::connect(action, &QAction::triggered, fieldMenu,
                             [this, tag, value, tracks] { runQuickTag(tag, value.value, tracks); });
            fieldMenu->addAction(action);
        }

        fieldMenu->addSeparator();

        auto* removeAction = new QAction(tr("Remove %1").arg(quickTagDisplayName(tag)), fieldMenu);
        QObject::connect(removeAction, &QAction::triggered, fieldMenu, [this, tag, tracks] {
            if(confirmQuickTag(quickTagDisplayName(tag), static_cast<int>(tracks.size()),
                               quickTaggerConfirmationThreshold(*m_settings))) {
                removeQuickTag(tag, tracks);
            }
        });
        fieldMenu->addAction(removeAction);

        menu->addMenu(fieldMenu);
    }
}

void QuickTaggerPlugin::runQuickTagAction(const Id& id)
{
    const auto tags = quickTags(*m_settings);
    for(const QuickTag& tag : tags) {
        for(const auto& value : tag.values) {
            if(actionId(tag, value.id) == id) {
                runQuickTag(tag, value.value);
                return;
            }
        }
    }
}

void QuickTaggerPlugin::runRemoveQuickTagAction(const Id& id)
{
    const auto tags = quickTags(*m_settings);
    for(const QuickTag& tag : tags) {
        if(removeActionId(tag) == id) {
            runRemoveQuickTag(tag);
            return;
        }
    }
}

void QuickTaggerPlugin::runQuickTag(const QuickTag& tag, const QString& value)
{
    const auto* selection = m_trackSelection->selectedSelection();
    if(!selection || !canWriteTracks(selection->tracks, m_audioLoader)) {
        return;
    }
    runQuickTag(tag, value, selection->tracks);
}

void QuickTaggerPlugin::runQuickTag(const QuickTag& tag, const QString& value, const TrackList& tracks)
{
    if(!confirmQuickTag(quickTagDisplayName(tag), static_cast<int>(tracks.size()),
                        quickTaggerConfirmationThreshold(*m_settings))) {
        return;
    }
    applyQuickTag(tag, value, tracks);
}

void QuickTaggerPlugin::runRemoveQuickTag(const QuickTag& tag)
{
    const auto* selection = m_trackSelection->selectedSelection();
    if(!selection || !canWriteTracks(selection->tracks, m_audioLoader)) {
        return;
    }

    if(!confirmQuickTag(quickTagDisplayName(tag), static_cast<int>(selection->tracks.size()),
                        quickTaggerConfirmationThreshold(*m_settings))) {
        return;
    }

    removeQuickTag(tag, selection->tracks);
}

void QuickTaggerPlugin::applyQuickTag(const QuickTag& tag, const QString& value, TrackList tracks)
{
    for(Track& track : tracks) {
        setTrackScriptValue(tag.field, value, track);
    }

    m_library->writeTrackMetadata(tracks);
}

void QuickTaggerPlugin::removeQuickTag(const QuickTag& tag, TrackList tracks)
{
    const QString field = tag.field.toUpper();

    for(Track& track : tracks) {
        track.removeExtraTag(field);
        setTrackScriptValue(tag.field, QString{}, track);
    }

    m_library->writeTrackMetadata(tracks);
}

bool QuickTaggerPlugin::confirmQuickTag(const QString& field, int trackCount, int threshold)
{
    if(threshold <= 0 || trackCount <= threshold) {
        return true;
    }

    const auto response = QMessageBox::question(Utils::getMainWindow(), tr("Quick Tagger"),
                                                //: %1 is the Quick Tagger entry name, for example "Rating".
                                                tr("Set \"%1\" on %Ln track(s)?", nullptr, trackCount).arg(field));
    return response == QMessageBox::Yes;
}

QAction* QuickTaggerPlugin::registeredAction(const Id& id) const
{
    if(const auto it = m_quickTaggerActions.find(id); it != m_quickTaggerActions.cend()) {
        return it->second;
    }

    if(const auto* command = m_actionManager->command(id)) {
        return command->action();
    }

    return nullptr;
}

void QuickTaggerPlugin::syncRegisteredAction(const Id& id, const QString& text, bool active)
{
    QAction* action = registeredAction(id);
    if(!action) {
        return;
    }

    if(!text.isEmpty()) {
        action->setText(text);
        if(Command* command = m_actionManager->command(id)) {
            command->setDescription(text);
        }
    }

    action->setEnabled(active);
    action->setVisible(active);

    m_quickTaggerActions[id] = action;
}
} // namespace Fooyin::QuickTagger

#include "moc_quicktaggerplugin.cpp"
