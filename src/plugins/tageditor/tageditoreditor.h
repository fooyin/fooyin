/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "tageditorfield.h"

#include <core/track.h>

#include <QString>
#include <QWidget>

namespace Fooyin {
class ActionManager;
class AutoHeaderView;
class MultiLineEditDelegate;
class SettingsManager;
class StarDelegate;
class ToolButton;

namespace TagEditor {
class TagEditorFieldRegistry;
class TagEditorAutocompleteDelegate;
class TagEditorModel;
class TagEditorView;
struct FillValuesResult;

class TagEditorEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TagEditorEditor(ActionManager* actionManager, TagEditorFieldRegistry* registry, SettingsManager* settings,
                             QWidget* parent = nullptr);

    void setTracks(const TrackList& tracks);
    void setReadOnly(bool readOnly);
    [[nodiscard]] bool hasChanges() const;
    [[nodiscard]] bool hasOnlyStatChanges() const;
    [[nodiscard]] TrackList tracks() const;
    TrackList applyChanges();

    void addTool(QWidget* widget);

    [[nodiscard]] AutoHeaderView* header() const;

Q_SIGNALS:
    void pendingChangesStateChanged();
    void autoFillValuesRequested();

private:
    void configureDelegates(const std::vector<TagEditorField>& items);
    void refreshModel();
    void updatePendingScopeState();

    TagEditorFieldRegistry* m_registry;
    SettingsManager* m_settings;

    bool m_readOnly;
    bool m_firstReset;
    TagEditorView* m_view;
    TagEditorModel* m_model;
    AutoHeaderView* m_header;

    TrackList m_tracks;

    std::set<int> m_delegateRows;
    TagEditorAutocompleteDelegate* m_autocompleteDelegate;
    MultiLineEditDelegate* m_multilineDelegate;
    StarDelegate* m_starDelegate;

    ToolButton* m_toolsButton;
    QAction* m_autoTrackNum;
    QAction* m_autoFillValuesAction;
    QAction* m_changeFields;
};
} // namespace TagEditor
} // namespace Fooyin
