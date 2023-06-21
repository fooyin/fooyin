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

#include "presetinput.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class QGridLayout;
class QVBoxLayout;

namespace Fy::Gui::Settings {
class PresetInputBox : public QWidget
{
public:
    explicit PresetInputBox(const QString& name, QWidget* parent = nullptr);

    void addEmptyBlock();
    void addInput(PresetInput* block);

    void deleteBlock();
    void clearBlocks();

    [[nodiscard]] PresetInputList blocks() const;
    [[nodiscard]] int blockCount() const;

private:
    void updateButtonState();

    QGridLayout* m_layout;
    QVBoxLayout* m_blockLayout;

    QLabel* m_name;
    QPushButton* m_addBlock;
    QPushButton* m_deleteBlock;

    PresetInputList m_blocks;
};
} // namespace Fy::Gui::Settings
