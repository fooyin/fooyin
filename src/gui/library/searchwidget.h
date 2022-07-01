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

#include "gui/widgets/widget.h"

namespace Library {
class MusicLibrary;
}

class Splitter;
class WidgetProvider;

class SearchWidget : public Widget,
                     public FactoryRegister<SearchWidget>
{
    Q_OBJECT

public:
    explicit SearchWidget(WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~SearchWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] static QString widgetName();
    void layoutEditingMenu(QMenu* menu) override;

signals:
    void searchChanged(const QString& search);

protected:
    void setupUi();
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void searchBoxContextMenu();

private:
    struct Private;
    std::unique_ptr<SearchWidget::Private> p;

    void textChanged(const QString& text);
};
