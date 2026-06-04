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

#include "plugins/filters/filtercontroller.h"
#include "plugins/filters/filtermodel.h"
#include "plugins/filters/filterwidget.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/internalcoresettings.h>
#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <core/ratingsymbols.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/editablelayout.h>
#include <gui/guisettings.h>
#include <gui/internalguisettings.h>
#include <gui/layoutprovider.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
class EmptyMusicLibrary : public MusicLibrary
{
public:
    using MusicLibrary::MusicLibrary;

    [[nodiscard]] bool hasLibrary() const override
    {
        return false;
    }

    [[nodiscard]] std::optional<LibraryInfo> libraryInfo(int /*id*/) const override
    {
        return {};
    }

    [[nodiscard]] std::optional<LibraryInfo> libraryForPath(const QString& /*path*/) const override
    {
        return {};
    }

    void loadAllTracks() override { }
    [[nodiscard]] bool isEmpty() const override
    {
        return true;
    }

    void refreshAll() override { }
    void rescanAll() override { }

    ScanRequest refresh(const LibraryInfo& /*library*/) override
    {
        return {};
    }

    ScanRequest rescan(const LibraryInfo& /*library*/) override
    {
        return {};
    }

    void cancelScan(int /*id*/) override { }

    ScanRequest scanTracks(const TrackList& /*tracks*/) override
    {
        return {};
    }

    ScanRequest scanModifiedTracks(const TrackList& /*tracks*/) override
    {
        return {};
    }

    ScanRequest scanFiles(const QList<QUrl>& /*files*/) override
    {
        return {};
    }

    ScanRequest loadPlaylist(const QList<QUrl>& /*files*/) override
    {
        return {};
    }

    [[nodiscard]] TrackList tracks() const override
    {
        return {};
    }

    [[nodiscard]] TrackList libraryTracks() const override
    {
        return {};
    }

    [[nodiscard]] Track trackForId(int /*id*/) const override
    {
        return {};
    }

    [[nodiscard]] TrackList tracksForIds(const TrackIds& /*ids*/) const override
    {
        return {};
    }

    [[nodiscard]] std::shared_ptr<TrackMetadataStore> metadataStore() const override
    {
        return {};
    }

    void updateTrack(const Track& /*track*/) override { }
    void updateTracks(const TrackList& /*tracks*/) override { }
    void updateTrackMetadata(const TrackList& /*tracks*/) override { }

    WriteRequest writeTrackMetadata(const TrackList& /*tracks*/) override
    {
        return {};
    }

    WriteRequest writeTrackCovers(const TrackCoverData& /*coverData*/) override
    {
        return {};
    }

    void updateTrackStats(const TrackList& /*tracks*/) override { }
    void updateTrackStats(const Track& /*track*/) override { }

    WriteRequest removeUnavailbleTracks() override
    {
        return {};
    }

    WriteRequest deleteTracks(const TrackList& /*tracks*/) override
    {
        return {};
    }
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
    row.key      = key;
    row.columns  = columns;
    row.trackIds = trackIds;
    return row;
}

Filters::FilterColumnList simpleColumns()
{
    return {{.id = 0, .name = u"Name"_s, .field = u"%title%"_s}};
}

void registerMinimalGuiSettings(SettingsManager& settings)
{
    settings.createTempSetting<Settings::Gui::LayoutEditing>(false);
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

        m_mainWindow    = std::make_unique<QMainWindow>();
        m_actionManager = std::make_unique<ActionManager>(m_settings.get());
        m_actionManager->setMainWindow(m_mainWindow.get());

        m_editableLayout = std::make_unique<EditableLayout>(m_actionManager.get(), &m_widgetProvider, &m_layoutProvider,
                                                            m_settings.get());

        m_audioLoader     = std::make_shared<AudioLoader>();
        m_coverRepository = std::make_unique<CoverRepository>(m_audioLoader, m_settings.get());

        const CorePluginContext coreContext{
            nullptr, nullptr, nullptr, &m_library, nullptr, m_settings.get(), m_audioLoader, nullptr, nullptr, {},
        };

        m_controller = std::make_unique<Filters::FilterController>(m_actionManager.get(), coreContext, nullptr,
                                                                   m_editableLayout.get(), m_coverRepository.get(),
                                                                   m_settings.get());
    }

    Filters::FilterGroup defaultGroup() const
    {
        const auto group = m_controller->groupById(Id{"Default"});
        EXPECT_TRUE(group.has_value());
        return group.value_or(Filters::FilterGroup{});
    }

    QTemporaryDir m_settingsDir;
    EmptyMusicLibrary m_library;
    WidgetProvider m_widgetProvider;
    LayoutProvider m_layoutProvider;

    std::unique_ptr<SettingsManager> m_settings;
    std::unique_ptr<CoreSettings> m_coreSettings;
    std::unique_ptr<QMainWindow> m_mainWindow;
    std::unique_ptr<ActionManager> m_actionManager;
    std::unique_ptr<EditableLayout> m_editableLayout;
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

    {
        const auto group = defaultGroup();
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
        const auto group = defaultGroup();
        ASSERT_EQ(1, group.filters.size());
        EXPECT_EQ(first, group.filters.front());
    }

    m_controller->addFilterToGroup(second, Id{"Default"});

    EXPECT_FALSE(m_controller->haveUngroupedFilters());
    EXPECT_FALSE(m_controller->filterIsUngrouped(second->id()));
    EXPECT_TRUE(m_controller->ungroupedFilters().empty());

    const auto regrouped = defaultGroup();
    ASSERT_EQ(2, regrouped.filters.size());
    EXPECT_EQ(first, regrouped.filters.at(0));
    EXPECT_EQ(second, regrouped.filters.at(1));
    EXPECT_EQ(0, first->index());
    EXPECT_EQ(1, second->index());
}

TEST_F(FilterControllerTest, LoadingGroupedWidgetWithReplacementIdDoesNotDuplicateMembership)
{
    auto* first  = m_controller->createFilter();
    auto* second = m_controller->createFilter();

    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);

    QJsonObject layout = savedWidgetLayout(*first);
    layout["ID"_L1]    = u"Grouped.Restored.Filter"_s;

    first->loadLayout(layout);

    EXPECT_EQ(Id{u"Grouped.Restored.Filter"_s}, first->id());

    const auto group = defaultGroup();
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

    EXPECT_EQ(Id{u"Ungrouped.Restored.Filter"_s}, filter->id());
    EXPECT_TRUE(m_controller->haveUngroupedFilters());
    EXPECT_TRUE(m_controller->filterIsUngrouped(filter->id()));

    const auto ungrouped = m_controller->ungroupedFilters();
    ASSERT_EQ(1U, ungrouped.size());
    EXPECT_EQ(filter, ungrouped.at(filter->id()));

    EXPECT_TRUE(m_controller->filterGroups().empty());
}

TEST_F(FilterControllerTest, FilterModelSetRowsInsertsWithoutModelResetWhenColumnsUnchanged)
{
    CoverProvider coverProvider{m_audioLoader, m_settings.get()};
    Filters::FilterModel model(&m_library, &coverProvider, m_settings.get());

    const auto columns = simpleColumns();
    model.setRows(columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1})});
    model.setShowSummary(false);

    QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};

    model.setRows(
        columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}), makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2})});

    EXPECT_EQ(0, resetSpy.count());
    ASSERT_EQ(1, rowsInsertedSpy.count());
    EXPECT_EQ(1, rowsInsertedSpy.at(0).at(1).toInt());
    EXPECT_EQ(1, rowsInsertedSpy.at(0).at(2).toInt());
    EXPECT_EQ(0, rowsRemovedSpy.count());
    EXPECT_EQ(2, model.rowCount({}));
    EXPECT_EQ(keyFor(u"A"_s), model.index(0, 0, {}).data(Filters::FilterItem::Key).toByteArray());
    EXPECT_EQ(keyFor(u"B"_s), model.index(1, 0, {}).data(Filters::FilterItem::Key).toByteArray());
}

TEST_F(FilterControllerTest, FilterModelSetRowsRemovesWithoutModelResetWhenColumnsUnchanged)
{
    CoverProvider coverProvider{m_audioLoader, m_settings.get()};
    Filters::FilterModel model(&m_library, &coverProvider, m_settings.get());

    const auto columns = simpleColumns();
    model.setRows(
        columns, {makeDisplayRow(keyFor(u"A"_s), {u"Alpha"_s}, {1}), makeDisplayRow(keyFor(u"B"_s), {u"Beta"_s}, {2})});
    model.setShowSummary(false);

    QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};

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

    QSignalSpy resetSpy{&model, &QAbstractItemModel::modelReset};
    QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    QSignalSpy dataChangedSpy{&model, &QAbstractItemModel::dataChanged};

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
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
