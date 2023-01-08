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

#include <QMainWindow>

namespace Core {
class ActionContainer;

namespace Widgets {
class FyWidget;
class SplitterWidget;

class EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(QWidget* parent = nullptr);
    ~EditableLayout() override;

    ActionContainer* createAddMenu(SplitterWidget* parent);
    void setupAddMenu(ActionContainer* menu, SplitterWidget* parent);

    void changeLayout(const QByteArray& layout);

    static FyWidget* splitterChild(QWidget* widget);
    void addParentContext(FyWidget* widget, ActionContainer* menu);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void saveLayout();
    bool loadLayout(const QByteArray& layout);
    bool loadLayout();

    static QRect widgetGeometry(FyWidget* widget);
    void showOverlay(FyWidget* widget);
    void hideOverlay();

private:
    struct Private;
    std::unique_ptr<EditableLayout::Private> p;
};
}; // namespace Widgets
}; // namespace Core
