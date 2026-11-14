/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "logmodel.h"

#include <QBasicTimer>
#include <QDialog>

#include <queue>

class QCheckBox;
class QComboBox;
class QSettings;

namespace Fooyin {
class LogModel;
class ExpandedTreeView;
class SettingsManager;
class SignalThrottler;

class LogWidget : public QDialog
{
    Q_OBJECT

public:
    explicit LogWidget(SettingsManager* settings, QWidget* parent = nullptr);

    void addEntry(const QString& message, QtMsgType type);

    void saveState(QSettings& settings) const;
    void restoreState(const QSettings& settings);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void copySelectedRows() const;
    void copyRows(const QModelIndexList& rows) const;
    void showContextMenu(const QPoint& pos);
    void saveLog();

    SettingsManager* m_settings;

    ExpandedTreeView* m_view;
    LogModel* m_model;
    QComboBox* m_level;
    QCheckBox* m_wordWrap;
    SignalThrottler* m_scrollThrottler;

    bool m_scrollIsAtBottom;

    std::queue<ConsoleEntry> m_queue;
    QBasicTimer m_flushTimer;
};
} // namespace Fooyin
