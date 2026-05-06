/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <memory>

class QJsonObject;
class QPushButton;

namespace Fooyin {
class ActionManager;
class AudioLoader;
class MusicLibrary;
class SettingsManager;
class TrackSelectionController;

namespace TagEditor {
class TagEditorFieldRegistry;
class TagEditorEditor;

class TagEditorPanel : public FyWidget
{
    Q_OBJECT

public:
    explicit TagEditorPanel(ActionManager* actionManager, TagEditorFieldRegistry* registry, MusicLibrary* library,
                            std::shared_ptr<AudioLoader> audioLoader, TrackSelectionController* selectionController,
                            SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

private:
    bool apply();
    void selectionChanged();
    void updateForTracks(const TrackList& tracks);

    MusicLibrary* m_library;
    std::shared_ptr<AudioLoader> m_audioLoader;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;
    TagEditorEditor* m_editor;
    QPushButton* m_applyButton;
    TrackList m_currentTracks;
};
} // namespace TagEditor
} // namespace Fooyin
