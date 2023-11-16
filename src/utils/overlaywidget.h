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

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;

namespace Fooyin {
class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(bool button = false, QWidget* parent = nullptr);
    ~OverlayWidget() override = default;

    void setText(const QString& text);
    void setButtonText(const QString& text);

signals:
    void settingsClicked();

private:
    QVBoxLayout* m_layout;
    QLabel* m_text;
    QPushButton* m_button;
};
} // namespace Fooyin
