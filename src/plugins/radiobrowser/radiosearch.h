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

#include <QPointer>

#include <array>

class QAction;
class QComboBox;
class QFrame;
class QGridLayout;
class QHBoxLayout;
class QJsonObject;
class QLabel;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Fooyin {
class ExpandingComboBox;
class SettingsManager;
class ToolButton;

namespace RadioBrowser {
class RadioSearch : public FyWidget
{
    Q_OBJECT

public:
    explicit RadioSearch(SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] RadioSearchRequest request(bool hideBroken) const;
    void setRequest(const RadioSearchRequest& request);
    void setSearchText(const QString& text);
    void reset();

    void setCountryCategories(const RadioCategoryList& categories);
    void setCodecCategories(const RadioCategoryList& categories);
    [[nodiscard]] QString countryName(const QString& countryCode) const;
    [[nodiscard]] QString genreName(const QString& tag) const;

    void setSaveSearchEnabled(bool enabled);
    void setSaveSearchToolTip(const QString& tooltip);
    void setSaveSearchIcon(bool saved);
    [[nodiscard]] QPoint saveSearchMenuPosition() const;

    void refreshThemeIcons();

Q_SIGNALS:
    void filterChanged();
    void saveSearchTriggered();
    void resetTriggered();

private:
    struct FilterGroup
    {
        QLabel* label{nullptr};
        ToolButton* clearButton{nullptr};
        ToolButton* pinButton{nullptr};
        bool pinned{false};
    };

    struct FilterState
    {
        QString searchText;
        QString countryCode;
        QString genreTag;
        QString codec;
        int minBitrate{0};
        int maxBitrate{0};
    };

    enum class FilterControl : size_t
    {
        Country = 0,
        Genre,
        Codec,
        Bitrate,
        Count
    };

    void setCategories(ExpandingComboBox* popupCombo, ExpandingComboBox* pinnedCombo, const QString& anyLabel,
                       const RadioCategoryList& categories, FilterControl control);
    void populateGenreCombo();
    int ensureGenreIndex(const QString& tag);

    void setState(const FilterState& state, bool notify);
    void renderControlsFromState();
    void renderControlFromState(FilterControl control);

    [[nodiscard]] int activeFilterCount() const;
    [[nodiscard]] bool filterActive(FilterControl control) const;

    void updateFilterButton();
    void updateFilterClearButtons();
    void clearFilter(FilterControl control);
    void ensurePinnedControls(FilterControl control);
    void showFilterPopup();
    void showFilterButtonContextMenu(const QPoint& pos);

    void updateFilterPlacement();
    void setFilterPinned(FilterControl control, bool pinned);
    [[nodiscard]] bool filterPinned(FilterControl control) const;

    static size_t filterIndex(FilterControl control);
    [[nodiscard]] FilterGroup& filterGroup(FilterControl control);
    [[nodiscard]] const FilterGroup& filterGroup(FilterControl control) const;
    static QString pinLayoutKey(FilterControl control);

    QHBoxLayout* m_mainLayout;
    QLineEdit* m_searchEdit;
    QPointer<ExpandingComboBox> m_countryCombo;
    QPointer<ExpandingComboBox> m_genreCombo;
    QPointer<ExpandingComboBox> m_codecCombo;
    QPointer<QSpinBox> m_minBitrate;
    QPointer<QSpinBox> m_maxBitrate;
    RadioCategoryList m_countryCategories;
    RadioCategoryList m_codecCategories;
    ExpandingComboBox* m_popupCountryCombo;
    ExpandingComboBox* m_popupGenreCombo;
    ExpandingComboBox* m_popupCodecCombo;
    QSpinBox* m_popupMinBitrate;
    QSpinBox* m_popupMaxBitrate;
    QWidget* m_popupBitrateWidget;
    ToolButton* m_filterButton;
    ToolButton* m_saveSearchButton;
    QAction* m_saveSearchAction;
    QAction* m_resetFiltersAction;
    QPointer<QWidget> m_pinnedFiltersWidget;
    QPointer<QHBoxLayout> m_pinnedFiltersLayout;
    QFrame* m_filterPopup;
    QGridLayout* m_popupFiltersLayout;
    std::array<FilterGroup, static_cast<size_t>(FilterControl::Count)> m_filterGroups;
    FilterState m_state;

    bool m_savedSearch;
    bool m_showFilterButtonText;
};
} // namespace RadioBrowser
} // namespace Fooyin
