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

#include "gui/filters/filtercontroller.h"
#include "gui/filters/filtermodel.h"
#include "gui/filters/filterwidget.h"
#include "gui/splitters/splitterwidget.h"
#include "gui/splitters/tabstackwidget.h"
#include "testutils.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/internalcoresettings.h>
#include <core/player/playercontroller.h>
#include <core/plugins/coreplugincontext.h>
#include <core/ratingsymbols.h>
#include <core/scripting/scriptcontextservice.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/guisettings.h>
#include <gui/guistyleprovider.h>
#include <gui/internalguisettings.h>
#include <gui/layout/editablelayout.h>
#include <gui/layout/layoutprovider.h>
#include <gui/playlist/currentplaylistcontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
class TestSplitterWidget : public SplitterWidget
{
public:
    using SplitterWidget::SplitterWidget;
    using WidgetContainer::setIndependentChildren;
};

class TestCurrentPlaylistController : public CurrentPlaylistController
{
public:
    using CurrentPlaylistController::CurrentPlaylistController;

    [[nodiscard]] Playlist* currentPlaylist() const override
    {
        return m_playlist;
    }

    [[nodiscard]] UId currentPlaylistId() const override
    {
        return m_playlist ? m_playlist->id() : UId{};
    }

    void changeCurrentPlaylist(const UId& /*id*/) override { }

    void selectTracks(const TrackList& tracks) override
    {
        m_selectedTracks = tracks;
    }

    void setCurrentPlaylist(Playlist* playlist)
    {
        Playlist* previous = std::exchange(m_playlist, playlist);
        Q_EMIT currentPlaylistChanged(previous, playlist);
    }

    [[nodiscard]] const TrackList& selectedTracks() const
    {
        return m_selectedTracks;
    }

private:
    Playlist* m_playlist{nullptr};
    TrackList m_selectedTracks;
};

QJsonObject savedWidgetLayout(Filters::FilterWidget& widget)
{
    QJsonArray layout;
    widget.saveLayout(layout);

    EXPECT_EQ(1, layout.size());
    const QJsonObject object = layout.at(0).toObject();
    EXPECT_TRUE(object.contains(widget.layoutName()));
    return object.value(widget.layoutName()).toObject();
}

Filters::RowKey keyFor(const QString& value)
{
    return value.toUtf8();
}

Filters::FilterRow makeDisplayRow(const Filters::RowKey& key, const QStringList& columns, const TrackIds& trackIds)
{
    Filters::FilterRow row;
    row.key     = key;
    row.columns = columns;
    for(const int trackId : trackIds) {
        Track track;
        track.setId(trackId);
        row.tracks.push_back(std::move(track));
    }
    return row;
}

Filters::FilterColumnList simpleColumns()
{
    return {{.id = 0, .name = u"Name"_s, .field = u"%title%"_s, .sortField = {}}};
}

void registerMinimalGuiSettings(SettingsManager& settings)
{
    ResolvedAppStyle style;
    style.revision = 1;

    settings.createTempSetting<Settings::Gui::LayoutEditing>(false);
    settings.createTempSetting<Settings::Gui::ResolvedAppStyle>(QVariant::fromValue(style));
    settings.createSetting<Settings::Gui::IconTheme>(0, u"Theme/IconTheme"_s);
    settings.createSetting<Settings::Gui::RatingFullStarSymbol>(defaultRatingFullStarSymbol(),
                                                                u"Interface/RatingFullStarSymbol"_s);
    settings.createSetting<Settings::Gui::RatingHalfStarSymbol>(defaultRatingHalfStarSymbol(),
                                                                u"Interface/RatingHalfStarSymbol"_s);
    settings.createSetting<Settings::Gui::RatingEmptyStarSymbol>(defaultRatingEmptyStarSymbol(),
                                                                 u"Interface/RatingEmptyStarSymbol"_s);
    settings.createSetting<Settings::Gui::Internal::EditableLayoutMargin>(-1, u"Interface/EditableLayoutMargin"_s);
    settings.createSetting<Settings::Gui::Internal::PixmapCacheSize>(64, u"Interface/PixmapCacheSize"_s);
    settings.createSetting<Settings::Gui::Internal::TrackCoverPaths>(QVariant::fromValue(CoverPaths{}),
                                                                     u"Artwork/Paths"_s);
    settings.createSetting<Settings::Gui::Internal::TrackCoverSourcePreference>(0, u"Artwork/LocalSourcePreference"_s);
    settings.createSetting<Settings::Gui::Internal::ArtistCoverFallbackToFront>(false,
                                                                                u"Artwork/ArtistFallbackToFront"_s);
}
} // namespace

class FilterControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NE(nullptr, qApp);

        ASSERT_TRUE(m_settingsDir.isValid());

        m_settings     = std::make_unique<SettingsManager>(m_settingsDir.filePath(u"settings.ini"_s));
        m_coreSettings = std::make_unique<CoreSettings>(m_settings.get());
        registerMinimalGuiSettings(*m_settings);

        m_player        = std::make_unique<PlayerController>(m_settings.get(), nullptr);
        m_scriptContext = std::make_unique<ScriptContextService>(m_player.get(), nullptr);

        m_mainWindow    = std::make_unique<QMainWindow>();
        m_actionManager = std::make_unique<ActionManager>(m_settings.get());
        m_actionManager->setMainWindow(m_mainWindow.get());

        m_editableLayout = std::make_unique<EditableLayout>(m_actionManager.get(), &m_widgetProvider, &m_layoutProvider,
                                                            m_settings.get());

        m_styleProvider = std::make_unique<GuiStyleProvider>(m_settings.get());

        m_audioLoader     = std::make_shared<AudioLoader>();
        m_coverRepository = std::make_unique<CoverRepository>(m_audioLoader, m_settings.get());

        const CorePluginContext coreContext{
            nullptr,       m_player.get(), nullptr, &m_library, nullptr, m_settings.get(),
            m_audioLoader, nullptr,        nullptr, nullptr,    nullptr, m_scriptContext.get(),
        };

        m_controller = std::make_unique<Filters::FilterController>(
            m_actionManager.get(), coreContext, &m_playlistController, nullptr, m_editableLayout.get(),
            m_coverRepository.get(), m_settings.get(), m_styleProvider.get());
    }

    void TearDown() override
    {
        std::set<Filters::FilterWidget*> widgets;
        for(const auto& group : m_controller->filterGroups() | std::views::values) {
            widgets.insert(group.filters.cbegin(), group.filters.cend());
        }
        for(auto* widget : m_controller->ungroupedFilters() | std::views::values) {
            widgets.emplace(widget);
        }
        for(auto* widget : widgets) {
            delete widget;
        }
    }

    [[nodiscard]] Filters::FilterGroup filterGroup(const Id& id) const
    {
        const auto group = m_controller->groupById(id);
        EXPECT_TRUE(group.has_value());
        return group.value_or(Filters::FilterGroup{});
    }

    void setLayoutRoot(FyWidget* widget)
    {
        auto* root = qobject_cast<WidgetContainer*>(m_editableLayout->root());
        ASSERT_NE(nullptr, root);
        root->insertWidget(0, widget);
    }

    Filters::FilterWidget* addFilter(WidgetContainer* container)
    {
        auto* filter = m_controller->createFilter();
        container->addWidget(filter);
        filter->finalise();
        return filter;
    }

    QTemporaryDir m_settingsDir;
    StubMusicLibrary m_library;
    TestCurrentPlaylistController m_playlistController;
    WidgetProvider m_widgetProvider;
    LayoutProvider m_layoutProvider;

    std::unique_ptr<SettingsManager> m_settings;
    std::unique_ptr<CoreSettings> m_coreSettings;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<ScriptContextService> m_scriptContext;
    std::unique_ptr<QMainWindow> m_mainWindow;
    std::unique_ptr<ActionManager> m_actionManager;
    std::unique_ptr<EditableLayout> m_editableLayout;
    std::unique_ptr<GuiStyleProvider> m_styleProvider;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<CoverRepository> m_coverRepository;
    std::unique_ptr<Filters::FilterController> m_controller;
};

TEST_F(FilterControllerTest, RegroupingMovesWidgetsBetweenGroupedAndUngroupedCollections)
{
    auto* first  = m_controller->createFilter();
    auto* second = m_controller->createFilter();

    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);

    const Id groupId{"TestGroup"};
    ASSERT_TRUE(m_controller->removeFilter(first));
    m_controller->addFilterToGroup(first, groupId);
    ASSERT_TRUE(m_controller->removeFilter(second));
    m_controller->addFilterToGroup(second, groupId);

    {
        const auto group = filterGroup(groupId);
        ASSERT_EQ(2, group.filters.size());
        EXPECT_EQ(first, group.filters.at(0));
        EXPECT_EQ(second, group.filters.at(1));
    }

    ASSERT_TRUE(m_controller->removeFilter(second));
    m_controller->addFilterToGroup(second, {});

    EXPECT_TRUE(m_controller->haveUngroupedFilters());
    EXPECT_TRUE(m_controller->filterIsUngrouped(second->id()));
    ASSERT_EQ(1U, m_controller->ungroupedFilters().size());
    EXPECT_EQ(second, m_controller->ungroupedFilters().at(second->id()));

    {
        const auto group = filterGroup(groupId);
        ASSERT_EQ(1, group.filters.size());
        EXPECT_EQ(first, group.filters.front());
    }

    ASSERT_TRUE(m_controller->removeFilter(second));
    m_controller->addFilterToGroup(second, groupId);

    EXPECT_FALSE(m_controller->haveUngroupedFilters());
    EXPECT_FALSE(m_controller->filterIsUngrouped(second->id()));
    EXPECT_TRUE(m_controller->ungroupedFilters().empty());

    const auto regrouped = filterGroup(groupId);
    ASSERT_EQ(2, regrouped.filters.size());
    EXPECT_EQ(first, regrouped.filters.at(0));
    EXPECT_EQ(second, regrouped.filters.at(1));
    EXPECT_EQ(0, first->index());
    EXPECT_EQ(1, second->index());
}

TEST_F(FilterControllerTest, IndependentChildScopesCanBeConfiguredPerContainer)
{
    TestSplitterWidget first{&m_widgetProvider, m_settings.get()};
    TestSplitterWidget second{&m_widgetProvider, m_settings.get()};
    const WidgetContainer& container = first;

    EXPECT_FALSE(container.hasIndependentChildren());
    first.setIndependentChildren(true);
    EXPECT_TRUE(container.hasIndependentChildren());
    EXPECT_FALSE(second.hasIndependentChildren());
    first.setIndependentChildren(false);
    EXPECT_FALSE(container.hasIndependentChildren());
}

TEST_F(FilterControllerTest, NewFilterChoosesGroupOnlyAfterInsertionAndFinalisation)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);

    auto* filter = m_controller->createFilter();
    EXPECT_FALSE(filter->group().isValid());
    EXPECT_TRUE(m_controller->filterIsUngrouped(filter->id()));

    page->addWidget(filter);
    EXPECT_FALSE(filter->group().isValid());
    filter->finalise();

    EXPECT_TRUE(filter->group().isValid());
    EXPECT_FALSE(m_controller->filterIsUngrouped(filter->id()));
    const auto group = filterGroup(filter->group());
    ASSERT_EQ(1, group.filters.size());
    EXPECT_EQ(filter, group.filters.front());
}

TEST_F(FilterControllerTest, NewFilterAppendsToSoleGroupWithoutTabs)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    const auto* first  = addFilter(page);
    const auto* second = addFilter(page);

    ASSERT_TRUE(first->group().isValid());
    EXPECT_EQ(first->group(), second->group());
    const auto group = filterGroup(first->group());
    ASSERT_EQ(2, group.filters.size());
    EXPECT_EQ(first, group.filters.at(0));
    EXPECT_EQ(second, group.filters.at(1));
    EXPECT_EQ(0, first->index());
    EXPECT_EQ(1, second->index());
}

TEST_F(FilterControllerTest, NewFilterSharesGroupAcrossNestedSplitters)
{
    auto* page     = new SplitterWidget(&m_widgetProvider, m_settings.get());
    auto* splitter = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    page->addWidget(splitter);

    const auto* first  = addFilter(splitter);
    const auto* second = addFilter(page);

    ASSERT_TRUE(first->group().isValid());
    EXPECT_EQ(first->group(), second->group());
}

TEST_F(FilterControllerTest, NewFiltersInDifferentTabsHaveIndependentGroups)
{
    auto* tabs = new TabStackWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(tabs);
    EXPECT_TRUE(tabs->hasIndependentChildren());

    const auto* first  = addFilter(tabs);
    const auto* second = addFilter(tabs);

    EXPECT_TRUE(first->group().isValid());
    EXPECT_TRUE(second->group().isValid());
    EXPECT_NE(first->group(), second->group());
}

TEST_F(FilterControllerTest, IndependentChildScopesAreRespectedForNonTabContainers)
{
    auto* container = new TestSplitterWidget(&m_widgetProvider, m_settings.get());
    container->setIndependentChildren(true);
    setLayoutRoot(container);

    const auto* first  = addFilter(container);
    const auto* second = addFilter(container);

    EXPECT_TRUE(first->group().isValid());
    EXPECT_TRUE(second->group().isValid());
    EXPECT_NE(first->group(), second->group());
}

TEST_F(FilterControllerTest, NestedTabGroupsDoNotAffectOuterPageGrouping)
{
    auto* page       = new SplitterWidget(&m_widgetProvider, m_settings.get());
    auto* tabs       = new TabStackWidget(&m_widgetProvider, m_settings.get());
    auto* nestedPage = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    page->addWidget(tabs);
    tabs->addWidget(nestedPage);

    const auto* outerFirst   = addFilter(page);
    const auto* nestedFirst  = addFilter(nestedPage);
    const auto* outerSecond  = addFilter(page);
    const auto* nestedSecond = addFilter(nestedPage);

    ASSERT_TRUE(outerFirst->group().isValid());
    ASSERT_TRUE(nestedFirst->group().isValid());
    EXPECT_NE(outerFirst->group(), nestedFirst->group());
    EXPECT_EQ(outerFirst->group(), outerSecond->group());
    EXPECT_EQ(nestedFirst->group(), nestedSecond->group());
}

TEST_F(FilterControllerTest, NewFilterRemainsUngroupedWhenScopeContainsMultipleGroups)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    const auto* first = addFilter(page);
    auto* second      = addFilter(page);
    ASSERT_TRUE(first->group().isValid());
    ASSERT_EQ(first->group(), second->group());
    ASSERT_TRUE(m_controller->removeFilter(second));
    m_controller->addFilterToGroup(second, Id{"OtherGroup"});

    auto* ambiguous = addFilter(page);
    EXPECT_FALSE(ambiguous->group().isValid());
    EXPECT_TRUE(m_controller->filterIsUngrouped(ambiguous->id()));
}

TEST_F(FilterControllerTest, NewFilterIgnoresDetachedGroupMembers)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    const auto* first = addFilter(page);
    auto* detached    = addFilter(page);
    ASSERT_TRUE(first->group().isValid());
    ASSERT_EQ(first->group(), detached->group());
    ASSERT_TRUE(m_controller->removeFilter(detached));
    m_controller->addFilterToGroup(detached, Id{"DetachedGroup"});

    ASSERT_EQ(detached, page->takeWidget(page->widgetIndex(detached->id())));

    const auto* appended = addFilter(page);
    EXPECT_EQ(first->group(), appended->group());
    EXPECT_EQ(Id{"DetachedGroup"}, detached->group());
}

TEST_F(FilterControllerTest, MovingFilterToAnotherTabPreservesItsGroup)
{
    auto* tabs       = new TabStackWidget(&m_widgetProvider, m_settings.get());
    auto* firstPage  = new SplitterWidget(&m_widgetProvider, m_settings.get());
    auto* secondPage = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(tabs);
    tabs->addWidget(firstPage);
    tabs->addWidget(secondPage);

    auto* moved        = addFilter(firstPage);
    const auto* second = addFilter(secondPage);
    const Id groupId   = moved->group();
    ASSERT_TRUE(groupId.isValid());
    ASSERT_TRUE(second->group().isValid());
    ASSERT_NE(groupId, second->group());

    ASSERT_EQ(moved, firstPage->takeWidget(firstPage->widgetIndex(moved->id())));
    secondPage->addWidget(moved);
    moved->finalise();

    EXPECT_EQ(groupId, moved->group());
    EXPECT_NE(second->group(), moved->group());
}

TEST_F(FilterControllerTest, FinalisationPreservesExplicitlyUngroupedFilters)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    ASSERT_TRUE(addFilter(page)->group().isValid());
    auto* ungrouped = m_controller->createFilter();
    ASSERT_TRUE(m_controller->removeFilter(ungrouped));
    m_controller->addFilterToGroup(ungrouped, {});
    page->addWidget(ungrouped);
    ungrouped->finalise();

    EXPECT_FALSE(ungrouped->group().isValid());
    EXPECT_TRUE(m_controller->filterIsUngrouped(ungrouped->id()));
}

TEST_F(FilterControllerTest, RepeatedFinalisationDoesNotRegroupAmbiguousFilters)
{
    auto* page = new SplitterWidget(&m_widgetProvider, m_settings.get());
    setLayoutRoot(page);
    const auto* first = addFilter(page);
    auto* second      = addFilter(page);
    ASSERT_TRUE(first->group().isValid());
    ASSERT_EQ(first->group(), second->group());
    ASSERT_TRUE(m_controller->removeFilter(second));
    m_controller->addFilterToGroup(second, Id{"OtherGroup"});
    auto* ambiguous = addFilter(page);
    ASSERT_FALSE(ambiguous->group().isValid());

    ASSERT_EQ(second, page->takeWidget(page->widgetIndex(second->id())));
    second->finalise();
    ambiguous->finalise();

    EXPECT_EQ(Id{"OtherGroup"}, second->group());
    EXPECT_FALSE(ambiguous->group().isValid());
    EXPECT_TRUE(m_controller->filterIsUngrouped(ambiguous->id()));
    EXPECT_EQ(first->group(), addFilter(page)->group());
}

TEST_F(FilterControllerTest, LoadingGroupedWidgetWithReplacementIdDoesNotDuplicateMembership)
{
    auto* first  = m_controller->createFilter();
    auto* second = m_controller->createFilter();

    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);

    first->loadLayoutData({{"Group"_L1, u"Default"_s}});
    second->loadLayoutData({{"Group"_L1, u"Default"_s}});

    QJsonObject layout = savedWidgetLayout(*first);
    layout["ID"_L1]    = u"Grouped.Restored.Filter"_s;

    first->loadLayout(layout);
    first->finalise();
    second->finalise();

    EXPECT_EQ(Id{u"Grouped.Restored.Filter"_s}, first->id());

    const auto group = filterGroup(Id{"Default"});
    ASSERT_EQ(2, group.filters.size());
    EXPECT_EQ(first, group.filters.at(0));
    EXPECT_EQ(second, group.filters.at(1));
}

TEST_F(FilterControllerTest, LoadingUngroupedWidgetWithReplacementIdPreservesUngroupedTracking)
{
    auto* filter = m_controller->createFilter();

    ASSERT_NE(nullptr, filter);
    ASSERT_TRUE(m_controller->removeFilter(filter));
    m_controller->addFilterToGroup(filter, {});

    QJsonObject layout = savedWidgetLayout(*filter);
    layout["ID"_L1]    = u"Ungrouped.Restored.Filter"_s;

    filter->loadLayout(layout);
    filter->finalise();

    EXPECT_EQ(Id{u"Ungrouped.Restored.Filter"_s}, filter->id());
    EXPECT_TRUE(m_controller->haveUngroupedFilters());
    EXPECT_TRUE(m_controller->filterIsUngrouped(filter->id()));

    const auto ungrouped = m_controller->ungroupedFilters();
    ASSERT_EQ(1U, ungrouped.size());
    EXPECT_EQ(filter, ungrouped.at(filter->id()));

    EXPECT_TRUE(m_controller->filterGroups().empty());
}

TEST_F(FilterControllerTest, SelectingFilterRowDoesNotRepublishSourceModel)
{
    Track track{u"/music/one.flac"_s};
    track.setId(1);
    track.setLibraryId(1);
    track.setGenres({u"Rock"_s});
    m_library.setLibraryTracks({track});

    auto* filter = m_controller->createFilter();
    ASSERT_NE(nullptr, filter);

    auto config            = filter->currentConfig();
    config.playlistEnabled = false;

    auto* model = filter->findChild<Filters::FilterModel*>();
    auto* view  = filter->findChild<QAbstractItemView*>();
    ASSERT_NE(nullptr, model);
    ASSERT_NE(nullptr, view);

    QSignalSpy resetSpy{model, &QAbstractItemModel::modelReset};
    filter->applyConfig(config);
    ASSERT_TRUE(resetSpy.wait(1000));
    ASSERT_EQ(2, model->rowCount({}));

    const QSignalSpy dataChangedSpy{model, &QAbstractItemModel::dataChanged};
    view->selectionModel()->select(view->model()->index(1, 0),
                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    EXPECT_EQ(0, dataChangedSpy.count());
}

TEST_F(FilterControllerTest, FilterModelSetRowsInsertsWithoutModelResetWhenColumnsUnchanged)
{
    CoverProvider coverProvider{m_audioLoader, m_settings.get()};
    Filters::FilterModel model(&m_library, &coverProvider, m_settings.get());

    const auto columns = simpleColumns();
    model.setRows(
        columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}), makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2})});
    model.setShowSummary(false);

    const QPersistentModelIndex firstIndex{model.index(0, 0, {})};
    const QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    const QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    const QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    const QSignalSpy rowsMovedSpy{&model, &QAbstractItemModel::rowsMoved};

    EXPECT_TRUE(model.setRows(columns, {makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2}),
                                        makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}),
                                        makeDisplayRow(keyFor(u"C"_s), {u"Gamma"_s}, {3})}));

    EXPECT_EQ(0, resetSpy.count());
    ASSERT_EQ(1, rowsInsertedSpy.count());
    EXPECT_EQ(2, rowsInsertedSpy.at(0).at(1).toInt());
    EXPECT_EQ(2, rowsInsertedSpy.at(0).at(2).toInt());
    EXPECT_EQ(0, rowsRemovedSpy.count());
    EXPECT_EQ(1, rowsMovedSpy.count());
    EXPECT_EQ(3, model.rowCount({}));
    EXPECT_EQ(keyFor(u"B"_s), model.index(0, 0, {}).data(Filters::FilterItem::Key).toByteArray());
    EXPECT_EQ(keyFor(u"A"_s), model.index(1, 0, {}).data(Filters::FilterItem::Key).toByteArray());
    EXPECT_EQ(keyFor(u"C"_s), model.index(2, 0, {}).data(Filters::FilterItem::Key).toByteArray());
    EXPECT_EQ(1, firstIndex.row());
    EXPECT_EQ(keyFor(u"A"_s), firstIndex.data(Filters::FilterItem::Key).toByteArray());

    EXPECT_TRUE(model.setRows(columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}),
                                        makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2}),
                                        makeDisplayRow(keyFor(u"C"_s), {u"Gamma"_s}, {3})}));
    EXPECT_EQ(0, resetSpy.count());
    EXPECT_EQ(2, rowsMovedSpy.count());
    EXPECT_EQ(0, firstIndex.row());
    EXPECT_EQ(keyFor(u"A"_s), firstIndex.data(Filters::FilterItem::Key).toByteArray());
}

TEST_F(FilterControllerTest, FilterModelSetRowsRemovesWithoutModelResetWhenColumnsUnchanged)
{
    CoverProvider coverProvider{m_audioLoader, m_settings.get()};
    Filters::FilterModel model(&m_library, &coverProvider, m_settings.get());

    const auto columns = simpleColumns();
    model.setRows(
        columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}), makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2})});
    model.setShowSummary(false);

    const QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    const QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    const QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};

    model.setRows(columns, {makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2})});

    EXPECT_EQ(0, resetSpy.count());
    ASSERT_EQ(1, rowsRemovedSpy.count());
    EXPECT_EQ(0, rowsRemovedSpy.at(0).at(1).toInt());
    EXPECT_EQ(0, rowsRemovedSpy.at(0).at(2).toInt());
    EXPECT_EQ(0, rowsInsertedSpy.count());
    EXPECT_EQ(1, model.rowCount({}));
    EXPECT_EQ(keyFor(u"B"_s), model.index(0, 0, {}).data(Filters::FilterItem::Key).toByteArray());
}

TEST_F(FilterControllerTest, FilterModelSetRowsUpdatesExistingRowWithoutModelReset)
{
    CoverProvider coverProvider{m_audioLoader, m_settings.get()};
    Filters::FilterModel model(&m_library, &coverProvider, m_settings.get());

    const auto columns = simpleColumns();
    model.setRows(columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1})});
    model.setShowSummary(false);

    const QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    const QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    const QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    const QSignalSpy dataChangedSpy{&model, &QAbstractItemModel::dataChanged};

    model.setRows(columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1, 2})});

    EXPECT_EQ(0, resetSpy.count());
    EXPECT_EQ(0, rowsInsertedSpy.count());
    EXPECT_EQ(0, rowsRemovedSpy.count());
    ASSERT_EQ(1, dataChangedSpy.count());
    EXPECT_EQ(TrackIds({1, 2}), model.index(0, 0, {}).data(Filters::FilterItem::TrackIdsRole).value<TrackIds>());
}
} // namespace Fooyin::Testing

int main(int argc, char** argv)
{
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows");
#else
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    }

    const QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
