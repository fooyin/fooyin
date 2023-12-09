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

#include "fyutils_export.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace Fooyin {
class FYUTILS_EXPORT OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    enum Option
    {
        None        = 0x0,
        Label       = 0x2,
        Button      = 0x4,
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit OverlayWidget(QWidget* parent = nullptr);
    explicit OverlayWidget(const Options& options, QWidget* parent = nullptr);

    void setText(const QString& text);
    void setButtonText(const QString& text);

    void setColour(const QColor& colour);
    void resetColour();

signals:
    void buttonClicked();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Options m_options;
    QColor m_colour;
    QLabel* m_text;
    QPushButton* m_button;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::OverlayWidget::Options)
