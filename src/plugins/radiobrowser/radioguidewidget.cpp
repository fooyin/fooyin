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

#include "radioguidewidget.h"

#include "radiobrowsercontroller.h"
#include "radioguidemodel.h"
#include "radioguideview.h"
#include "radiostationdialog.h"
#include "radiostationimportexportdialog.h"

#include <core/coresettings.h>
#include <gui/guisettings.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsmanager.h>

#include <QAbstractItemModel>
#include <QEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto GuideState   = "RadioBrowser/GuideState"_L1;
constexpr auto LastEntryKey = "LastEntry"_L1;

namespace Fooyin::RadioBrowser {
namespace {
QString loadingTextForCategory(const RadioCategoryType type)
{
    switch(type) {
        case RadioCategoryType::Country:
            return RadioGuideWidget::tr("Loading Countries…");
        case RadioCategoryType::Language:
            return RadioGuideWidget::tr("Loading Languages…");
        case RadioCategoryType::Tag:
            return RadioGuideWidget::tr("Loading Genres…");
        case RadioCategoryType::Codec:
            return RadioGuideWidget::tr("Loading Codecs…");
    }

    return RadioGuideWidget::tr("Loading…");
}

QString sectionExpansionKey(const QModelIndex& index)
{
    return index.data(RadioGuideModel::SectionKeyRole).toString();
}

void expandSections(QTreeView* view, const QAbstractItemModel* model, const QModelIndex& parent = {})
{
    for(int row{0}; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(index.data(RadioGuideModel::ItemKindRole).toInt() == static_cast<int>(ItemKind::Section)) {
            view->expand(index);
            expandSections(view, model, index);
        }
    }
}

QModelIndex findActionIndex(const QAbstractItemModel* model, const Action action, const QModelIndex& parent = {})
{
    for(int row{0}; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const auto kind         = static_cast<ItemKind>(index.data(RadioGuideModel::ItemKindRole).toInt());
        if(kind == ItemKind::Action && index.data(RadioGuideModel::ActionRole).toInt() == static_cast<int>(action)) {
            return index;
        }

        if(const QModelIndex childIndex = findActionIndex(model, action, index); childIndex.isValid()) {
            return childIndex;
        }
    }

    return {};
}

QString discoverEntryKey(const QModelIndex& index)
{
    if(!index.isValid()) {
        return {};
    }

    const auto kind = static_cast<ItemKind>(index.data(RadioGuideModel::ItemKindRole).toInt());

    switch(kind) {
        case ItemKind::Action:
            return u"action:%1"_s.arg(index.data(RadioGuideModel::ActionRole).toInt());
        case ItemKind::Category:
            return u"category:%1:%2"_s.arg(index.data(RadioGuideModel::CategoryTypeRole).toInt())
                .arg(index.data(RadioGuideModel::CategoryValueRole).toString());
        case ItemKind::SavedSearch:
            return u"saved:%1"_s.arg(index.data(RadioGuideModel::SavedSearchIdRole).toString());
        case ItemKind::Section:
            break;
    }

    return {};
}

QModelIndex findGuideEntryIndex(const QAbstractItemModel* model, const QString& key, const QModelIndex& parent = {})
{
    if(key.isEmpty()) {
        return {};
    }

    for(int row{0}; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(discoverEntryKey(index) == key) {
            return index;
        }

        if(const QModelIndex childIndex = findGuideEntryIndex(model, key, index); childIndex.isValid()) {
            return childIndex;
        }
    }

    return {};
}
} // namespace

RadioGuideWidget::RadioGuideWidget(RadioBrowserController* controller, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_controller{controller}
    , m_settings{settings}
    , m_treeView{new RadioGuideView(this)}
    , m_model{new RadioGuideModel(this)}
    , m_savedSearchCount{0}
    , m_categoryRequestActive{false}
    , m_savedSearchesLoaded{false}
    , m_showScrollbar{true}
{
    setObjectName(RadioGuideWidget::name());

    m_treeView->setModel(m_model);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setItemsExpandable(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(18);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setAlternatingRowColors(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_treeView);

    initialiseModel();

    QObject::connect(m_treeView, &QTreeView::clicked, this, &RadioGuideWidget::activateIndex);
    QObject::connect(m_treeView, &QTreeView::activated, this, &RadioGuideWidget::activateIndex);
    QObject::connect(m_treeView, &QWidget::customContextMenuRequested, this, &RadioGuideWidget::showContextMenu);
    QObject::connect(m_controller, &RadioBrowserController::categoriesStarted, this,
                     [this](RadioCategoryType type) { m_treeView->setStatusText(loadingTextForCategory(type)); });
    QObject::connect(
        m_controller, &RadioBrowserController::categoriesChanged, this,
        [this](const RadioCategoryType type, const RadioCategoryList& categories) { setCategories(type, categories); });
    QObject::connect(m_controller, &RadioBrowserController::categoriesFailed, this,
                     [this](RadioCategoryType, const QString& error) {
                         m_categoryRequestActive = false;
                         m_treeView->setStatusText(error);
                         requestNextCategory();
                     });
    QObject::connect(m_controller, &RadioBrowserController::savedSearchesChanged, this,
                     &RadioGuideWidget::setSavedSearches);
    QObject::connect(m_controller, &RadioBrowserController::latestSearchChanged, this,
                     &RadioGuideWidget::selectLatestSearch);

    const auto handleThemeChange = [this]() {
        refreshThemeIcons();
    };
    m_settings->subscribe<Settings::Gui::IconTheme>(this, handleThemeChange);
    m_settings->subscribe<Settings::Gui::Theme>(this, handleThemeChange);
    m_settings->subscribe<Settings::Gui::Style>(this, handleThemeChange);

    setSavedSearches(m_controller->savedSearches());
    requestCategories(RadioCategoryType::Country);
}

QString RadioGuideWidget::name() const
{
    return tr("Radio Guide");
}

QString RadioGuideWidget::layoutName() const
{
    return u"RadioGuide"_s;
}

void RadioGuideWidget::saveLayoutData(QJsonObject& layout)
{
    saveState();
    layout["CollapsedSections"_L1]
        = QJsonArray::fromStringList(Utils::saveCollapsedExpansionState(m_treeView, sectionExpansionKey));
    layout["ShowScrollbar"_L1] = m_showScrollbar;
}

void RadioGuideWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("ShowScrollbar"_L1)) {
        setScrollbarVisible(layout.value("ShowScrollbar"_L1).toBool());
    }

    if(layout.contains("CollapsedSections"_L1)) {
        QStringList collapsedSections;
        const QJsonArray sections = layout.value("CollapsedSections"_L1).toArray();
        for(const auto& section : sections) {
            collapsedSections.push_back(section.toString());
        }
        Utils::restoreCollapsedExpansionState(m_treeView, collapsedSections, sectionExpansionKey);
    }
}

void RadioGuideWidget::finalise()
{
    restoreState();
}

void RadioGuideWidget::changeEvent(QEvent* event)
{
    FyWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            refreshThemeIcons();
            break;
        default:
            break;
    }
}

void RadioGuideWidget::initialiseModel()
{
    expandSections(m_treeView, m_model);
    m_treeView->collapse(m_model->sectionForCategoryType(RadioCategoryType::Country));
}

void RadioGuideWidget::requestCategories(const RadioCategoryType type)
{
    if(!m_categoryRequests.contains(type)) {
        m_categoryRequests.enqueue(type);
    }
    requestNextCategory();
}

void RadioGuideWidget::requestNextCategory()
{
    if(m_categoryRequestActive || m_categoryRequests.empty()) {
        return;
    }

    m_categoryRequestActive = true;
    m_controller->fetchCategories(m_categoryRequests.dequeue());
}

void RadioGuideWidget::setCategories(const RadioCategoryType type, const RadioCategoryList& categories)
{
    if(!m_model->sectionForCategoryType(type).isValid()) {
        m_categoryRequestActive = false;
        requestNextCategory();
        return;
    }

    m_model->setCategories(type, categories);

    m_categoryRequestActive = false;
    m_treeView->clearStatusText();
    restoreLastEntry();
    requestNextCategory();
}

void RadioGuideWidget::setSavedSearches(const RadioSavedSearchList& searches)
{
    const bool searchAdded = m_savedSearchesLoaded && searches.size() > m_savedSearchCount;

    m_model->setSavedSearches(searches);
    m_savedSearchesLoaded = true;
    m_savedSearchCount    = searches.size();

    if(searchAdded) {
        expandSavedSearches();
    }

    restoreLastEntry();
}

void RadioGuideWidget::expandSavedSearches()
{
    const QModelIndex index = m_model->savedSearchesSection();
    if(!index.isValid()) {
        return;
    }

    m_treeView->expand(index.parent());
    m_treeView->expand(index);
    m_treeView->scrollTo(index);
}

void RadioGuideWidget::selectLatestSearch()
{
    const QModelIndex index = findActionIndex(m_model, Action::LatestSearch);
    if(!index.isValid()) {
        return;
    }

    m_treeView->expand(index.parent());
    m_treeView->setCurrentIndex(index);
    m_treeView->scrollTo(index);
}

void RadioGuideWidget::saveState() const
{
    QJsonObject state;
    state.insert(LastEntryKey, discoverEntryKey(m_treeView->currentIndex().siblingAtColumn(0)));

    FyStateSettings stateSettings;
    stateSettings.setValue(GuideState, QJsonDocument{state}.toJson(QJsonDocument::Compact));
}

void RadioGuideWidget::restoreState()
{
    const FyStateSettings stateSettings;
    const QByteArray state = stateSettings.value(GuideState).toByteArray();
    if(state.isEmpty()) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(state);
    if(!doc.isObject()) {
        return;
    }

    m_pendingRestoreEntryKey = doc.object().value(LastEntryKey).toString();
    restoreLastEntry();
}

bool RadioGuideWidget::restoreLastEntry()
{
    if(m_pendingRestoreEntryKey.isEmpty()) {
        return false;
    }

    const QModelIndex index = findGuideEntryIndex(m_model, m_pendingRestoreEntryKey);
    if(!index.isValid()) {
        return false;
    }

    m_pendingRestoreEntryKey.clear();
    m_treeView->setCurrentIndex(index);
    activateIndex(index);
    return true;
}

void RadioGuideWidget::activateCurrentIndex()
{
    activateIndex(m_treeView->currentIndex());
}

void RadioGuideWidget::activateIndex(const QModelIndex& index)
{
    if(!index.isValid()) {
        return;
    }

    const QModelIndex itemIndex = index.siblingAtColumn(0);
    const auto kind             = static_cast<ItemKind>(itemIndex.data(RadioGuideModel::ItemKindRole).toInt());

    if(kind == ItemKind::Section) {
        m_treeView->setExpanded(itemIndex, !m_treeView->isExpanded(itemIndex));
        return;
    }

    m_treeView->setCurrentIndex(itemIndex);
    saveState();

    if(kind == ItemKind::Action) {
        switch(static_cast<Action>(itemIndex.data(RadioGuideModel::ActionRole).toInt())) {
            case Action::NowListening:
                m_controller->fetchNowListening();
                break;
            case Action::Trending:
                m_controller->fetchRecentlyClicked();
                break;
            case Action::Popular:
                m_controller->fetchTopVoted();
                break;
            case Action::Newest:
                m_controller->fetchRecentlyChanged();
                break;
            case Action::Random:
                m_controller->fetchRandom();
                break;
            case Action::LatestSearch:
                if(const auto request = m_controller->latestSearchRequest()) {
                    m_controller->searchStations(*request);
                }
                break;
            case Action::MyStations:
                m_controller->browseSavedStations();
                break;
        }
        return;
    }

    if(kind == ItemKind::Category) {
        m_controller->browseCategory(m_model->categoryForIndex(itemIndex));
        return;
    }

    if(kind == ItemKind::SavedSearch) {
        const QString id = itemIndex.data(RadioGuideModel::SavedSearchIdRole).toString();
        for(const RadioSavedSearch& search : m_controller->savedSearches()) {
            if(search.id == id) {
                m_controller->activateSavedSearch(search);
                return;
            }
        }
    }
}

void RadioGuideWidget::showContextMenu(const QPoint& pos)
{
    const QModelIndex contextIndex = m_treeView->indexAt(pos);
    if(contextIndex.isValid()) {
        m_treeView->setCurrentIndex(contextIndex.siblingAtColumn(0));
    }

    const QModelIndex index  = m_treeView->currentIndex().siblingAtColumn(0);
    const auto kind          = static_cast<ItemKind>(index.data(RadioGuideModel::ItemKindRole).toInt());
    const bool isSavedSearch = kind == ItemKind::SavedSearch;
    const bool canBrowse     = kind == ItemKind::Action || kind == ItemKind::Category || isSavedSearch;

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* browseAction = menu->addAction(tr("Browse"));
    browseAction->setEnabled(canBrowse);
    QObject::connect(browseAction, &QAction::triggered, this, &RadioGuideWidget::activateCurrentIndex);
    menu->addSeparator();

    if(isSavedSearch) {
        const QString id   = index.data(RadioGuideModel::SavedSearchIdRole).toString();
        const QString name = index.data(RadioGuideModel::SavedSearchNameRole).toString();

        auto* renameAction = menu->addAction(tr("Rename saved search…"));
        QObject::connect(renameAction, &QAction::triggered, this, [this, id, name]() { renameSavedSearch(id, name); });

        auto* removeAction = menu->addAction(tr("Remove saved search"));
        QObject::connect(removeAction, &QAction::triggered, this, [this, id]() { removeSavedSearch(id); });

        menu->addSeparator();
    }

    const bool isMyStations
        = kind == ItemKind::Action
       && static_cast<Action>(index.data(RadioGuideModel::ActionRole).toInt()) == Action::MyStations;
    if(isMyStations) {
        auto* addCustomAction = menu->addAction(tr("Add custom station…"));
        QObject::connect(addCustomAction, &QAction::triggered, this, &RadioGuideWidget::addCustomStation);

        menu->addSeparator();

        auto* importAction = menu->addAction(tr("Import stations…"));
        QObject::connect(importAction, &QAction::triggered, this, &RadioGuideWidget::importSavedStations);

        auto* exportAction = menu->addAction(tr("Export stations…"));
        exportAction->setEnabled(!m_controller->savedStations().empty());
        QObject::connect(exportAction, &QAction::triggered, this, &RadioGuideWidget::exportSavedStations);

        menu->addSeparator();
    }

    addDisplayMenu(menu);

    menu->popup(m_treeView->viewport()->mapToGlobal(pos));
}

void RadioGuideWidget::addDisplayMenu(QMenu* menu)
{
    auto* displayMenu = new QMenu(tr("Display"), menu);

    auto* showScrollbar = new QAction(tr("Show scrollbar"), displayMenu);
    showScrollbar->setCheckable(true);
    showScrollbar->setChecked(m_showScrollbar);
    QObject::connect(showScrollbar, &QAction::triggered, this, &RadioGuideWidget::setScrollbarVisible);

    displayMenu->addAction(showScrollbar);
    menu->addMenu(displayMenu);
}

void RadioGuideWidget::setScrollbarVisible(const bool visible)
{
    m_showScrollbar = visible;
    m_treeView->setVerticalScrollBarPolicy(m_showScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void RadioGuideWidget::refreshThemeIcons()
{
    m_model->refreshIcons();
    m_treeView->viewport()->update();
}

void RadioGuideWidget::addCustomStation()
{
    RadioStationDialog dialog{m_controller, this};
    if(dialog.exec() == QDialog::Accepted) {
        m_controller->addLocalStation(dialog.station());
        m_controller->browseSavedStations();
    }
}

void RadioGuideWidget::importSavedStations()
{
    RadioStationImportExportDialog::importStations(m_controller, this);
}

void RadioGuideWidget::exportSavedStations()
{
    RadioStationImportExportDialog::exportStations(m_controller, this);
}

void RadioGuideWidget::renameSavedSearch(const QString& id, const QString& name)
{
    bool accepted{false};
    const QString newName = QInputDialog::getText(this, tr("Rename Saved Search"), tr("Name") + u":"_s,
                                                  QLineEdit::Normal, name, &accepted)
                                .trimmed();
    if(accepted && !newName.isEmpty()) {
        m_controller->renameSearch(id, newName);
    }
}

void RadioGuideWidget::removeSavedSearch(const QString& id)
{
    m_controller->removeSearch(id);
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioguidewidget.cpp"
