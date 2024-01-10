/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/trackfwd.h>
#include <gui/fywidget.h>
#include <gui/propertiesdialog.h>
#include <utils/extendabletableview.h>

#include <QWidget>

namespace Fooyin {
class ActionManager;
class SettingsManager;
class TrackSelectionController;

namespace TagEditor {
class TagEditorView : public ExtendableTableView
{
    Q_OBJECT

public:
    explicit TagEditorView(ActionManager* actionManager, QWidget* parent = nullptr);

    void handleNewRow() override;

    [[nodiscard]] int sizeHintForRow(int row) const override;
};

class TagEditorWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(ActionManager* actionManager, TrackSelectionController* trackSelection,
                             SettingsManager* settings, QWidget* parent = nullptr);
    ~TagEditorWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void apply() override;

signals:
    void trackMetadataChanged(const TrackList& tracks);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace TagEditor
} // namespace Fooyin
