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
#include "core/typedefs.h"

#include <QPixmap>
#include <QWidget>

class QVBoxLayout;

namespace Gui::Widgets {
class ComboIcon : public QWidget
{
    Q_OBJECT

    struct Icon
    {
        QPixmap icon;
        QPixmap iconActive;
    };

    using PathIconPair      = std::pair<QString, Icon>;
    using PathIconContainer = std::vector<PathIconPair>;

public:
    explicit ComboIcon(const QString& path, Core::Fy::Attributes attributes, QWidget* parent = nullptr);
    explicit ComboIcon(const QString& path, QWidget* parent = nullptr);
    ~ComboIcon() override;

    void setup(const QString& path);

    bool hasAttribute(Core::Fy::Attribute attribute);
    void addAttribute(Core::Fy::Attribute attribute);
    void removeAttribute(Core::Fy::Attribute attribute);

    void addPixmap(const QString& path, const QPixmap& icon);
    void addPixmap(const QString& path);

    void setIcon(const QString& path, bool active = false);

signals:
    void clicked(const QString& path = {});
    void entered();

protected:
    void labelClicked();

private:
    QVBoxLayout* m_layout;
    ClickableLabel* m_label;
    Core::Fy::Attributes m_attributes;
    int m_currentIndex;
    PathIconContainer m_icons;
};
} // namespace Gui::Widgets
