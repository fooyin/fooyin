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

#include <utils/expandableinputbox.h>

#include <utils/utils.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

constexpr auto AddIcon    = "list-add";
constexpr auto RemoveIcon = "list-remove";

namespace Fooyin {
class ExpandableInputPrivate
{
public:
    explicit ExpandableInputPrivate(ExpandableInput::Attributes attributes_)
        : attributes{attributes_}
    { }

    ExpandableInput::Attributes attributes;
    QLineEdit* editBlock{nullptr};
    bool readOnly{false};
};

ExpandableInput::ExpandableInput(QWidget* parent)
    : ExpandableInput{None, parent}
{ }

ExpandableInput::ExpandableInput(Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<ExpandableInputPrivate>(attributes)}
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

bool ExpandableInput::readOnly() const
{
    return p->readOnly;
}

void ExpandableInput::setReadOnly(bool readOnly)
{
    p->readOnly = readOnly;

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

class ExpandableInputBoxPrivate
{
public:
    ExpandableInputBoxPrivate(ExpandableInputBox* self_, const QString& title_,
                              ExpandableInput::Attributes attributes_);

    void updateButtonState() const;

    ExpandableInputBox* m_self;

    ExpandableInput::Attributes m_attributes;

    QHBoxLayout* m_widgetLayout;
    QGridLayout* m_blockLayout;
    QPushButton* m_addBlock;
    QPushButton* m_deleteBlock;
    ExpandableInputList m_blocks;

    int m_maxBlocks{8};
    bool m_readOnly{false};

    std::function<ExpandableInput*(QWidget*)> m_widget{nullptr};
    std::function<QWidget*(ExpandableInput*)> m_sideWidget{nullptr};
    std::vector<QWidget*> m_createdSideWidgets;
};

ExpandableInputBoxPrivate::ExpandableInputBoxPrivate(ExpandableInputBox* self_, const QString& title_,
                                                     ExpandableInput::Attributes attributes_)
    : m_self{self_}
    , m_attributes{attributes_}
    , m_widgetLayout{new QHBoxLayout()}
    , m_blockLayout{new QGridLayout()}
    , m_addBlock{new QPushButton(Utils::iconFromTheme(AddIcon), {}, m_self)}
    , m_deleteBlock{new QPushButton(Utils::iconFromTheme(RemoveIcon), {}, m_self)}
{
    auto* layout = new QGridLayout(m_self);
    layout->setContentsMargins(0, 0, 0, 0);

    m_addBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_deleteBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* titleLabel = new QLabel(title_, m_self);

    layout->addWidget(titleLabel, 0, 0);
    layout->addLayout(m_widgetLayout, 0, 1);
    layout->addWidget(m_deleteBlock, 0, 2);
    layout->addWidget(m_addBlock, 0, 3);
    layout->addLayout(m_blockLayout, 1, 0, 1, 4);

    layout->setColumnStretch(0, 1);
}

void ExpandableInputBoxPrivate::updateButtonState() const
{
    m_addBlock->setEnabled(!m_readOnly && static_cast<int>(m_blocks.size()) < m_maxBlocks);
    m_deleteBlock->setEnabled(!m_readOnly && m_blocks.size() > 1);
}

ExpandableInputBox::ExpandableInputBox(const QString& title, QWidget* parent)
    : ExpandableInputBox{title, ExpandableInput::None, parent}
{ }

ExpandableInputBox::ExpandableInputBox(const QString& title, ExpandableInput::Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<ExpandableInputBoxPrivate>(this, title, attributes)}
{
    QObject::connect(p->m_addBlock, &QPushButton::pressed, this, &ExpandableInputBox::addEmptyBlock);
    QObject::connect(p->m_deleteBlock, &QPushButton::pressed, this, &ExpandableInputBox::deleteBlock);
}

ExpandableInputBox::~ExpandableInputBox() = default;

void ExpandableInputBox::addEmptyBlock()
{
    ExpandableInput* block{nullptr};

    if(p->m_attributes & ExpandableInput::CustomWidget && p->m_widget) {
        block = p->m_widget(this);
    }
    else {
        block = new ExpandableInput(p->m_attributes, this);
    }
    block->setReadOnly(p->m_readOnly);

    const int row = static_cast<int>(p->m_blocks.size());

    p->m_blockLayout->addWidget(block, row, 0);

    if(p->m_sideWidget) {
        if(auto* widget = p->m_sideWidget(block)) {
            p->m_blockLayout->addWidget(widget, row, 1);
            p->m_createdSideWidgets.push_back(widget);
        }
    }

    p->m_blocks.emplace_back(block);
    p->updateButtonState();
}

void ExpandableInputBox::addInput(ExpandableInput* input)
{
    const int row = static_cast<int>(p->m_blocks.size());

    input->setAttributes(p->m_attributes);
    input->setReadOnly(p->m_readOnly);
    p->m_blockLayout->addWidget(input, row, 0);

    if(p->m_sideWidget) {
        if(auto* widget = p->m_sideWidget(input)) {
            p->m_blockLayout->addWidget(widget, row, 1);
            p->m_createdSideWidgets.push_back(widget);
        }
    }

    p->m_blocks.emplace_back(input);
    p->updateButtonState();
}

ExpandableInputList ExpandableInputBox::blocks() const
{
    return p->m_blocks;
}

int ExpandableInputBox::blockCount() const
{
    return static_cast<int>(p->m_blocks.size());
}

void ExpandableInputBox::setMaximum(int max)
{
    p->m_maxBlocks = max;
}

bool ExpandableInputBox::readOnly() const
{
    return p->m_readOnly;
}

void ExpandableInputBox::setReadOnly(bool readOnly)
{
    p->m_readOnly = readOnly;

    p->updateButtonState();
}

void ExpandableInputBox::addBoxWidget(QWidget* widget)
{
    p->m_widgetLayout->addWidget(widget);
}

void ExpandableInputBox::setInputWidget(std::function<ExpandableInput*(QWidget*)> widget)
{
    p->m_widget = std::move(widget);
}

void ExpandableInputBox::setSideWidget(std::function<QWidget*(ExpandableInput*)> widget)
{
    p->m_sideWidget = std::move(widget);
}

void ExpandableInputBox::deleteBlock()
{
    if(p->m_blocks.empty()) {
        return;
    }

    auto* block = p->m_blocks.back();
    emit blockDeleted(block->text());
    std::erase(p->m_blocks, block);
    block->deleteLater();

    if(p->m_sideWidget) {
        auto* blockWidget = p->m_createdSideWidgets.back();
        std::erase(p->m_createdSideWidgets, blockWidget);
        blockWidget->deleteLater();
    }

    p->updateButtonState();
}

void ExpandableInputBox::clearBlocks()
{
    QLayoutItem* child;
    while((child = p->m_blockLayout->takeAt(0)) != nullptr) {
        if(QWidget* widget = child->widget()) {
            widget->deleteLater();
        }
    }

    p->m_blocks.clear();
}
} // namespace Fooyin

#include "utils/moc_expandableinputbox.cpp"
