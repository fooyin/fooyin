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

#include "clickablelabel.h"

#include <QPixmap>
#include <QWidget>

class QVBoxLayout;

namespace Utils {
class ComboIcon : public QWidget
{
    Q_OBJECT

    struct Icon
    {
        QPixmap icon;
        QPixmap iconActive;
        QPixmap iconDisabled;
    };

    using PathIconPair      = std::pair<QString, Icon>;
    using PathIconContainer = std::vector<PathIconPair>;

public:
    enum Attribute
    {
        HasActiveIcon   = 1,
        HasDisabledIcon = 2,
        AutoShift       = 4,
        Active          = 6,
        Enabled         = 8,
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    explicit ComboIcon(const QString& path, Attributes attributes, QWidget* parent = nullptr);
    explicit ComboIcon(const QString& path, QWidget* parent = nullptr);
    ~ComboIcon() override = default;

    void setup(const QString& path);

    void addPixmap(const QString& path, const QPixmap& icon);
    void addPixmap(const QString& path);

    void setIcon(const QString& path, bool active = false);
    void setIconEnabled(bool enable = true);

signals:
    void clicked(const QString& path = {});
    void entered();

protected:
    void changeEvent(QEvent* event) override;

private:
    void labelClicked();
    bool hasAttribute(Attribute attribute);
    void addAttribute(Attribute attribute);
    void removeAttribute(Attribute attribute);

    QVBoxLayout* m_layout;
    ClickableLabel* m_label;
    Attributes m_attributes;
    int m_currentIndex;
    PathIconContainer m_icons;
};
} // namespace Utils
