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
        None       = 0x0,
        Label      = 0x1,
        Button     = 0x2,
        Resize     = 0x4,
        Static     = 0x8,
        Selectable = 0x10,
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit OverlayWidget(QWidget* parent = nullptr);
    explicit OverlayWidget(const Options& options, QWidget* parent = nullptr);
    ~OverlayWidget() override;

    void setOption(Option option, bool on = true);

    void setText(const QString& text);
    void setButtonText(const QString& text);
    [[nodiscard]] QPushButton* button() const;
    [[nodiscard]] QLabel* label() const;

    void connectOverlay(OverlayWidget* other);
    void disconnectOverlay(OverlayWidget* other);

    void addWidget(QWidget* widget);

    [[nodiscard]] QColor colour() const;
    void setColour(const QColor& colour);
    void resetColour();

    void select();
    void deselect();

    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void clicked();
    void entered();
    void left();

protected:
    void showEvent(QShowEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::OverlayWidget::Options)
