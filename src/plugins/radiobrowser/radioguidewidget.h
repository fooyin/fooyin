/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "radiodiscovery.h"
#include "radiostation.h"

#include <gui/fywidget.h>

#include <QQueue>

class QJsonObject;
class QPoint;
class QEvent;

namespace Fooyin {
class SettingsManager;

namespace RadioBrowser {
class RadioBrowserController;
class RadioGuideModel;
class RadioGuideView;

class RadioGuideWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit RadioGuideWidget(RadioBrowserController* controller, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    void initialiseModel();
    void requestCategories(RadioCategoryType type);
    void requestNextCategory();
    void setCategories(RadioCategoryType type, const RadioCategoryList& categories);
    void setSavedSearches(const RadioSavedSearchList& searches);
    void expandSavedSearches();
    void selectLatestSearch();
    void saveState() const;
    void restoreState();
    bool restoreLastEntry();
    void activateCurrentIndex();
    void activateIndex(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);
    void addDisplayMenu(QMenu* menu);
    void setScrollbarVisible(bool visible);
    void refreshThemeIcons();
    void addCustomStation();
    void importSavedStations();
    void exportSavedStations();
    void renameSavedSearch(const QString& id, const QString& name);
    void removeSavedSearch(const QString& id);

    RadioBrowserController* m_controller;
    SettingsManager* m_settings;

    RadioGuideView* m_treeView;
    RadioGuideModel* m_model;
    QQueue<RadioCategoryType> m_categoryRequests;
    QString m_pendingRestoreEntryKey;
    size_t m_savedSearchCount;
    bool m_categoryRequestActive;
    bool m_savedSearchesLoaded;
    bool m_showScrollbar;
};
} // namespace RadioBrowser
} // namespace Fooyin
