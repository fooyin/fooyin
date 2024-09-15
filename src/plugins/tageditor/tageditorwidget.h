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

#include <core/track.h>
#include <gui/fywidget.h>
#include <gui/propertiesdialog.h>
#include <gui/widgets/extendabletableview.h>

#include <QWidget>

class QToolButton;

namespace Fooyin {
class ActionManager;
class MultiLineEditDelegate;
class SettingsManager;
class StarDelegate;
class WidgetContext;

namespace TagEditor {
class TagEditorFieldRegistry;
class TagEditorModel;
class TagEditorView;

class TagEditorWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(ActionManager* actionManager, TagEditorFieldRegistry* registry, SettingsManager* settings,
                             QWidget* parent = nullptr);
    ~TagEditorWidget() override;

    void setTracks(const TrackList& tracks);
    void setReadOnly(bool readOnly);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    [[nodiscard]] bool hasTools() const override;
    void addTools(QMenu* menu) override;
    void apply() override;

signals:
    void trackMetadataChanged(const Fooyin::TrackList& tracks);
    void trackStatsChanged(const Fooyin::TrackList& tracks);

private:
    void saveState() const;
    void restoreState() const;

    TagEditorFieldRegistry* m_registry;
    SettingsManager* m_settings;

    bool m_readOnly;
    TagEditorView* m_view;
    TagEditorModel* m_model;

    std::set<int> m_delegateRows;
    MultiLineEditDelegate* m_multilineDelegate;
    StarDelegate* m_starDelegate;

    QAction* m_autoTrackNum;
    QAction* m_changeFields;
};
} // namespace TagEditor
} // namespace Fooyin
