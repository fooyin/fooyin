/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "fyutils_export.h"

#include <QWidget>

class QLineEdit;

namespace Fooyin {
class FYUTILS_EXPORT ExpandableInput : public QWidget
{
    Q_OBJECT

public:
    enum Attribute
    {
        None         = 0,
        CustomWidget = 1 << 0,
        ClearButton  = 1 << 1,
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    explicit ExpandableInput(QWidget* parent = nullptr);
    explicit ExpandableInput(Attributes attributes, QWidget* parent = nullptr);
    ~ExpandableInput() override;

    [[nodiscard]] Attributes attributes() const;
    [[nodiscard]] virtual QString text() const;
    [[nodiscard]] QLineEdit* widget() const;

    virtual void setReadOnly(bool readOnly);

    virtual void setAttributes(Attributes attributes);
    virtual void setText(const QString& text);

signals:
    void textChanged(const QString& text);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
using ExpandableInputList = std::vector<ExpandableInput*>;

class FYUTILS_EXPORT ExpandableInputBox : public QWidget
{
    Q_OBJECT

public:
    explicit ExpandableInputBox(const QString& title, QWidget* parent = nullptr);
    ExpandableInputBox(const QString& title, ExpandableInput::Attributes attributes, QWidget* parent = nullptr);
    ~ExpandableInputBox() override;

    void addEmptyBlock();
    void addInput(ExpandableInput* input);

    [[nodiscard]] ExpandableInputList blocks() const;
    [[nodiscard]] int blockCount() const;

    void setMaximum(int max);

    void addBoxWidget(QWidget* widget);
    void setInputWidget(std::function<ExpandableInput*(QWidget*)> widget);
    void setSideWidget(std::function<QWidget*(ExpandableInput*)> widget);

    void deleteBlock();
    void clearBlocks();

signals:
    void blockDeleted(const QString& text);

private:
    struct Private;
    std::unique_ptr<Private> p;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ExpandableInput::Attributes)
} // namespace Fooyin
