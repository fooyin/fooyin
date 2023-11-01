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

#include <utils/expandableinputbox.h>

#include <gui/guiconstants.h>

#include <QApplication>
#include <QColorDialog>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

constexpr auto AddIcon    = "list-add";
constexpr auto RemoveIcon = "list-remove";

namespace Fy::Utils {
struct ExpandableInput::Private
{
    Attributes attributes;
    QLineEdit* editBlock{nullptr};

    explicit Private(Attributes attributes)
        : attributes{attributes}
    { }
};

ExpandableInput::ExpandableInput(QWidget* parent)
    : ExpandableInput{None, parent}
{ }

ExpandableInput::ExpandableInput(Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(attributes)}
{
    if(!(attributes & CustomWidget)) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        p->editBlock = new QLineEdit(this);
        QObject::connect(p->editBlock, &QLineEdit::textEdited, this, &ExpandableInput::textChanged);
        layout->addWidget(p->editBlock);
    }

    ExpandableInput::setAttributes(attributes);
}

ExpandableInput::~ExpandableInput() = default;

ExpandableInput::Attributes ExpandableInput::attributes() const
{
    return p->attributes;
}

QString ExpandableInput::text() const
{
    if(p->editBlock) {
        return p->editBlock->text();
    }
    return {};
}

QLineEdit* ExpandableInput::widget() const
{
    if(p->editBlock) {
        return p->editBlock;
    }
    return nullptr;
}

void ExpandableInput::setReadOnly(bool readOnly)
{
    if(p->editBlock) {
        p->editBlock->setReadOnly(readOnly);
    }
}

void ExpandableInput::setAttributes(Attributes attributes)
{
    p->attributes = attributes;

    if(attributes & ClearButton && p->editBlock) {
        p->editBlock->setClearButtonEnabled(true);
    }
}

void ExpandableInput::setText(const QString& text)
{
    if(p->editBlock) {
        p->editBlock->setText(text);
    }
}

struct ExpandableInputBox::Private
{
    ExpandableInputBox* self;

    ExpandableInput::Attributes attributes;

    QGridLayout* blockLayout;
    QPushButton* addBlock;
    QPushButton* deleteBlock;
    ExpandableInputList blocks;

    int maxBlocks{8};

    std::function<ExpandableInput*(QWidget*)> widget{nullptr};
    std::function<QWidget*(ExpandableInput*)> sideWidget{nullptr};
    std::vector<QWidget*> createdSideWidgets;

    Private(ExpandableInputBox* self, const QString& title, ExpandableInput::Attributes attributes)
        : self{self}
        , attributes{attributes}
        , blockLayout{new QGridLayout()}
        , addBlock{new QPushButton(QIcon::fromTheme(AddIcon), "", self)}
        , deleteBlock{new QPushButton(QIcon::fromTheme(RemoveIcon), "", self)}
    {
        auto* layout = new QGridLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        addBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        deleteBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* titleLabel = new QLabel(title, self);

        layout->addWidget(titleLabel, 0, 0);
        layout->addWidget(deleteBlock, 0, 1);
        layout->addWidget(addBlock, 0, 2);
        layout->addLayout(blockLayout, 1, 0, 1, 3);
    }

    void updateButtonState() const
    {
        addBlock->setEnabled(static_cast<int>(blocks.size()) < maxBlocks);
        deleteBlock->setVisible(blocks.size() > 1);
    }
};

ExpandableInputBox::ExpandableInputBox(const QString& title, QWidget* parent)
    : ExpandableInputBox{title, ExpandableInput::None, parent}
{ }

ExpandableInputBox::ExpandableInputBox(const QString& title, ExpandableInput::Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, title, attributes)}
{
    QObject::connect(p->addBlock, &QPushButton::pressed, this, &ExpandableInputBox::addEmptyBlock);
    QObject::connect(p->deleteBlock, &QPushButton::pressed, this, &ExpandableInputBox::deleteBlock);
}

ExpandableInputBox::~ExpandableInputBox() = default;

void ExpandableInputBox::addEmptyBlock()
{
    ExpandableInput* block{nullptr};

    if(p->attributes & ExpandableInput::CustomWidget && p->widget) {
        block = p->widget(this);
    }
    else {
        block = new ExpandableInput(p->attributes, this);
    }

    const int row = static_cast<int>(p->blocks.size());

    p->blockLayout->addWidget(block, row, 0);

    if(p->sideWidget) {
        if(auto* widget = p->sideWidget(block)) {
            p->blockLayout->addWidget(widget, row, 1);
            p->createdSideWidgets.push_back(widget);
        }
    }

    p->blocks.emplace_back(block);
    p->updateButtonState();
}

void ExpandableInputBox::addInput(ExpandableInput* input)
{
    const int row = static_cast<int>(p->blocks.size());

    input->setAttributes(p->attributes);
    p->blockLayout->addWidget(input, row, 0);

    if(p->sideWidget) {
        if(auto* widget = p->sideWidget(input)) {
            p->blockLayout->addWidget(widget, row, 1);
            p->createdSideWidgets.push_back(widget);
        }
    }

    p->blocks.emplace_back(input);
    p->updateButtonState();
}

ExpandableInputList ExpandableInputBox::blocks() const
{
    return p->blocks;
}

int ExpandableInputBox::blockCount() const
{
    return static_cast<int>(p->blocks.size());
}

void ExpandableInputBox::setMaximum(int max)
{
    p->maxBlocks = max;
}

void ExpandableInputBox::setInputWidget(std::function<ExpandableInput*(QWidget*)> widget)
{
    p->widget = std::move(widget);
}

void ExpandableInputBox::setSideWidget(std::function<QWidget*(ExpandableInput*)> widget)
{
    p->sideWidget = std::move(widget);
}

void ExpandableInputBox::deleteBlock()
{
    if(p->blocks.empty()) {
        return;
    }

    auto* block = p->blocks.back();
    emit blockDeleted(block->text());
    std::erase(p->blocks, block);
    block->deleteLater();

    if(p->sideWidget) {
        auto* blockWidget = p->createdSideWidgets.back();
        std::erase(p->createdSideWidgets, blockWidget);
        blockWidget->deleteLater();
    }

    p->updateButtonState();
}

void ExpandableInputBox::clearBlocks()
{
    QLayoutItem* child;
    while((child = p->blockLayout->takeAt(0)) != nullptr) {
        if(QWidget* widget = child->widget()) {
            widget->deleteLater();
        }
    }

    p->blocks.clear();
}
} // namespace Fy::Utils
