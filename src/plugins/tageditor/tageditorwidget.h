/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fywidget.h>
#include <gui/propertiesdialog.h>

#include <QWidget>

class QHBoxLayout;
class QTableView;

namespace Fy {

namespace Utils {
class ExtendableTableView;
class SettingsManager;
}

namespace Core::Library {
class MusicLibrary;
}

namespace Gui {
class TrackSelectionController;
}

namespace TagEditor {
class TagEditorModel;

class TagEditorWidget : public Gui::PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(Gui::TrackSelectionController* trackSelection, Core::Library::MusicLibrary* library,
                             Utils::SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void apply() override;

private:
    Gui::TrackSelectionController* m_trackSelection;
    Core::Library::MusicLibrary* m_library;
    Utils::SettingsManager* m_settings;

    Utils::ExtendableTableView* m_view;
    TagEditorModel* m_model;
};
} // namespace TagEditor
} // namespace Fy
