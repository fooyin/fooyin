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

#pragma once

#include "settingscategory.h"

#include <utils/id.h>

#include <QByteArray>
#include <QDialog>
#include <QString>

#include <set>

class QDialogButtonBox;
class QStackedLayout;
class QModelIndex;

namespace Fooyin {
class SettingsModel;
class SimpleTreeView;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(PageList pages, QWidget* parent = nullptr);

    void openSettings();
    void openPage(const Id& id);

    [[nodiscard]] Id currentPage() const;
    [[nodiscard]] QByteArray saveState() const;
    void restoreState(const QByteArray& state);

    void done(int value) override;
    void accept() override;
    void reject() override;

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void resettingAll();

private:
    bool apply();
    void reset();
    void resetAll();
    void showCategory(const QModelIndex& index);
    void checkCategoryWidget(SettingsCategory* category);
    void currentChanged(const QModelIndex& current);
    void currentTabChanged(int index);
    SettingsPage* findPage(const Id& id);
    static QString categoryKey(const QModelIndex& index);

    SettingsModel* m_model;
    SimpleTreeView* m_categoryTree;
    QStackedLayout* m_stackedLayout;
    QDialogButtonBox* m_buttonBox;

    PageList m_pages;
    std::set<SettingsPage*> m_visitedPages;

    Id m_currentCategory;
    Id m_currentPage;
};
} // namespace Fooyin
