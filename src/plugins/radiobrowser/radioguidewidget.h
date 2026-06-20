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
#include "radioguideconfig.h"
#include "radiostation.h"
#include "radiowidget.h"

#include <QQueue>

#include <optional>

class QJsonObject;
class QPoint;

namespace Fooyin {
class SettingsManager;

namespace RadioBrowser {
class RadioBrowserController;
class RadioGuideModel;
class RadioGuideView;

class RadioGuideWidget : public RadioWidget
{
    Q_OBJECT

public:
    using ConfigData = RadioGuideConfig;

    explicit RadioGuideWidget(RadioBrowserController* controller, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;
    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

protected:
    void openConfigDialog() override;

private:
    void initialiseModel();
    void reloadGuideConfig(const RadioGuideConfig& config);
    void requestCategories(RadioCategoryType type);
    void requestNextCategory();
    void setCategories(RadioCategoryType type, const RadioCategoryList& categories);
    void setSavedSearches(const RadioSavedSearchList& searches);
    void expandSavedSearches();
    void selectLatestSearch();
    void saveState() const;
    bool restoreState();
    bool restoreLastEntry();
    void activateDefaultEntry();
    void activateCurrentIndex();
    void activateIndex(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);
    void addDisplayMenu(QMenu* menu);
    void setScrollbarVisible(bool visible);
    void refreshThemeIcons();
    void addCustomStation();
    void importSavedStations();
    void exportSavedStations();
    void removeSavedSearch(const QString& id);

    RadioBrowserController* m_controller;
    SettingsManager* m_settings;

    RadioGuideView* m_treeView;
    RadioGuideModel* m_model;
    ConfigData m_config;
    QQueue<RadioCategoryType> m_categoryRequests;
    std::optional<RadioCategoryType> m_activeCategoryRequest;
    QString m_pendingRestoreEntryKey;
    size_t m_savedSearchCount;
    bool m_savedSearchesLoaded;
    bool m_showScrollbar;
    bool m_showCountries;
};
} // namespace RadioBrowser
} // namespace Fooyin
