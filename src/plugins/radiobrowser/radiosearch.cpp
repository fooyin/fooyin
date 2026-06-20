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

#include "radiosearch.h"

#include "radioguideconfig.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/expandingcombobox.h>
#include <gui/widgets/toolbutton.h>

#include <QAction>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>

#include <unordered_set>

using namespace Qt::StringLiterals;

constexpr auto ShowFilterTextKey = "ShowFilterText"_L1;
constexpr auto RetryCategoryData = "__radio_browser_retry_category__"_L1;
constexpr int RetryCategoryRole  = Qt::UserRole + 1;

namespace Fooyin::RadioBrowser {
namespace {
void syncComboValue(QComboBox* target, const QString& value)
{
    const int index = value.isEmpty() ? 0 : target->findData(value);
    target->setCurrentIndex(index >= 0 ? index : 0);
}

void ensureComboValue(QComboBox* target, const QString& label, const QString& value)
{
    if(!value.isEmpty() && target->findData(value) < 0) {
        target->addItem(label, value);
    }
}

void updatePinButtonIcon(QToolButton* button, const bool pinned)
{
    if(button) {
        button->setIcon(Gui::iconFromTheme(pinned ? Constants::Icons::WindowUnpin : Constants::Icons::WindowPin));
    }
}

void updateFilterComboWidth(QComboBox* combo)
{
    combo->setMinimumContentsLength(std::max(1, static_cast<int>(combo->currentText().size())));
    combo->updateGeometry();
}

void setupFilterCombo(ExpandingComboBox* combo)
{
    combo->setResizeToCurrentEnabled(false);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    updateFilterComboWidth(combo);

    QObject::connect(combo, &QComboBox::currentTextChanged, combo, [combo] { updateFilterComboWidth(combo); });
}

QStringList uniqueTagList(const QStringList& tags)
{
    QStringList result;
    for(const QString& tag : tags) {
        const QString trimmed = tag.trimmed();
        if(!trimmed.isEmpty() && !result.contains(trimmed)) {
            result.emplace_back(trimmed);
        }
    }
    return result;
}
} // namespace

RadioSearch::RadioSearch(SettingsManager* settings, QWidget* parent)
    : RadioWidget{parent}
    , m_mainLayout{new QHBoxLayout(this)}
    , m_searchEdit{new QLineEdit(this)}
    , m_countryCombo{nullptr}
    , m_languageCombo{nullptr}
    , m_genreCombo{nullptr}
    , m_tagCombo{nullptr}
    , m_codecCombo{nullptr}
    , m_minBitrate{nullptr}
    , m_maxBitrate{nullptr}
    , m_popupCountryCombo{new ExpandingComboBox(this)}
    , m_popupLanguageCombo{new ExpandingComboBox(this)}
    , m_popupGenreCombo{new ExpandingComboBox(this)}
    , m_popupTagCombo{new ExpandingComboBox(this)}
    , m_popupCodecCombo{new ExpandingComboBox(this)}
    , m_popupMinBitrate{new QSpinBox(this)}
    , m_popupMaxBitrate{new QSpinBox(this)}
    , m_popupBitrateWidget{new QWidget(this)}
    , m_filterButton{new ToolButton(settings, this)}
    , m_saveSearchButton{new ToolButton(settings, this)}
    , m_saveSearchAction{new QAction(this)}
    , m_resetFiltersAction{new QAction(tr("Reset filters"), this)}
    , m_filterPopup{new QFrame(this, Qt::Popup)}
    , m_popupFiltersLayout{new QGridLayout(m_filterPopup)}
    , m_savedSearch{false}
    , m_showFilterButtonText{true}
{
    m_mainLayout->setContentsMargins({});

    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Search stations"));
    m_mainLayout->addWidget(m_searchEdit, 1);

    m_popupCountryCombo->addItem(tr("Any country"), QString{});
    setupFilterCombo(m_popupCountryCombo);

    m_popupLanguageCombo->addItem(tr("Any language"), QString{});
    setupFilterCombo(m_popupLanguageCombo);

    m_popupGenreCombo->addItem(tr("Any genre"), QString{});
    setupFilterCombo(m_popupGenreCombo);
    populateGenreCombo();

    m_popupTagCombo->addItem(tr("Any tag"), QString{});
    setupFilterCombo(m_popupTagCombo);

    m_popupCodecCombo->addItem(tr("Any codec"), QString{});
    setupFilterCombo(m_popupCodecCombo);

    m_popupMinBitrate->setRange(0, 1000);
    m_popupMinBitrate->setSingleStep(32);
    m_popupMinBitrate->setSuffix(u" kbps"_s);
    m_popupMinBitrate->setSpecialValueText(tr("Min"));

    m_popupMaxBitrate->setRange(0, 1000);
    m_popupMaxBitrate->setSingleStep(32);
    m_popupMaxBitrate->setSuffix(u" kbps"_s);
    m_popupMaxBitrate->setSpecialValueText(tr("Max"));

    auto* popupBitrateLayout = new QHBoxLayout(m_popupBitrateWidget);
    popupBitrateLayout->setContentsMargins({});
    popupBitrateLayout->addWidget(m_popupMinBitrate);
    popupBitrateLayout->addWidget(m_popupMaxBitrate);

    const auto setupFilterGroup = [this, settings](FilterControl control, const QString& label) {
        auto& group = filterGroup(control);

        group.label = new QLabel(label + u":"_s, m_filterPopup);

        group.clearButton = new ToolButton(settings, m_filterPopup);
        group.clearButton->setToolTip(tr("Clear filter"));

        group.pinButton = new ToolButton(settings, m_filterPopup);
        group.pinButton->setCheckable(true);
        group.pinButton->setToolTip(tr("Pin filter to bar"));

        QObject::connect(group.clearButton, &QToolButton::clicked, this, [this, control]() { clearFilter(control); });
        QObject::connect(group.pinButton, &QToolButton::toggled, this,
                         [this, control](const bool checked) { setFilterPinned(control, checked); });
    };

    setupFilterGroup(FilterControl::Country, tr("Country"));
    setupFilterGroup(FilterControl::Language, tr("Language"));
    setupFilterGroup(FilterControl::Genre, tr("Genre"));
    setupFilterGroup(FilterControl::Tag, tr("Tag"));
    setupFilterGroup(FilterControl::Codec, tr("Codec"));
    setupFilterGroup(FilterControl::Bitrate, tr("Bitrate"));

    m_filterButton->setText(tr("Filters"));
    m_filterButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_filterButton->setToolTip(tr("Show filters"));
    m_filterButton->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mainLayout->addWidget(m_filterButton);

    m_saveSearchButton->setDefaultAction(m_saveSearchAction);
    m_mainLayout->addWidget(m_saveSearchButton);

    auto* resetFilters = new ToolButton(settings, this);
    m_resetFiltersAction->setStatusTip(tr("Reset all filters"));
    resetFilters->setDefaultAction(m_resetFiltersAction);
    m_mainLayout->addWidget(resetFilters);

    QObject::connect(m_searchEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        auto state{m_state};
        state.searchText = text;
        setState(state, true);
    });
    QObject::connect(m_popupCountryCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if(handleCategoryRetry(m_popupCountryCombo, RadioCategoryType::Country)) {
            return;
        }

        auto state{m_state};
        state.countryCode = m_popupCountryCombo->currentData().toString();
        setState(state, true);
    });
    QObject::connect(m_popupLanguageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if(handleCategoryRetry(m_popupLanguageCombo, RadioCategoryType::Language)) {
            return;
        }

        auto state{m_state};
        state.language = m_popupLanguageCombo->currentData().toString();
        setState(state, true);
    });
    QObject::connect(m_popupGenreCombo, &QComboBox::currentIndexChanged, this, [this]() {
        auto state{m_state};
        state.genreTag = m_popupGenreCombo->currentData().toString();
        setState(state, true);
    });
    QObject::connect(m_popupTagCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if(handleCategoryRetry(m_popupTagCombo, RadioCategoryType::Tag)) {
            return;
        }

        auto state{m_state};
        state.tag = m_popupTagCombo->currentData().toString();
        setState(state, true);
    });
    QObject::connect(m_popupCodecCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if(handleCategoryRetry(m_popupCodecCombo, RadioCategoryType::Codec)) {
            return;
        }

        auto state{m_state};
        state.codec = m_popupCodecCombo->currentData().toString();
        setState(state, true);
    });
    QObject::connect(m_popupMinBitrate, &QSpinBox::valueChanged, this, [this](int value) {
        auto state{m_state};
        state.minBitrate = value;
        setState(state, true);
    });
    QObject::connect(m_popupMaxBitrate, &QSpinBox::valueChanged, this, [this](int value) {
        auto state{m_state};
        state.maxBitrate = value;
        setState(state, true);
    });

    QObject::connect(m_filterButton, &QToolButton::clicked, this, &RadioSearch::showFilterPopup);
    QObject::connect(m_filterButton, &QWidget::customContextMenuRequested, this,
                     &RadioSearch::showFilterButtonContextMenu);
    QObject::connect(m_saveSearchAction, &QAction::triggered, this, &RadioSearch::saveSearchTriggered);
    QObject::connect(m_resetFiltersAction, &QAction::triggered, this, &RadioSearch::resetTriggered);

    updateFilterPlacement();
    updateFilterButton();
    refreshThemeIcons();
}

QString RadioSearch::name() const
{
    return tr("Radio Search");
}

QString RadioSearch::layoutName() const
{
    return u"RadioSearch"_s;
}

void RadioSearch::saveLayoutData(QJsonObject& layout)
{
    layout[ShowFilterTextKey] = m_showFilterButtonText;

    for(size_t i{0}; i < filterIndex(FilterControl::Count); ++i) {
        const auto control = static_cast<FilterControl>(i);
        layout.insert(pinLayoutKey(control), filterPinned(control));
    }
}

void RadioSearch::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(ShowFilterTextKey)) {
        m_showFilterButtonText = layout.value(ShowFilterTextKey).toBool(true);
    }

    for(size_t i{0}; i < filterIndex(FilterControl::Count); ++i) {
        const auto control          = static_cast<FilterControl>(i);
        filterGroup(control).pinned = layout.value(pinLayoutKey(control)).toBool(false);
    }

    renderControlsFromState();
    updateFilterPlacement();
    updateFilterButton();
}

RadioSearchRequest RadioSearch::request(bool hideBroken) const
{
    RadioSearchRequest filterRequest;

    filterRequest.displayText = m_state.searchText;
    filterRequest.text        = filterRequest.displayText.trimmed();
    filterRequest.countryCode = m_state.countryCode;
    filterRequest.language    = m_state.language;
    filterRequest.tag         = uniqueTagList({m_state.genreTag, m_state.tag}).join(u","_s);
    filterRequest.order       = u"clickcount"_s;
    filterRequest.bitrateMin  = m_state.minBitrate;
    filterRequest.bitrateMax  = m_state.maxBitrate;
    filterRequest.hideBroken  = hideBroken;

    if(filterRequest.bitrateMax > 0 && filterRequest.bitrateMin > filterRequest.bitrateMax) {
        filterRequest.bitrateMax = filterRequest.bitrateMin;
    }

    filterRequest.codec = m_state.codec;

    return filterRequest;
}

void RadioSearch::setRequest(const RadioSearchRequest& request)
{
    FilterState state;

    state.searchText  = request.displayText.isNull() ? request.text : request.displayText;
    state.countryCode = request.countryCode.trimmed().toUpper();
    state.language    = request.language.trimmed();
    state.codec       = request.codec.trimmed();
    state.minBitrate  = std::max(0, request.bitrateMin);
    state.maxBitrate  = std::max(0, request.bitrateMax);

    QStringList remainingTags;
    const auto splitTags = splitStationTags(request.tag);
    for(const QString& tag : splitTags) {
        if(state.genreTag.isEmpty() && m_popupGenreCombo->findData(tag) >= 0) {
            state.genreTag = tag;
        }
        else {
            remainingTags.emplace_back(tag);
        }
    }
    state.tag = uniqueTagList(remainingTags).join(u","_s);

    setState(state, false);
}

void RadioSearch::setSearchText(const QString& text)
{
    auto state{m_state};
    state.searchText = text;
    setState(state, false);
}

void RadioSearch::reset()
{
    setState({}, false);
}

void RadioSearch::setCountryCategories(const RadioCategoryList& categories)
{
    m_countryCategories = categories;
    setCategories(m_popupCountryCombo, m_countryCombo, tr("Any country"), categories, FilterControl::Country);
}

void RadioSearch::setLanguageCategories(const RadioCategoryList& categories)
{
    m_languageCategories = categories;
    setCategories(m_popupLanguageCombo, m_languageCombo, tr("Any language"), categories, FilterControl::Language);
}

void RadioSearch::setTagCategories(const RadioCategoryList& categories)
{
    m_tagCategories = categories;
    populateTagCombo();
}

void RadioSearch::setCodecCategories(const RadioCategoryList& categories)
{
    m_codecCategories = categories;
    setCategories(m_popupCodecCombo, m_codecCombo, tr("Any codec"), categories, FilterControl::Codec);
}

void RadioSearch::setCountryCategoriesFailed(const QString& error)
{
    setCategoriesFailed(m_popupCountryCombo, m_countryCombo, tr("Any country"),
                        error.isEmpty() ? tr("Countries unavailable - select to retry")
                                        : tr("Countries unavailable: %1 - select to retry").arg(error),
                        FilterControl::Country);
}

void RadioSearch::setLanguageCategoriesFailed(const QString& error)
{
    setCategoriesFailed(m_popupLanguageCombo, m_languageCombo, tr("Any language"),
                        error.isEmpty() ? tr("Languages unavailable - select to retry")
                                        : tr("Languages unavailable: %1 - select to retry").arg(error),
                        FilterControl::Language);
}

void RadioSearch::setTagCategoriesFailed(const QString& error)
{
    setCategoriesFailed(m_popupTagCombo, m_tagCombo, tr("Any tag"),
                        error.isEmpty() ? tr("Tags unavailable - select to retry")
                                        : tr("Tags unavailable: %1 - select to retry").arg(error),
                        FilterControl::Tag);
}

void RadioSearch::setCodecCategoriesFailed(const QString& error)
{
    setCategoriesFailed(m_popupCodecCombo, m_codecCombo, tr("Any codec"),
                        error.isEmpty() ? tr("Codecs unavailable - select to retry")
                                        : tr("Codecs unavailable: %1 - select to retry").arg(error),
                        FilterControl::Codec);
}

QString RadioSearch::countryName(const QString& countryCode) const
{
    const int index = m_popupCountryCombo->findData(countryCode.trimmed());
    return index >= 0 ? m_popupCountryCombo->itemText(index) : countryCode.trimmed();
}

QString RadioSearch::tagName(const QString& tag) const
{
    QStringList names;
    const auto splitTags = splitStationTags(tag);
    for(const QString& entry : splitTags) {
        if(m_genreNames.contains(entry)) {
            names.emplace_back(m_genreNames.at(entry));
        }
        else if(m_tagNames.contains(entry)) {
            names.emplace_back(m_tagNames.at(entry));
        }
        else {
            names.emplace_back(entry);
        }
    }
    return names.join(u", "_s);
}

void RadioSearch::setSaveSearchEnabled(const bool enabled)
{
    m_saveSearchAction->setEnabled(enabled);
}

void RadioSearch::setSaveSearchToolTip(const QString& tooltip)
{
    m_saveSearchAction->setToolTip(tooltip);
    m_saveSearchAction->setStatusTip(tooltip);
}

void RadioSearch::setSaveSearchIcon(const bool saved)
{
    m_savedSearch = saved;
    Gui::setThemeIcon(m_saveSearchAction, saved ? Constants::Icons::Favorite : Constants::Icons::FavoriteOff);
}

QPoint RadioSearch::saveSearchMenuPosition() const
{
    return m_saveSearchButton->mapToGlobal(QPoint{0, m_saveSearchButton->height()});
}

void RadioSearch::refreshThemeIcons()
{
    m_filterButton->setIcon(Gui::iconFromTheme(Constants::Icons::Filter));

    for(size_t i{0}; i < m_filterGroups.size(); ++i) {
        updatePinButtonIcon(m_filterGroups.at(i).pinButton, filterPinned(static_cast<FilterControl>(i)));
    }

    updateFilterClearButtons();
    setSaveSearchIcon(m_savedSearch);
    Gui::setThemeIcon(m_resetFiltersAction, Constants::Icons::Clear);
}

void RadioSearch::setCategories(ExpandingComboBox* popupCombo, ExpandingComboBox* pinnedCombo, const QString& anyLabel,
                                const RadioCategoryList& categories, FilterControl control)
{
    const QSignalBlocker popupBlocker{popupCombo};
    std::optional<QSignalBlocker> pinnedBlocker;
    if(pinnedCombo) {
        pinnedBlocker.emplace(pinnedCombo);
    }

    std::vector combos{popupCombo};
    if(pinnedCombo) {
        combos.push_back(pinnedCombo);
    }

    for(auto* combo : combos) {
        combo->clear();
        combo->addItem(anyLabel, QString{});
    }

    RadioCategoryList sorted{categories};
    std::ranges::sort(sorted, [](const RadioCategory& lhs, const RadioCategory& rhs) {
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    for(const auto& category : sorted) {
        for(auto* combo : combos) {
            combo->addItem(category.name, category.value);
        }
    }

    for(auto* combo : combos) {
        combo->resizeDropDown();
    }

    renderControlFromState(control);
    updateFilterButton();
}

void RadioSearch::setCategoriesFailed(ExpandingComboBox* popupCombo, ExpandingComboBox* pinnedCombo,
                                      const QString& anyLabel, const QString& failureLabel, FilterControl control)
{
    const QSignalBlocker popupBlocker{popupCombo};
    std::optional<QSignalBlocker> pinnedBlocker;
    if(pinnedCombo) {
        pinnedBlocker.emplace(pinnedCombo);
    }

    std::vector combos{popupCombo};
    if(pinnedCombo) {
        combos.push_back(pinnedCombo);
    }

    for(auto* combo : combos) {
        combo->clear();
        combo->addItem(anyLabel, QString{});
        combo->addItem(failureLabel, QString{});
        combo->setItemData(1, RetryCategoryData, RetryCategoryRole);
        combo->resizeDropDown();
    }

    renderControlFromState(control);
    updateFilterButton();
}

bool RadioSearch::handleCategoryRetry(QComboBox* combo, const RadioCategoryType type)
{
    if(!combo || combo->itemData(combo->currentIndex(), RetryCategoryRole).toString() != RetryCategoryData) {
        return false;
    }

    const QSignalBlocker blocker{combo};
    combo->setCurrentIndex(0);
    Q_EMIT categoryRetryRequested(type);
    return true;
}

void RadioSearch::populateGenreCombo()
{
    const QSignalBlocker popupBlocker{m_popupGenreCombo};
    std::optional<QSignalBlocker> pinnedBlocker;
    if(m_genreCombo) {
        pinnedBlocker.emplace(m_genreCombo);
    }

    std::vector combos{m_popupGenreCombo};
    if(m_genreCombo) {
        combos.push_back(m_genreCombo);
    }

    for(auto* combo : combos) {
        combo->clear();
        combo->addItem(tr("Any genre"), QString{});
    }

    m_genreNames.clear();

    RadioGuideTagList genreTags = RadioGuideConfigStore::defaultGenreTags();
    std::ranges::sort(genreTags, [](const RadioGuideTag& lhs, const RadioGuideTag& rhs) {
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    std::unordered_set<QString> seenTags;

    for(const RadioGuideTag& tag : genreTags) {
        const QString value = tag.tag.trimmed();
        if(value.isEmpty() || seenTags.contains(value)) {
            continue;
        }

        seenTags.emplace(value);

        for(auto* combo : combos) {
            combo->addItem(tag.name, value);
        }

        m_genreNames.emplace(value, tag.name);
    }

    for(auto* combo : combos) {
        combo->resizeDropDown();
    }

    if(m_filterGroups.at(filterIndex(FilterControl::Genre)).label) {
        renderControlFromState(FilterControl::Genre);
        updateFilterButton();
    }
}

void RadioSearch::populateTagCombo()
{
    const QSignalBlocker popupBlocker{m_popupTagCombo};
    std::optional<QSignalBlocker> pinnedBlocker;
    if(m_tagCombo) {
        pinnedBlocker.emplace(m_tagCombo);
    }

    m_popupTagCombo->clear();
    m_popupTagCombo->addItem(tr("Any tag"), QString{});
    if(m_tagCombo) {
        m_tagCombo->clear();
        m_tagCombo->addItem(tr("Any tag"), QString{});
    }

    m_tagNames.clear();

    RadioCategoryList sorted{m_tagCategories};
    std::ranges::sort(sorted, [](const RadioCategory& lhs, const RadioCategory& rhs) {
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    for(const RadioCategory& tag : sorted) {
        m_popupTagCombo->addItem(tag.name, tag.value);
        if(m_tagCombo) {
            m_tagCombo->addItem(tag.name, tag.value);
        }
        m_tagNames.emplace(tag.value, tag.name);
    }

    const QString stateTag = m_state.tag.trimmed();
    if(!stateTag.isEmpty() && m_popupTagCombo->findData(stateTag) < 0) {
        m_popupTagCombo->addItem(stateTag, stateTag);
        if(m_tagCombo) {
            m_tagCombo->addItem(stateTag, stateTag);
        }
        m_tagNames.emplace(stateTag, stateTag);
    }

    m_popupTagCombo->resizeDropDown();
    if(m_tagCombo) {
        m_tagCombo->resizeDropDown();
    }

    if(m_filterGroups.at(filterIndex(FilterControl::Tag)).label) {
        renderControlFromState(FilterControl::Tag);
        updateFilterButton();
    }
}

void RadioSearch::setState(const FilterState& state, const bool notify)
{
    auto normalised{state};
    normalised.minBitrate
        = std::clamp(normalised.minBitrate, m_popupMinBitrate->minimum(), m_popupMinBitrate->maximum());
    normalised.maxBitrate
        = std::clamp(normalised.maxBitrate, m_popupMaxBitrate->minimum(), m_popupMaxBitrate->maximum());
    const bool changed = m_state.searchText != normalised.searchText || m_state.countryCode != normalised.countryCode
                      || m_state.language != normalised.language || m_state.genreTag != normalised.genreTag
                      || m_state.tag != normalised.tag || m_state.codec != normalised.codec
                      || m_state.minBitrate != normalised.minBitrate || m_state.maxBitrate != normalised.maxBitrate;

    m_state = normalised;
    renderControlsFromState();
    updateFilterButton();

    if(changed && notify) {
        Q_EMIT filterChanged();
    }
}

void RadioSearch::renderControlsFromState()
{
    if(m_searchEdit->text() != m_state.searchText) {
        const QSignalBlocker blocker{m_searchEdit};
        m_searchEdit->setText(m_state.searchText);
    }

    renderControlFromState(FilterControl::Country);
    renderControlFromState(FilterControl::Language);
    renderControlFromState(FilterControl::Genre);
    renderControlFromState(FilterControl::Tag);
    renderControlFromState(FilterControl::Codec);
    renderControlFromState(FilterControl::Bitrate);
}

void RadioSearch::renderControlFromState(FilterControl control)
{
    switch(control) {
        case FilterControl::Country: {
            const QSignalBlocker popupBlocker{m_popupCountryCombo};
            syncComboValue(m_popupCountryCombo, m_state.countryCode);
            updateFilterComboWidth(m_popupCountryCombo);
            if(m_countryCombo) {
                const QSignalBlocker pinnedBlocker{m_countryCombo};
                syncComboValue(m_countryCombo, m_state.countryCode);
                updateFilterComboWidth(m_countryCombo);
            }
        } break;
        case FilterControl::Language: {
            const QSignalBlocker popupBlocker{m_popupLanguageCombo};
            syncComboValue(m_popupLanguageCombo, m_state.language);
            updateFilterComboWidth(m_popupLanguageCombo);
            if(m_languageCombo) {
                const QSignalBlocker pinnedBlocker{m_languageCombo};
                syncComboValue(m_languageCombo, m_state.language);
                updateFilterComboWidth(m_languageCombo);
            }
        } break;
        case FilterControl::Genre: {
            const QSignalBlocker popupBlocker{m_popupGenreCombo};
            syncComboValue(m_popupGenreCombo, m_state.genreTag);
            updateFilterComboWidth(m_popupGenreCombo);
            if(m_genreCombo) {
                const QSignalBlocker pinnedBlocker{m_genreCombo};
                syncComboValue(m_genreCombo, m_state.genreTag);
                updateFilterComboWidth(m_genreCombo);
            }
        } break;
        case FilterControl::Tag: {
            const QSignalBlocker popupBlocker{m_popupTagCombo};
            ensureComboValue(m_popupTagCombo, tagName(m_state.tag), m_state.tag);
            syncComboValue(m_popupTagCombo, m_state.tag);
            updateFilterComboWidth(m_popupTagCombo);
            if(m_tagCombo) {
                const QSignalBlocker pinnedBlocker{m_tagCombo};
                ensureComboValue(m_tagCombo, tagName(m_state.tag), m_state.tag);
                syncComboValue(m_tagCombo, m_state.tag);
                updateFilterComboWidth(m_tagCombo);
            }
        } break;
        case FilterControl::Codec: {
            const QSignalBlocker popupBlocker{m_popupCodecCombo};
            syncComboValue(m_popupCodecCombo, m_state.codec);
            updateFilterComboWidth(m_popupCodecCombo);
            if(m_codecCombo) {
                const QSignalBlocker pinnedBlocker{m_codecCombo};
                syncComboValue(m_codecCombo, m_state.codec);
                updateFilterComboWidth(m_codecCombo);
            }
        } break;
        case FilterControl::Bitrate: {
            const QSignalBlocker minPopupBlocker{m_popupMinBitrate};
            const QSignalBlocker maxPopupBlocker{m_popupMaxBitrate};
            m_popupMinBitrate->setValue(m_state.minBitrate);
            m_popupMaxBitrate->setValue(m_state.maxBitrate);
            if(m_minBitrate) {
                const QSignalBlocker minPinnedBlocker{m_minBitrate};
                m_minBitrate->setValue(m_state.minBitrate);
            }
            if(m_maxBitrate) {
                const QSignalBlocker maxPinnedBlocker{m_maxBitrate};
                m_maxBitrate->setValue(m_state.maxBitrate);
            }
        } break;
        case FilterControl::Count:
            break;
    }
}

int RadioSearch::activeFilterCount() const
{
    int count{0};

    if(filterActive(FilterControl::Country)) {
        ++count;
    }
    if(filterActive(FilterControl::Language)) {
        ++count;
    }
    if(filterActive(FilterControl::Genre)) {
        ++count;
    }
    if(filterActive(FilterControl::Tag)) {
        ++count;
    }
    if(filterActive(FilterControl::Codec)) {
        ++count;
    }
    if(filterActive(FilterControl::Bitrate)) {
        ++count;
    }

    return count;
}

bool RadioSearch::filterActive(FilterControl control) const
{
    switch(control) {
        case FilterControl::Country:
            return !m_state.countryCode.trimmed().isEmpty();
        case FilterControl::Language:
            return !m_state.language.trimmed().isEmpty();
        case FilterControl::Genre:
            return !m_state.genreTag.trimmed().isEmpty();
        case FilterControl::Tag:
            return !m_state.tag.trimmed().isEmpty();
        case FilterControl::Codec:
            return !m_state.codec.trimmed().isEmpty();
        case FilterControl::Bitrate:
            return m_state.minBitrate > 0 || m_state.maxBitrate > 0;
        case FilterControl::Count:
            break;
    }

    return false;
}

void RadioSearch::updateFilterButton()
{
    const int count = activeFilterCount();
    if(count > 0) {
        m_filterButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        //: %1 is the number of active search filters
        m_filterButton->setText(m_showFilterButtonText ? tr("Filters (%1)").arg(count) : u"(%1)"_s.arg(count));
        m_filterButton->setToolTip(tr("%Ln active filter(s)", nullptr, count));
    }
    else {
        m_filterButton->setToolButtonStyle(m_showFilterButtonText ? Qt::ToolButtonTextBesideIcon
                                                                  : Qt::ToolButtonIconOnly);
        m_filterButton->setText(m_showFilterButtonText ? tr("Filters") : QString{});
        m_filterButton->setToolTip(tr("Show filters"));
    }

    updateFilterClearButtons();
}

void RadioSearch::updateFilterClearButtons()
{
    const QIcon clearIcon = Gui::iconFromTheme(Constants::Icons::Clear);
    for(size_t i{0}; i < filterIndex(FilterControl::Count); ++i) {
        const auto control = static_cast<FilterControl>(i);
        auto& group        = filterGroup(control);

        if(!group.clearButton) {
            continue;
        }

        const bool active = filterActive(control);
        // Keep the clear button disabled but 'visible' in the layout so it's space is not collapsed
        group.clearButton->setEnabled(active);
        group.clearButton->setIcon(active ? clearIcon : QIcon{});
        group.clearButton->setToolTip(active ? tr("Clear filter") : QString{});
    }
}

void RadioSearch::clearFilter(FilterControl control)
{
    auto state{m_state};

    switch(control) {
        case FilterControl::Country:
            state.countryCode.clear();
            break;
        case FilterControl::Language:
            state.language.clear();
            break;
        case FilterControl::Genre:
            state.genreTag.clear();
            break;
        case FilterControl::Tag:
            state.tag.clear();
            break;
        case FilterControl::Codec:
            state.codec.clear();
            break;
        case FilterControl::Bitrate:
            state.minBitrate = 0;
            state.maxBitrate = 0;
            break;
        case FilterControl::Count:
            break;
    }

    setState(state, true);
}

void RadioSearch::ensurePinnedControls(FilterControl control)
{
    if(!m_pinnedFiltersWidget || !m_pinnedFiltersLayout) {
        m_pinnedFiltersWidget = new QWidget(this);
        m_pinnedFiltersLayout = new QHBoxLayout(m_pinnedFiltersWidget);
        m_pinnedFiltersLayout->setContentsMargins({});
        m_mainLayout->insertWidget(1, m_pinnedFiltersWidget);
    }

    const auto copyComboItems = [](const QComboBox* source, QComboBox* target) {
        target->clear();
        for(int i{0}; i < source->count(); ++i) {
            target->addItem(source->itemIcon(i), source->itemText(i), source->itemData(i));
            target->setItemData(i, source->itemData(i, RetryCategoryRole), RetryCategoryRole);
        }
    };

    switch(control) {
        case FilterControl::Country: {
            if(m_countryCombo) {
                return;
            }

            m_countryCombo = new ExpandingComboBox(m_pinnedFiltersWidget);
            setupFilterCombo(m_countryCombo);

            if(!m_countryCategories.empty()) {
                setCategories(m_popupCountryCombo, m_countryCombo, tr("Any country"), m_countryCategories, control);
            }
            else {
                copyComboItems(m_popupCountryCombo, m_countryCombo);
            }
            syncComboValue(m_countryCombo, m_state.countryCode);
            updateFilterComboWidth(m_countryCombo);

            QObject::connect(m_countryCombo, &QComboBox::currentIndexChanged, this, [this]() {
                if(handleCategoryRetry(m_countryCombo, RadioCategoryType::Country)) {
                    return;
                }

                auto state{m_state};
                state.countryCode = m_countryCombo->currentData().toString();
                setState(state, true);
            });
            break;
        }
        case FilterControl::Language: {
            if(m_languageCombo) {
                return;
            }

            m_languageCombo = new ExpandingComboBox(m_pinnedFiltersWidget);
            setupFilterCombo(m_languageCombo);

            if(!m_languageCategories.empty()) {
                setCategories(m_popupLanguageCombo, m_languageCombo, tr("Any language"), m_languageCategories, control);
            }
            else {
                copyComboItems(m_popupLanguageCombo, m_languageCombo);
            }
            syncComboValue(m_languageCombo, m_state.language);
            updateFilterComboWidth(m_languageCombo);

            QObject::connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this]() {
                if(handleCategoryRetry(m_languageCombo, RadioCategoryType::Language)) {
                    return;
                }

                auto state{m_state};
                state.language = m_languageCombo->currentData().toString();
                setState(state, true);
            });
            break;
        }
        case FilterControl::Genre: {
            if(m_genreCombo) {
                return;
            }

            m_genreCombo = new ExpandingComboBox(m_pinnedFiltersWidget);
            setupFilterCombo(m_genreCombo);
            populateGenreCombo();
            syncComboValue(m_genreCombo, m_state.genreTag);
            updateFilterComboWidth(m_genreCombo);

            QObject::connect(m_genreCombo, &QComboBox::currentIndexChanged, this, [this]() {
                auto state{m_state};
                state.genreTag = m_genreCombo->currentData().toString();
                setState(state, true);
            });
            break;
        }
        case FilterControl::Tag: {
            if(m_tagCombo) {
                return;
            }

            m_tagCombo = new ExpandingComboBox(m_pinnedFiltersWidget);
            setupFilterCombo(m_tagCombo);
            populateTagCombo();
            syncComboValue(m_tagCombo, m_state.tag);
            updateFilterComboWidth(m_tagCombo);

            QObject::connect(m_tagCombo, &QComboBox::currentIndexChanged, this, [this]() {
                if(handleCategoryRetry(m_tagCombo, RadioCategoryType::Tag)) {
                    return;
                }

                auto state{m_state};
                state.tag = m_tagCombo->currentData().toString();
                setState(state, true);
            });
            break;
        }
        case FilterControl::Codec: {
            if(m_codecCombo) {
                return;
            }

            m_codecCombo = new ExpandingComboBox(m_pinnedFiltersWidget);
            setupFilterCombo(m_codecCombo);

            if(!m_codecCategories.empty()) {
                setCategories(m_popupCodecCombo, m_codecCombo, tr("Any codec"), m_codecCategories, control);
            }
            else {
                copyComboItems(m_popupCodecCombo, m_codecCombo);
            }
            syncComboValue(m_codecCombo, m_state.codec);
            updateFilterComboWidth(m_codecCombo);

            QObject::connect(m_codecCombo, &QComboBox::currentIndexChanged, this, [this]() {
                if(handleCategoryRetry(m_codecCombo, RadioCategoryType::Codec)) {
                    return;
                }

                auto state{m_state};
                state.codec = m_codecCombo->currentData().toString();
                setState(state, true);
            });
            break;
        }
        case FilterControl::Bitrate: {
            if(m_minBitrate && m_maxBitrate) {
                return;
            }

            m_minBitrate = new QSpinBox(m_pinnedFiltersWidget);
            m_minBitrate->setRange(0, 1000);
            m_minBitrate->setSingleStep(32);
            m_minBitrate->setSuffix(u" kbps"_s);
            m_minBitrate->setSpecialValueText(tr("Min bitrate"));
            m_minBitrate->setValue(m_state.minBitrate);

            m_maxBitrate = new QSpinBox(m_pinnedFiltersWidget);
            m_maxBitrate->setRange(0, 1000);
            m_maxBitrate->setSingleStep(32);
            m_maxBitrate->setSuffix(u" kbps"_s);
            m_maxBitrate->setSpecialValueText(tr("Max bitrate"));
            m_maxBitrate->setValue(m_state.maxBitrate);

            QObject::connect(m_minBitrate, &QSpinBox::valueChanged, this, [this](int value) {
                auto state{m_state};
                state.minBitrate = value;
                setState(state, true);
            });
            QObject::connect(m_maxBitrate, &QSpinBox::valueChanged, this, [this](int value) {
                auto state{m_state};
                state.maxBitrate = value;
                setState(state, true);
            });
            break;
        }
        case FilterControl::Count:
            break;
    }

    renderControlFromState(control);
}

void RadioSearch::showFilterPopup()
{
    renderControlsFromState();
    updateFilterPlacement();

    m_filterPopup->adjustSize();
    const QPoint popupPosition = m_filterButton->mapToGlobal(QPoint{0, m_filterButton->height()});
    m_filterPopup->move(popupPosition);
    m_filterPopup->show();
}

void RadioSearch::showFilterButtonContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabelAction = new QAction(tr("Show label"), menu);
    showLabelAction->setCheckable(true);
    showLabelAction->setChecked(m_showFilterButtonText);
    QObject::connect(showLabelAction, &QAction::triggered, this, [this](bool checked) {
        m_showFilterButtonText = checked;
        updateFilterButton();
    });

    menu->addAction(showLabelAction);
    menu->popup(m_filterButton->mapToGlobal(pos));
}

void RadioSearch::updateFilterPlacement()
{
    int pinnedCount{0};
    int popupRow{0};
    updateFilterClearButtons();

    const auto removePinnedControl = [this](FilterControl control) {
        switch(control) {
            case FilterControl::Country:
                if(m_countryCombo) {
                    m_pinnedFiltersLayout->removeWidget(m_countryCombo);
                }
                break;
            case FilterControl::Language:
                if(m_languageCombo) {
                    m_pinnedFiltersLayout->removeWidget(m_languageCombo);
                }
                break;
            case FilterControl::Genre:
                if(m_genreCombo) {
                    m_pinnedFiltersLayout->removeWidget(m_genreCombo);
                }
                break;
            case FilterControl::Tag:
                if(m_tagCombo) {
                    m_pinnedFiltersLayout->removeWidget(m_tagCombo);
                }
                break;
            case FilterControl::Codec:
                if(m_codecCombo) {
                    m_pinnedFiltersLayout->removeWidget(m_codecCombo);
                }
                break;
            case FilterControl::Bitrate:
                if(m_minBitrate) {
                    m_pinnedFiltersLayout->removeWidget(m_minBitrate);
                }
                if(m_maxBitrate) {
                    m_pinnedFiltersLayout->removeWidget(m_maxBitrate);
                }
                break;
            case FilterControl::Count:
                break;
        }
    };

    const auto deletePinnedControl = [this, &removePinnedControl](FilterControl control) {
        removePinnedControl(control);

        switch(control) {
            case FilterControl::Country:
                delete m_countryCombo;
                break;
            case FilterControl::Language:
                delete m_languageCombo;
                break;
            case FilterControl::Genre:
                delete m_genreCombo;
                break;
            case FilterControl::Tag:
                delete m_tagCombo;
                break;
            case FilterControl::Codec:
                delete m_codecCombo;
                break;
            case FilterControl::Bitrate:
                delete m_minBitrate;
                delete m_maxBitrate;
                break;
            case FilterControl::Count:
                break;
        }
    };

    const auto addPinnedControl = [this](FilterControl control) {
        switch(control) {
            case FilterControl::Country:
                m_pinnedFiltersLayout->addWidget(m_countryCombo);
                break;
            case FilterControl::Language:
                m_pinnedFiltersLayout->addWidget(m_languageCombo);
                break;
            case FilterControl::Genre:
                m_pinnedFiltersLayout->addWidget(m_genreCombo);
                break;
            case FilterControl::Tag:
                m_pinnedFiltersLayout->addWidget(m_tagCombo);
                break;
            case FilterControl::Codec:
                m_pinnedFiltersLayout->addWidget(m_codecCombo);
                break;
            case FilterControl::Bitrate:
                m_pinnedFiltersLayout->addWidget(m_minBitrate);
                m_pinnedFiltersLayout->addWidget(m_maxBitrate);
                break;
            case FilterControl::Count:
                break;
        }
    };

    for(size_t i{0}; i < filterIndex(FilterControl::Count); ++i) {
        const auto control = static_cast<FilterControl>(i);
        auto& group        = filterGroup(control);

        m_popupFiltersLayout->removeWidget(group.label);
        m_popupFiltersLayout->removeWidget(group.clearButton);
        m_popupFiltersLayout->removeWidget(group.pinButton);

        switch(control) {
            case FilterControl::Country:
                m_popupFiltersLayout->removeWidget(m_popupCountryCombo);
                break;
            case FilterControl::Language:
                m_popupFiltersLayout->removeWidget(m_popupLanguageCombo);
                break;
            case FilterControl::Genre:
                m_popupFiltersLayout->removeWidget(m_popupGenreCombo);
                break;
            case FilterControl::Tag:
                m_popupFiltersLayout->removeWidget(m_popupTagCombo);
                break;
            case FilterControl::Codec:
                m_popupFiltersLayout->removeWidget(m_popupCodecCombo);
                break;
            case FilterControl::Bitrate:
                m_popupFiltersLayout->removeWidget(m_popupBitrateWidget);
                break;
            case FilterControl::Count:
                break;
        }

        const bool pinned = filterPinned(control);
        {
            const QSignalBlocker blocker{group.pinButton};
            group.pinButton->setChecked(pinned);
        }

        const QString tooltip = pinned ? tr("Unpin filter") : tr("Pin filter to bar");
        group.pinButton->setToolTip(tooltip);
        updatePinButtonIcon(group.pinButton, pinned);

        if(pinned) {
            removePinnedControl(control);
            ensurePinnedControls(control);
            addPinnedControl(control);

            ++pinnedCount;
        }
        else {
            deletePinnedControl(control);
        }

        m_popupFiltersLayout->addWidget(group.label, popupRow, 0);

        switch(control) {
            case FilterControl::Country:
                m_popupFiltersLayout->addWidget(m_popupCountryCombo, popupRow, 1);
                break;
            case FilterControl::Language:
                m_popupFiltersLayout->addWidget(m_popupLanguageCombo, popupRow, 1);
                break;
            case FilterControl::Genre:
                m_popupFiltersLayout->addWidget(m_popupGenreCombo, popupRow, 1);
                break;
            case FilterControl::Tag:
                m_popupFiltersLayout->addWidget(m_popupTagCombo, popupRow, 1);
                break;
            case FilterControl::Codec:
                m_popupFiltersLayout->addWidget(m_popupCodecCombo, popupRow, 1);
                break;
            case FilterControl::Bitrate:
                m_popupFiltersLayout->addWidget(m_popupBitrateWidget, popupRow, 1);
                break;
            case FilterControl::Count:
                break;
        }

        m_popupFiltersLayout->addWidget(group.clearButton, popupRow, 2);
        m_popupFiltersLayout->addWidget(group.pinButton, popupRow, 3);
        ++popupRow;
    }

    if(m_pinnedFiltersWidget && pinnedCount <= 0) {
        m_pinnedFiltersWidget->deleteLater();
    }
}

void RadioSearch::setFilterPinned(FilterControl control, bool pinned)
{
    auto& group = filterGroup(control);
    if(std::exchange(group.pinned, pinned) == pinned) {
        return;
    }

    updateFilterPlacement();

    if(m_filterPopup->isVisible()) {
        m_filterPopup->adjustSize();
    }
}

bool RadioSearch::filterPinned(FilterControl control) const
{
    return filterGroup(control).pinned;
}

size_t RadioSearch::filterIndex(FilterControl control)
{
    return static_cast<size_t>(control);
}

RadioSearch::FilterGroup& RadioSearch::filterGroup(FilterControl control)
{
    return m_filterGroups.at(filterIndex(control));
}

const RadioSearch::FilterGroup& RadioSearch::filterGroup(FilterControl control) const
{
    return m_filterGroups.at(filterIndex(control));
}

QString RadioSearch::pinLayoutKey(FilterControl control)
{
    switch(control) {
        case FilterControl::Country:
            return u"CountryFilterPinned"_s;
        case FilterControl::Language:
            return u"LanguageFilterPinned"_s;
        case FilterControl::Genre:
            return u"GenreFilterPinned"_s;
        case FilterControl::Tag:
            return u"TagFilterPinned"_s;
        case FilterControl::Codec:
            return u"CodecFilterPinned"_s;
        case FilterControl::Bitrate:
            return u"BitrateFilterPinned"_s;
        case FilterControl::Count:
            break;
    }

    return {};
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiosearch.cpp"
