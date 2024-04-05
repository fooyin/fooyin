/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "gui/fywidget.h"

class QLabel;

namespace Fooyin {
class Dummy : public FyWidget
{
    Q_OBJECT

public:
    explicit Dummy(QWidget* parent = nullptr);
    explicit Dummy(QString name, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void layoutEditingChanged(bool enabled) override;

    [[nodiscard]] QString missingName() const;

private:
    void updateText();

    bool m_editing;
    QString m_missingName;
    QLabel* m_label;
};
} // namespace Fooyin
