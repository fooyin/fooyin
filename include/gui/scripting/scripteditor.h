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

#include "fygui_export.h"

#include <QDialog>

namespace Fooyin {
class LibraryManager;
class TrackSelectionController;
class ScriptEditorPrivate;

class FYGUI_EXPORT ScriptEditor : public QDialog
{
    Q_OBJECT

public:
    ScriptEditor(LibraryManager* libraryManager, TrackSelectionController* trackSelection, QWidget* parent = nullptr);
    explicit ScriptEditor(const QString& script, QWidget* parent = nullptr);
    explicit ScriptEditor(QWidget* parent = nullptr);
    ~ScriptEditor() override;

    static void openEditor(const QString& script, const std::function<void(const QString&)>& callback);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<ScriptEditorPrivate> p;
};
} // namespace Fooyin
