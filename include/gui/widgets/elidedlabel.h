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

#include "fygui_export.h"

#include <QLabel>

namespace Fooyin {
class FYGUI_EXPORT ElidedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ElidedLabel(QWidget* parent = nullptr);
    explicit ElidedLabel(Qt::TextElideMode elideMode, QWidget* parent = nullptr);
    ElidedLabel(QString text, Qt::TextElideMode elideMode, QWidget* parent = nullptr);

    [[nodiscard]] Qt::TextElideMode elideMode() const;
    void setElideMode(Qt::TextElideMode elideMode);

    [[nodiscard]] QString text() const;
    void setText(const QString& text);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void elideText(int width);

    Qt::TextElideMode m_elideMode;
    QString m_text;
    bool m_isElided;
};
} // namespace Fooyin
