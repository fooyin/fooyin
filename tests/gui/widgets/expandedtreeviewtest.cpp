/*
 * Fooyin
 * Copyright © 2026, Von Diego <64450412+donutinit@users.noreply.github.com>
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

#include <gui/widgets/expandedtreeview.h>

#include <QAbstractListModel>
#include <QApplication>
#include <QDropEvent>
#include <QMimeData>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include <gtest/gtest.h>

#include <optional>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
constexpr auto RowsMimeType = "application/x-fooyin-test-rows";

class FileListModel : public QAbstractListModel
{
public:
    explicit FileListModel(bool exportFiles)
        : m_files{u"/music/first.flac"_s, u"/music/second.flac"_s, u"/music/third.flac"_s}
        , m_exportFiles{exportFiles}
    { }

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_files.size());
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override
    {
        if(role == Qt::DisplayRole) {
            return m_files.at(index.row());
        }
        return {};
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if(!index.isValid()) {
            return Qt::ItemIsDropEnabled;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    }

    [[nodiscard]] QStringList mimeTypes() const override
    {
        return {QString::fromLatin1(RowsMimeType)};
    }

    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override
    {
        auto* mimeData = new QMimeData();
        mimeData->setData(QString::fromLatin1(RowsMimeType), {});

        if(m_exportFiles) {
            QList<QUrl> urls;
            for(const QModelIndex& index : indexes) {
                urls.push_back(QUrl::fromLocalFile(m_files.at(index.row())));
            }
            mimeData->setUrls(urls);
        }

        return mimeData;
    }

    [[nodiscard]] Qt::DropActions supportedDragActions() const override
    {
        return Qt::MoveAction | Qt::CopyAction;
    }

    [[nodiscard]] Qt::DropActions supportedDropActions() const override
    {
        return Qt::MoveAction | Qt::CopyAction;
    }

    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int /*row*/, int /*column*/,
                      const QModelIndex& /*parent*/) override
    {
        if(!data->hasFormat(QString::fromLatin1(RowsMimeType))) {
            return false;
        }
        m_droppedAction = action;
        return true;
    }

    [[nodiscard]] std::optional<Qt::DropAction> droppedAction() const
    {
        return m_droppedAction;
    }

private:
    QStringList m_files;
    bool m_exportFiles;
    std::optional<Qt::DropAction> m_droppedAction;
};

// Stands in for a file manager: all it knows about the drag is what the drag offers
class DropTarget : public QWidget
{
public:
    DropTarget()
    {
        setAcceptDrops(true);
    }

    [[nodiscard]] bool dropped() const
    {
        return m_dropped;
    }

    [[nodiscard]] Qt::DropActions possibleActions() const
    {
        return m_possibleActions;
    }

    [[nodiscard]] Qt::DropAction proposedAction() const
    {
        return m_proposedAction;
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override
    {
        m_dropped         = true;
        m_possibleActions = event->possibleActions();
        m_proposedAction  = event->proposedAction();
        event->acceptProposedAction();
    }

private:
    bool m_dropped{false};
    Qt::DropActions m_possibleActions;
    Qt::DropAction m_proposedAction{Qt::IgnoreAction};
};

class ExpandedTreeViewDragTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // QSimpleDrag is the only drag implementation that can be driven by synthetic mouse events
        if(QGuiApplication::platformName() != "minimal"_L1) {
            GTEST_SKIP() << "Simulated drags need the minimal platform";
        }
    }

    static void showView(ExpandedTreeView& view, QAbstractItemModel* model)
    {
        view.setModel(model);
        view.setDragEnabled(true);
        view.setDragDropMode(QAbstractItemView::DragDrop);
        view.setDefaultDropAction(Qt::MoveAction);
        view.viewport()->setAcceptDrops(true);
        view.setGeometry(0, 0, 300, 200);
        view.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&view));
    }

    static void showTarget(DropTarget& target)
    {
        target.setGeometry(400, 0, 200, 200);
        target.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&target));
    }

    static QRect rowRect(const ExpandedTreeView& view, int row)
    {
        return view.visualRect(view.model()->index(row, 0));
    }

    // QDrag::exec() blocks in a nested event loop, so the second half of the gesture is queued
    // before the drag starts and runs inside that loop.
    static void dragRow(ExpandedTreeView& view, int row, QWidget* target, const QPoint& dropPos,
                        Qt::KeyboardModifiers dropModifiers = {})
    {
        QTimer gesture;
        gesture.setSingleShot(true);
        QObject::connect(&gesture, &QTimer::timeout, target, [target, dropPos, dropModifiers] {
            QTest::mouseMove(target, dropPos - QPoint{1, 0});
            QTest::mouseMove(target, dropPos);
            QTest::mouseRelease(target, Qt::LeftButton, dropModifiers, dropPos);
        });

        // Cancel the drag rather than hang if the release never reaches it
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, target, [target] { QTest::keyClick(target, Qt::Key_Escape); });

        gesture.start(0ms);
        watchdog.start(5s);

        const QPoint pressPos = rowRect(view, row).center();
        QTest::mousePress(view.viewport(), Qt::LeftButton, {}, pressPos);
        QTest::mouseMove(view.viewport(), pressPos + QPoint{1, 0});
        QTest::mouseMove(view.viewport(), pressPos + QPoint{QApplication::startDragDistance() + 2, 0});
    }
};

TEST_F(ExpandedTreeViewDragTest, FileDragOnlyOffersCopyToOtherApplications)
{
    FileListModel model{true};
    ExpandedTreeView view;
    showView(view, &model);

    DropTarget fileManager;
    showTarget(fileManager);

    dragRow(view, 0, &fileManager, {100, 100});

    ASSERT_TRUE(fileManager.dropped());
    EXPECT_FALSE(fileManager.possibleActions().testFlag(Qt::MoveAction));
    EXPECT_TRUE(fileManager.possibleActions().testFlag(Qt::CopyAction));
    EXPECT_EQ(Qt::CopyAction, fileManager.proposedAction());
}

TEST_F(ExpandedTreeViewDragTest, DragWithoutFilesStillOffersMove)
{
    FileListModel model{false};
    ExpandedTreeView view;
    showView(view, &model);

    DropTarget target;
    showTarget(target);

    dragRow(view, 0, &target, {100, 100});

    ASSERT_TRUE(target.dropped());
    EXPECT_TRUE(target.possibleActions().testFlag(Qt::MoveAction));
    EXPECT_EQ(Qt::MoveAction, target.proposedAction());
}

TEST_F(ExpandedTreeViewDragTest, FileDragMovesRowsWithinView)
{
    FileListModel model{true};
    ExpandedTreeView view;
    showView(view, &model);

    dragRow(view, 0, view.viewport(), rowRect(view, 2).topLeft() + QPoint{10, 1});

    ASSERT_TRUE(model.droppedAction().has_value());
    EXPECT_EQ(Qt::MoveAction, model.droppedAction().value());
}

TEST_F(ExpandedTreeViewDragTest, FileDragCopiesRowsWithinViewWithCtrl)
{
    FileListModel model{true};
    ExpandedTreeView view;
    showView(view, &model);

    dragRow(view, 0, view.viewport(), rowRect(view, 2).topLeft() + QPoint{10, 1}, Qt::ControlModifier);

    ASSERT_TRUE(model.droppedAction().has_value());
    EXPECT_EQ(Qt::CopyAction, model.droppedAction().value());
}

TEST_F(ExpandedTreeViewDragTest, FileDragMovesRowsInInternalMoveMode)
{
    FileListModel model{true};
    ExpandedTreeView view;
    showView(view, &model);
    view.setDragDropMode(QAbstractItemView::InternalMove);

    dragRow(view, 0, view.viewport(), rowRect(view, 2).topLeft() + QPoint{10, 1});

    ASSERT_TRUE(model.droppedAction().has_value());
    EXPECT_EQ(Qt::MoveAction, model.droppedAction().value());
}
} // namespace
} // namespace Fooyin::Testing

int main(int argc, char** argv)
{
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows");
#else
        qputenv("QT_QPA_PLATFORM", "minimal");
#endif
    }

    const QApplication app{argc, argv};
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
