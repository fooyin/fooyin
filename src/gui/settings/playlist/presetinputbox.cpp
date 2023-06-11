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

#include "presetinputbox.h"

#include "gui/guiconstants.h"

#include <QGridLayout>

namespace Fy::Gui::Settings {
PresetInputBox::PresetInputBox(const QString& name, QWidget* parent)
    : QWidget{parent}
    , m_layout{new QGridLayout(this)}
    , m_blockLayout{new QVBoxLayout()}
    , m_name{new QLabel(name, this)}
    , m_addBlock{new QPushButton(QIcon::fromTheme(Constants::Icons::Add), "", this)}
    , m_deleteBlock{new QPushButton(QIcon::fromTheme(Constants::Icons::Remove), "", this)}
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_addBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_deleteBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_layout->addWidget(m_name, 0, 0);
    m_layout->addWidget(m_deleteBlock, 0, 1);
    m_layout->addWidget(m_addBlock, 0, 2);
    m_layout->addLayout(m_blockLayout, 1, 0, 1, 3);

    QObject::connect(m_addBlock, &QPushButton::pressed, this, &PresetInputBox::addEmptyBlock);
    QObject::connect(m_deleteBlock, &QPushButton::pressed, this, &PresetInputBox::deleteBlock);
}

void PresetInputBox::addEmptyBlock()
{
    auto* block = new PresetInput(this);
    m_blockLayout->addWidget(block);
    m_blocks.emplace_back(block);
    updateButtonState();
}

void PresetInputBox::addInput(PresetInput* input)
{
    m_blockLayout->addWidget(input);
    m_blocks.emplace_back(input);
    updateButtonState();
}

void PresetInputBox::deleteBlock()
{
    if(m_blocks.empty()) {
        return;
    }
    auto* block = m_blocks.back();
    m_blockLayout->removeWidget(block);
    block->deleteLater();
    m_blocks.erase(m_blocks.end() - 1);
    updateButtonState();
}

void PresetInputBox::clearBlocks()
{
    QLayoutItem* child;
    while((child = m_blockLayout->takeAt(0)) != nullptr) {
        if(QWidget* widget = child->widget()) {
            m_blockLayout->removeWidget(widget);
        }
    }

    for(PresetInput* block : m_blocks) {
        block->deleteLater();
    }
    m_blocks.clear();
}

PresetInputList PresetInputBox::blocks() const
{
    return m_blocks;
}

int PresetInputBox::blockCount() const
{
    return static_cast<int>(m_blocks.size());
}

void PresetInputBox::updateButtonState()
{
    m_addBlock->setEnabled(m_blocks.size() < 4);
    m_deleteBlock->setVisible(m_blocks.size() > 1);
}
} // namespace Fy::Gui::Settings
