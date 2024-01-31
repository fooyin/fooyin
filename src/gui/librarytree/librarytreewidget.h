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

#include "gui/fywidget.h"

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
class TrackSelectionController;
class LibraryTreeWidgetPrivate;

class LibraryTreeWidget : public FyWidget
{
    Q_OBJECT

public:
    LibraryTreeWidget(MusicLibrary* library, TrackSelectionController* trackSelection, SettingsManager* settings,
                      QWidget* parent = nullptr);

    QString name() const override;
    QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void searchEvent(const QString& search) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    std::unique_ptr<LibraryTreeWidgetPrivate> p;
};
} // namespace Fooyin
