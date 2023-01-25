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

#include <gui/fywidget.h>

class Splitter;
class QHBoxLayout;
class QLineEdit;

namespace Core {
class SettingsManager;
}

namespace Filters {
class FilterManager;

class SearchWidget : public Gui::Widgets::FyWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(FilterManager* manager, Core::SettingsManager* settings, QWidget* parent = nullptr);
    ~SearchWidget() override;

    [[nodiscard]] QString name() const override;

signals:
    void searchChanged(const QString& search);

protected:
    void setupUi();
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void searchBoxContextMenu(bool editing);

private:
    void textChanged(const QString& text);

    Core::SettingsManager* m_settings;
    FilterManager* m_manager;
    QHBoxLayout* m_layout;
    QLineEdit* m_searchBox;
    QString m_defaultText;
};
} // namespace Filters
