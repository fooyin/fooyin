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

#include "settingsmodel.h"

#include <QDialog>

class QGridLayout;
class QStackedLayout;

namespace Fy::Utils {
class SimpleListView;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(PageList pages, QWidget* parent = nullptr);
    ~SettingsDialog() override;

    void setupUi();

    void openSettings();
    void openPage(const Id& id);

private:
    void done(int value) override;
    void accept() override;
    void reject() override;

    void apply();
    void currentChanged(const QModelIndex& current);
    void currentTabChanged(int index);

    void showCategory(int index);
    void checkCategoryWidget(SettingsCategory* category);

    SettingsModel m_model;
    SimpleListView* m_categoryList;
    QGridLayout* m_mainGridLayout;
    QStackedLayout* m_stackedLayout;

    PageList m_pages;
    std::set<SettingsPage*> m_visitedPages;

    Id m_currentCategory;
    Id m_currentPage;
};
} // namespace Fy::Utils
