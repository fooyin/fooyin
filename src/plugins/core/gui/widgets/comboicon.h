/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "clickablelabel.h"

#include <QPixmap>
#include <QWidget>

class QVBoxLayout;

class ComboIcon : public QWidget
{
    Q_OBJECT

    struct Icon
    {
        QPixmap icon;
        QPixmap iconActive;
    };

    using PathIconPair = std::pair<QString, Icon>;
    using PathIconContainer = std::vector<PathIconPair>;

public:
    explicit ComboIcon(const QString& path, bool autoShift, QWidget* parent = nullptr);
    explicit ComboIcon(const QString& path, QWidget* parent = nullptr);
    ~ComboIcon() override;

    void setup(const QString& name);

    void addPixmap(const QString& path, const QPixmap& icon);
    void addPixmap(const QString& path);

    void setIcon(const QString& path, bool active = false);

signals:
    void clicked(const QString& path = {});

protected:
    void labelClicked();

private:
    QVBoxLayout* m_layout;
    ClickableLabel* m_label;
    bool m_autoShift;
    bool m_active;
    int m_currentIndex;
    PathIconContainer m_icons;
};
