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

#include <QWidget>

namespace Fooyin {
class PlayerController;
class SeekContainerPrivate;

class FYGUI_EXPORT SeekContainer : public QWidget
{
    Q_OBJECT

public:
    explicit SeekContainer(PlayerController* playerController, QWidget* parent = nullptr);
    ~SeekContainer() override;

    void insertWidget(int index, QWidget* widget);

    [[nodiscard]] bool labelsEnabled() const;
    [[nodiscard]] bool elapsedTotal() const;

    void setLabelsEnabled(bool enabled);
    void setElapsedTotal(bool enabled);

signals:
    void elapsedClicked();
    void totalClicked();

protected:
    void showEvent(QShowEvent* event) override;

private:
    std::unique_ptr<SeekContainerPrivate> p;
};
} // namespace Fooyin
