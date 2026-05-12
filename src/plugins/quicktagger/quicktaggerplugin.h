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

#pragma once

#include "quicktagger.h"

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <utils/id.h>

#include <QPointer>

#include <unordered_map>

class QAction;
class QMenu;

namespace Fooyin {
class ActionManager;
class AudioLoader;
class MusicLibrary;
class SettingsManager;
class TrackSelectionController;

namespace QuickTagger {
class QuickTaggerPage;

class QuickTaggerPlugin : public QObject,
                          public Plugin,
                          public CorePlugin,
                          public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "quicktagger.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

private:
    void registerQuickTaggerActions();
    void renderQuickTaggerMenu(QMenu* menu, const TrackList& tracks);

    void runQuickTagAction(const Id& id);
    void runRemoveQuickTagAction(const Id& id);
    void runQuickTag(const QuickTag& tag, const QString& value);
    void runQuickTag(const QuickTag& tag, const QString& value, const TrackList& tracks);
    void runRemoveQuickTag(const QuickTag& tag);
    void applyQuickTag(const QuickTag& tag, const QString& value, TrackList tracks);
    void removeQuickTag(const QuickTag& tag, TrackList tracks);
    bool confirmQuickTag(const QString& field, int trackCount, int threshold);

    [[nodiscard]] QAction* registeredAction(const Id& id) const;
    void syncRegisteredAction(const Id& id, const QString& text, bool active);

    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    std::shared_ptr<AudioLoader> m_audioLoader;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    QuickTaggerPage* m_page;
    std::unordered_map<Id, QPointer<QAction>, Id::IdHash> m_quickTaggerActions;
};
} // namespace QuickTagger
} // namespace Fooyin
