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
#include <utils/extendabletableview.h>

#include <QWidget>

class QToolButton;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class WidgetContext;

namespace TagEditor {
class TagEditorModel;

class TagEditorView : public ExtendableTableView
{
    Q_OBJECT

public:
    explicit TagEditorView(ActionManager* actionManager, QWidget* parent = nullptr);

    void setTagEditTriggers(EditTrigger triggers);
    void setupActions();

    [[nodiscard]] int sizeHintForRow(int row) const override;

protected:
    void setupContextActions(QMenu* menu, const QPoint& pos) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void copySelection();
    void pasteSelection(bool match);

    ActionManager* m_actionManager;

    EditTrigger m_editTrigger;
    WidgetContext* m_context;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_pasteFields;
};

class TagEditorWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(const TrackList& tracks, bool readOnly, ActionManager* actionManager,
                             SettingsManager* settings, QWidget* parent = nullptr);
    ~TagEditorWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void apply() override;

signals:
    void trackMetadataChanged(const Fooyin::TrackList& tracks);
    void trackStatsChanged(const Fooyin::TrackList& tracks);

private:
    void setupToolsMenu();
    void saveState() const;
    void restoreState() const;

    SettingsManager* m_settings;
    TagEditorView* m_view;
    TagEditorModel* m_model;
    QToolButton* m_toolsButton;
    QMenu* m_toolsMenu;
};
} // namespace TagEditor
} // namespace Fooyin
