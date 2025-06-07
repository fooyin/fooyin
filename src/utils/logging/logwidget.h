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

#include "fyutils_export.h"

#include <QWidget>

class QComboBox;
class QTreeView;

namespace Fooyin {
class LogModel;
class SettingsManager;

class FYUTILS_EXPORT LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(SettingsManager* settings, QWidget* parent = nullptr);

    void addEntry(const QString& message, QtMsgType type);

    [[nodiscard]] QSize sizeHint() const override;

private:
    void saveLog();

    SettingsManager* m_settings;
    QTreeView* m_view;
    LogModel* m_model;
    QComboBox* m_level;
    bool m_scrollIsAtBottom;
};
} // namespace Fooyin
