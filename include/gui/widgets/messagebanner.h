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

#include "fygui_export.h"

#include <QWidget>

class QLabel;
class QToolButton;

namespace Fooyin {
class FYGUI_EXPORT MessageBanner : public QWidget
{
    Q_OBJECT

public:
    explicit MessageBanner(QWidget* parent = nullptr);

    void setText(const QString& text);
    void clear();

    [[nodiscard]] QString text() const;

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void closed();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updatePosition();
    void scheduleUpdatePosition();

    QLabel* m_label;
    QToolButton* m_closeButton;
    bool m_updatingPosition;
};
} // namespace Fooyin
