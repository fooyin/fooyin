/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <QBasicTimer>
#include <QPointer>
#include <QWidget>

namespace Fooyin {
class FySplitterHandle;

class SplitterGrabOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit SplitterGrabOverlay(FySplitterHandle* handle);
    ~SplitterGrabOverlay() override;

    void showForHandle();

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;
    bool event(QEvent* event) override;

private:
    QPointer<FySplitterHandle> m_handle;
    QBasicTimer m_timer;
};
} // namespace Fooyin